package main

import (
	"log"
	"os"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/config"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/handlers"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Invalid configuration: %v", err)
	}

	if err := os.MkdirAll("./data", 0755); err != nil {
		log.Fatalf("Failed to create data directory: %v", err)
	}

	db, err := gorm.Open(sqlite.Open(cfg.DBPath), &gorm.Config{})
	if err != nil {
		log.Fatalf("Failed to connect database: %v", err)
	}

	if err := models.AutoMigrate(db); err != nil {
		log.Fatalf("Failed to migrate database: %v", err)
	}

	authHandler := handlers.NewAuthHandler(db)
	authHandler.InitAdmin()

	wsHub := websocket.NewHub()
	go wsHub.Run()

	// Agent 注册表
	registry := agent.NewRegistry(wsHub)
	go registry.StartHeartbeatCheck()

	// TCP Frame Listener（接受 runtime outbound 连接）
	// Note: cfg.AgentAddr is the listening address for runtime connections,
	// not a target address for dialing (deprecated old approach).
	frameListener := agentPkg.NewFrameListener(registry, wsHub)
	agentListenAddr := cfg.AgentAddr
	go func() {
		if err := frameListener.Start(agentListenAddr); err != nil {
			log.Printf("[FrameListener] Failed to start: %v (runtime outbound connection disabled)", err)
		}
	}()

	// 脚本输出持久化回调
	frameListener.SetScriptOutputHandler(func(agentID string, data map[string]any) {
		scriptID, _ := data["scriptId"].(string)
		message, _ := data["message"].(string)
		level, _ := data["level"].(string)
		if level == "" {
			level = "info"
		}
		db.Create(&models.ExecutionLog{
			ScriptID: scriptID,
			Output:   message,
			Level:    level,
		})
	})

	// 工作流引擎
	wfEngine := workflow.NewEngine(db, registry, wsHub, cfg.ScriptsDir)

	// Settings handler (DB version for persistence)
	settingsHandler := handlers.NewSettingsHandler(db)

	r := gin.Default()
	r.Use(middleware.CORS())

	r.Static("/assets", cfg.StaticDir+"/assets")
	r.StaticFile("/favicon.ico", cfg.StaticDir+"/favicon.ico")
	r.GET("/", func(c *gin.Context) {
		c.File(cfg.StaticDir + "/index.html")
	})

	// ====== API v1 路由（保留兼容） ======
	v1 := r.Group("/api/v1")
	{
		// Login endpoint with rate limiting to prevent brute force attacks
		v1.POST("/auth/login", middleware.RateLimitMiddleware(middleware.GetRateLimiter()), authHandler.HandleLogin)
		v1.POST("/auth/logout", authHandler.HandleLogout)

		auth := v1.Group("")
		auth.Use(middleware.AuthRequired())
		{
			statusHandler := handlers.NewStatusHandler(db, registry)
			profileHandler := handlers.NewProfileHandler(db)
			auth.GET("/status", statusHandler.HandleStatus)
			auth.GET("/health", statusHandler.HandleHealth)
			auth.GET("/profile", profileHandler.HandleGetProfile)
			auth.GET("/profile/games", profileHandler.HandleGetGames)
			auth.GET("/profile/permissions", profileHandler.HandleGetPermissions)
			auth.PUT("/profile", profileHandler.HandleUpdateProfile)
			auth.PUT("/profile/password", profileHandler.HandleUpdatePassword)

			// 只读接口 - 所有登录用户可访问
			windowHandler := handlers.NewWindowHandler(registry)
			auth.GET("/windows", windowHandler.HandleList)
			auth.GET("/settings", settingsHandler.HandleGetSettings)
		}

		// 写入接口 - 需要 admin 权限
		admin := v1.Group("")
		admin.Use(middleware.AuthRequired(), middleware.RoleRequired("admin"))
		{
			screenshotHandler := handlers.NewScreenshotHandler(wsHub)
			admin.POST("/screenshot", screenshotHandler.HandleScreenshot)

			scriptHandler := handlers.NewScriptHandler(db, cfg.ScriptsDir, registry)
			admin.GET("/scripts", scriptHandler.HandleList)      // 列表保留为只读
			admin.POST("/scripts", scriptHandler.HandleCreate)   // 创建脚本
			admin.DELETE("/scripts", scriptHandler.HandleDelete) // 删除脚本
			admin.POST("/scripts/content", scriptHandler.HandleGetContent)
			admin.POST("/scripts/save", scriptHandler.HandleSave) // 保存脚本
			admin.POST("/scripts/run", scriptHandler.HandleRun)   // 运行脚本
			admin.POST("/scripts/stop", scriptHandler.HandleStop) // 停止脚本
			admin.POST("/scripts/logs", scriptHandler.HandleLogs)

			admin.PUT("/settings", settingsHandler.HandleUpdateSettings)
		}
	}

	// ====== Dashboard 兼容路由（无 v1 前缀，匹配前端 wingman.ts） ======
	api := r.Group("/api")
	api.Use(middleware.AuthRequired())
	{
		// Agent 管理 - 只读接口
		agentHandler := handlers.NewAgentHandler(registry, db)
		api.GET("/agents", agentHandler.HandleList)
		api.GET("/agents/:agentId", agentHandler.HandleGet)

		// 工作流管理 - 只读接口
		wfHandler := handlers.NewWorkflowHandler(wfEngine, db)
		api.GET("/workflows", wfHandler.HandleList)
		api.GET("/workflows/:id", wfHandler.HandleGet)
		api.GET("/workflows/:id/workers", wfHandler.HandleGetWorkers)
		api.GET("/workflows/:id/steps/:stepId/status", wfHandler.HandleGetStepStatus)

		// 脚本管理 - 只读接口
		scriptHandlerAPI := handlers.NewScriptHandler(db, cfg.ScriptsDir, registry)
		api.GET("/scripts", scriptHandlerAPI.HandleList)
		api.POST("/scripts/content", scriptHandlerAPI.HandleGetContent) // 获取内容

		// 写入接口 - 需要 admin 权限
		apiAdmin := api.Group("")
		apiAdmin.Use(middleware.RoleRequired("admin"))
		{
			apiAdmin.POST("/agents/:agentId/shutdown", agentHandler.HandleShutdown)

			apiAdmin.POST("/workflows", wfHandler.HandleCreate)
			apiAdmin.POST("/workflows/:id/cancel", wfHandler.HandleCancel)

			apiAdmin.POST("/scripts", scriptHandlerAPI.HandleCreate)
			apiAdmin.POST("/scripts/delete", scriptHandlerAPI.HandleDelete)
			apiAdmin.DELETE("/scripts", scriptHandlerAPI.HandleDelete)
			apiAdmin.POST("/scripts/save", scriptHandlerAPI.HandleSave)
			apiAdmin.POST("/scripts/run", scriptHandlerAPI.HandleRun)
			apiAdmin.POST("/scripts/stop", scriptHandlerAPI.HandleStop)
			apiAdmin.POST("/scripts/logs", scriptHandlerAPI.HandleLogs)
		}
	}

	// ====== Debugger 路由（仅 admin） ======
	debugger := r.Group("/api/debugger")
	debugger.Use(middleware.AuthRequired(), middleware.RoleRequired("admin"))
	{
		debugger.POST("/connect", handlers.HandleDebuggerConnect)
		debugger.POST("/command", handlers.HandleDebuggerCommand)
		debugger.GET("/breakpoints", handlers.HandleDebuggerGetBreakpoints)
		debugger.POST("/breakpoints", handlers.HandleDebuggerSetBreakpoints)
	}

	apiAudit := r.Group("/api")
	apiAudit.Use(middleware.AuthRequired())
	{
		auditHandler := handlers.NewAuditHandler(db)
		apiAudit.GET("/audit", auditHandler.HandleList)
	}

	r.GET("/ws", func(c *gin.Context) {
		websocket.HandleWebSocket(c, wsHub)
	})

	addr := config.Addr(cfg)
	log.Printf("Server starting on http://%s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
