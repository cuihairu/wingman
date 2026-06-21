package main

import (
	"log"
	"os"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/config"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/handlers"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
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

	// 内置 RBAC 角色/权限种子
	if err := rbac.Seed(db); err != nil {
		log.Printf("Failed to seed RBAC: %v", err)
	}

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

			// 注：messages/feedback 仅在下方 /api 组注册（匹配前端 wingman.ts 使用的路径），
			// 此处不再重复注册 /api/v1/messages 与 /api/v1/feedback，避免两套可达路径。

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
		messageHandler := handlers.NewMessageHandler(db)
		feedbackHandler := handlers.NewFeedbackHandler(db)
		api.GET("/messages", messageHandler.HandleList)
		api.GET("/messages/unread-count", messageHandler.HandleUnreadCount)
		api.POST("/messages/:id/read", messageHandler.HandleMarkRead)
		api.POST("/messages/read-all", messageHandler.HandleMarkAllRead)
		api.POST("/feedback", feedbackHandler.HandleCreate)

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
		api.GET("/workflow-templates", wfHandler.HandleListTemplates)

		// 脚本管理 - 只读接口
		scriptHandlerAPI := handlers.NewScriptHandler(db, cfg.ScriptsDir, registry)
		api.GET("/scripts", scriptHandlerAPI.HandleList)
		api.POST("/scripts/content", scriptHandlerAPI.HandleGetContent) // 获取内容

		// 写入接口 - 细粒度权限（PermissionRequired，admin 自动放行）。
		// 取代原先粗粒度 RoleRequired("admin")，使 operator/viewer 等自定义角色
		// 可按权限码（agents:manage / workflows:run / scripts:edit / scripts:run）获得受限访问。
		// agents:manage
		agentsMgmt := api.Group("")
		agentsMgmt.Use(middleware.PermissionRequired(db, "agents:manage"))
		{
			agentsMgmt.POST("/agents/:agentId/shutdown", agentHandler.HandleShutdown)
			agentsMgmt.PUT("/agents/:agentId/tags", agentHandler.HandleSetTags)
		}

		// workflows:run
		workflowsRun := api.Group("")
		workflowsRun.Use(middleware.PermissionRequired(db, "workflows:run"))
		{
			workflowsRun.POST("/workflows", wfHandler.HandleCreate)
			workflowsRun.POST("/workflows/:id/cancel", wfHandler.HandleCancel)
		}

		// scripts:edit（创建/删除/保存）
		scriptsEdit := api.Group("")
		scriptsEdit.Use(middleware.PermissionRequired(db, "scripts:edit"))
		{
			scriptsEdit.POST("/scripts", scriptHandlerAPI.HandleCreate)
			scriptsEdit.POST("/scripts/delete", scriptHandlerAPI.HandleDelete)
			scriptsEdit.DELETE("/scripts", scriptHandlerAPI.HandleDelete)
			scriptsEdit.POST("/scripts/save", scriptHandlerAPI.HandleSave)
		}

		// scripts:run（运行/停止/日志）
		scriptsRun := api.Group("")
		scriptsRun.Use(middleware.PermissionRequired(db, "scripts:run"))
		{
			scriptsRun.POST("/scripts/run", scriptHandlerAPI.HandleRun)
			scriptsRun.POST("/scripts/stop", scriptHandlerAPI.HandleStop)
			scriptsRun.POST("/scripts/logs", scriptHandlerAPI.HandleLogs)
		}

		// 设置 - dashboard 兼容路径（与 /api/v1/settings 等价，统一前端前缀为 /api）
		settingsView := api.Group("")
		settingsView.Use(middleware.PermissionRequired(db, "settings:view"))
		{
			settingsView.GET("/settings", settingsHandler.HandleGetSettings)
		}
		settingsEdit := api.Group("")
		settingsEdit.Use(middleware.PermissionRequired(db, "settings:edit"))
		{
			settingsEdit.PUT("/settings", settingsHandler.HandleUpdateSettings)
		}
	}

	// ====== Debugger 路由（仅 admin，直连模式） ======
	debuggerHandler := handlers.NewDebugHandler(registry)
	debugger := r.Group("/api/debugger")
	debugger.Use(middleware.AuthRequired(), middleware.RoleRequired("admin"))
	{
		debugger.GET("/info", debuggerHandler.HandleDebuggerInfo)
		debugger.POST("/connect", debuggerHandler.HandleDebuggerConnect)
		debugger.POST("/command", debuggerHandler.HandleDebuggerCommand)
		debugger.GET("/breakpoints", debuggerHandler.HandleDebuggerGetBreakpoints)
		debugger.POST("/breakpoints", debuggerHandler.HandleDebuggerSetBreakpoints)
	}

	apiAudit := r.Group("/api")
	apiAudit.Use(middleware.AuthRequired())
	{
		auditHandler := handlers.NewAuditHandler(db)
		apiAudit.GET("/audit", auditHandler.HandleList)
	}

	// ====== 用户/角色/权限管理路由（细粒度权限） ======
	// users:manage（内置仅 admin 拥有；自定义角色可按需授予）
	adminMgmt := r.Group("/api/admin")
	adminMgmt.Use(middleware.AuthRequired())
	{
		userHandler := handlers.NewUserHandler(db)
		usersMgmt := adminMgmt.Group("")
		usersMgmt.Use(middleware.PermissionRequired(db, "users:manage"))
		{
			usersMgmt.GET("/users", userHandler.HandleList)
			usersMgmt.POST("/users", userHandler.HandleCreate)
			usersMgmt.GET("/users/:id", userHandler.HandleGet)
			usersMgmt.PUT("/users/:id", userHandler.HandleUpdate)
			usersMgmt.DELETE("/users/:id", userHandler.HandleDelete)
			usersMgmt.POST("/users/:id/reset-password", userHandler.HandleResetPassword)
		}

		roleHandler := handlers.NewRoleHandler(db)
		rolesMgmt := adminMgmt.Group("")
		rolesMgmt.Use(middleware.PermissionRequired(db, "roles:manage"))
		{
			rolesMgmt.GET("/roles", roleHandler.HandleListRoles)
			rolesMgmt.GET("/roles/:code", roleHandler.HandleGetRole)
			rolesMgmt.POST("/roles", roleHandler.HandleCreateRole)
			rolesMgmt.PUT("/roles/:code", roleHandler.HandleUpdateRole)
			rolesMgmt.DELETE("/roles/:code", roleHandler.HandleDeleteRole)
		}

		// 权限目录为只读参考，任何能管理用户/角色的人均可查看。
		permView := adminMgmt.Group("")
		permView.Use(middleware.PermissionRequired(db, "users:manage", "roles:manage"))
		{
			permView.GET("/permissions", roleHandler.HandleListPermissions)
		}
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
