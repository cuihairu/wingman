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
	wfEngine := workflow.NewEngine(db, registry, wsHub)

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
		v1.POST("/auth/login", authHandler.HandleLogin)
		v1.POST("/auth/logout", authHandler.HandleLogout)

		auth := v1.Group("")
		auth.Use(middleware.AuthRequired())
		{
			statusHandler := handlers.NewStatusHandler(db, registry)
			auth.GET("/status", statusHandler.HandleStatus)
			auth.GET("/health", statusHandler.HandleHealth)

			scriptHandler := handlers.NewScriptHandler(db, cfg.ScriptsDir, registry)
			auth.GET("/scripts", scriptHandler.HandleList)
			auth.POST("/scripts", scriptHandler.HandleCreate)
			auth.DELETE("/scripts", scriptHandler.HandleDelete)
			auth.POST("/scripts/content", scriptHandler.HandleGetContent)
			auth.POST("/scripts/save", scriptHandler.HandleSave)
			auth.POST("/scripts/run", scriptHandler.HandleRun)
			auth.POST("/scripts/stop", scriptHandler.HandleStop)
			auth.POST("/scripts/logs", scriptHandler.HandleLogs)

			windowHandler := handlers.NewWindowHandler(registry)
			auth.GET("/windows", windowHandler.HandleList)

			auth.GET("/settings", handlers.HandleGetSettings)
			auth.PUT("/settings", handlers.HandleUpdateSettings)

			screenshotHandler := handlers.NewScreenshotHandler(wsHub)
			auth.POST("/screenshot", screenshotHandler.HandleScreenshot)
		}
	}

	// ====== Dashboard 兼容路由（无 v1 前缀，匹配前端 wingman.ts） ======
	api := r.Group("/api")
	api.Use(middleware.AuthRequired())
	{
		// Agent 管理
		agentHandler := handlers.NewAgentHandler(registry, db)
		api.GET("/agents", agentHandler.HandleList)
		api.GET("/agents/:agentId", agentHandler.HandleGet)
		api.POST("/agents/:agentId/shutdown", agentHandler.HandleShutdown)

		// 工作流管理
		wfHandler := handlers.NewWorkflowHandler(wfEngine, db)
		api.GET("/workflows", wfHandler.HandleList)
		api.GET("/workflows/:id", wfHandler.HandleGet)
		api.POST("/workflows", wfHandler.HandleCreate)
		api.POST("/workflows/:id/cancel", wfHandler.HandleCancel)
		api.GET("/workflows/:id/workers", wfHandler.HandleGetWorkers)
		api.GET("/workflows/:id/steps/:stepId/status", wfHandler.HandleGetStepStatus)

		// 脚本管理（复用 ScriptHandler）
		scriptHandlerAPI := handlers.NewScriptHandler(db, cfg.ScriptsDir, registry)
		api.GET("/scripts", scriptHandlerAPI.HandleList)
		api.POST("/scripts", scriptHandlerAPI.HandleCreate)
		api.DELETE("/scripts", scriptHandlerAPI.HandleDelete)
		api.POST("/scripts/content", scriptHandlerAPI.HandleGetContent)
		api.POST("/scripts/save", scriptHandlerAPI.HandleSave)
		api.POST("/scripts/run", scriptHandlerAPI.HandleRun)
		api.POST("/scripts/stop", scriptHandlerAPI.HandleStop)
		api.POST("/scripts/logs", scriptHandlerAPI.HandleLogs)
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

	r.GET("/ws", func(c *gin.Context) {
		websocket.HandleWebSocket(c, wsHub)
	})

	addr := config.Addr(cfg)
	log.Printf("Server starting on http://%s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
