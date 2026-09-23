package handlers

// 路由装配：main.go 只负责配置/数据库/后台组件生命周期，
// 全部 gin 中间件与路由注册收口到本文件（自架构盘点第四轮起），
// 为后续按域拆分 handlers 子包提供单一装配点。

import (
	"time"

	_ "github.com/cuihaitao/wingman/orchestrator/server/docs" // swag 生成的 OpenAPI 文档
	"github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// RouterDeps 聚合 RegisterRoutes 所需的跨包组件。
// 组件的构造与生命周期管理留在 main（frameListener.Start/Stop、
// wfEngine 引擎等），本包只消费。
type RouterDeps struct {
	DB *gorm.DB
	// Registry 内存 Agent 注册表（agent 出站连接的落点）
	Registry *agent.Registry
	// WsHub Dashboard WebSocket 广播器
	WsHub *websocket.Hub
	// TeamManager 团队投递管理器（来自 FrameListener）
	TeamManager *agent.TeamManager
	// WfEngine 工作流引擎
	WfEngine *workflow.Engine
	// AuthHandler 已执行 InitAdmin 的认证 handler（main 侧种子后传入）
	AuthHandler *AuthHandler
	// Guacamole 像素面网关（票据 REST + WS 反代 guacd；nil = 不启用，
	// 远程桌面经 guacd 翻译，见 docs/remote-gateway-guacamole-design.md）
	Guacamole *GuacamoleHandler
	// ScriptsDir / StaticDir 静态资源与脚本目录
	ScriptsDir string
	StaticDir  string
	// ProcessStart 进程启动时间（uptime 指标基准）
	ProcessStart time.Time
}

// RegisterRoutes 在 gin engine 上挂载中间件、静态资源与全部 API 路由。
// 中间件顺序与路由路径与原 main.go 装配完全一致，行为等价。
func RegisterRoutes(r *gin.Engine, deps RouterDeps) {
	// gin.Default() = New + 默认 Logger + Recovery；此处替换为自定义请求日志格式，
	// 并保留 Recovery 兜底 panic。
	r.Use(gin.LoggerWithFormatter(middleware.RequestLogFormatter))
	r.Use(gin.Recovery())
	r.Use(middleware.CORS())

	// 指标采集：总请求数（按 method/route/status 计数），在路由注册前挂载
	metricsHandler := NewMetricsHandler(deps.WsHub, deps.Registry, deps.ProcessStart)
	r.Use(metricsHandler.RequestCounter())

	// Swagger UI（由 swag init 生成的 docs 包驱动）
	r.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))

	r.Static("/assets", deps.StaticDir+"/assets")
	r.StaticFile("/favicon.ico", deps.StaticDir+"/favicon.ico")
	r.GET("/", func(c *gin.Context) {
		c.File(deps.StaticDir + "/index.html")
	})

	// Settings handler (DB version for persistence)
	settingsHandler := NewSettingsHandler(deps.DB)
	// 触发器处理器（透传 runtime trigger.* 到 Dashboard）
	triggerHandler := NewTriggerHandler(deps.Registry, deps.DB)
	// 团队处理器（Dashboard 创建团队；runtime agent 随后经 team.join 加入）
	teamHandler := NewTeamHandler(deps.TeamManager)

	// ====== API v1 路由（保留兼容） ======
	v1 := r.Group("/api/v1")
	{
		// Login endpoint with rate limiting to prevent brute force attacks
		v1.POST("/auth/login", middleware.RateLimitMiddleware(middleware.GetRateLimiter()), deps.AuthHandler.HandleLogin)
		// Logout 需要有效会话；无状态 JWT 由客户端丢弃令牌
		v1.POST("/auth/logout", middleware.AuthRequired(), deps.AuthHandler.HandleLogout)

		auth := v1.Group("")
		auth.Use(middleware.AuthRequired())
		{
			statusHandler := NewStatusHandler(deps.DB, deps.Registry)
			profileHandler := NewProfileHandler(deps.DB)
			auth.GET("/status", statusHandler.HandleStatus)
			auth.GET("/health", statusHandler.HandleHealth)
			// Prometheus 指标端点（需认证；Prometheus 抓取配置带 Bearer token）
			auth.GET("/metrics", metricsHandler.HandleMetrics)
			auth.GET("/profile", profileHandler.HandleGetProfile)
			auth.GET("/profile/games", profileHandler.HandleGetGames)
			auth.GET("/profile/permissions", profileHandler.HandleGetPermissions)
			auth.PUT("/profile", profileHandler.HandleUpdateProfile)
			auth.PUT("/profile/password", profileHandler.HandleUpdatePassword)

			// 注：messages/feedback 仅在下方 /api 组注册（匹配前端 wingman.ts 使用的路径），
			// 此处不再重复注册 /api/v1/messages 与 /api/v1/feedback，避免两套可达路径。

			// 只读接口 - 所有登录用户可访问
			windowHandler := NewWindowHandler(deps.Registry)
			auth.GET("/windows", windowHandler.HandleList)
			// 与 /api/settings 的 settingsView 等价（双注册路径权限一致）
			auth.GET("/settings", middleware.PermissionRequired(deps.DB, "settings:view"), settingsHandler.HandleGetSettings)
		}

		// 写入接口 - 需要 admin 权限
		admin := v1.Group("")
		admin.Use(middleware.AuthRequired(), middleware.RoleRequired("admin"))
		{
			screenshotHandler := NewScreenshotHandler(deps.WsHub)
			admin.POST("/screenshot", screenshotHandler.HandleScreenshot)

			scriptHandler := NewScriptHandler(deps.DB, deps.ScriptsDir, deps.Registry)
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
		messageHandler := NewMessageHandler(deps.DB)
		feedbackHandler := NewFeedbackHandler(deps.DB)
		api.GET("/messages", messageHandler.HandleList)
		api.GET("/messages/unread-count", messageHandler.HandleUnreadCount)
		api.POST("/messages/:id/read", messageHandler.HandleMarkRead)
		api.POST("/messages/read-all", messageHandler.HandleMarkAllRead)
		api.POST("/feedback", feedbackHandler.HandleCreate)

		// Agent 管理 - 只读接口
		agentHandler := NewAgentHandler(deps.Registry, deps.DB)
		api.GET("/agents", agentHandler.HandleList)
		api.GET("/agents/:agentId", agentHandler.HandleGet)
		api.GET("/agents/:agentId/triggers", triggerHandler.HandleList)

		// 工作流管理 - 只读接口
		wfHandler := NewWorkflowHandler(deps.WfEngine, deps.DB)
		api.GET("/workflows", wfHandler.HandleList)
		api.GET("/workflows/:id", wfHandler.HandleGet)
		api.GET("/workflows/:id/workers", wfHandler.HandleGetWorkers)
		api.GET("/workflows/:id/steps/:stepId/status", wfHandler.HandleGetStepStatus)
		api.GET("/workflow-templates", wfHandler.HandleListTemplates)

		// 脚本管理 - 只读接口
		scriptHandlerAPI := NewScriptHandler(deps.DB, deps.ScriptsDir, deps.Registry)
		api.GET("/scripts", scriptHandlerAPI.HandleList)
		api.POST("/scripts/content", scriptHandlerAPI.HandleGetContent) // 获取内容

		// 批量操作（按 agentIds/tags 选择器 fan-out）
		batchHandler := NewBatchHandler(deps.DB, deps.ScriptsDir, deps.Registry)

		// 写入接口 - 细粒度权限（PermissionRequired，admin 自动放行）。
		// 取代原先粗粒度 RoleRequired("admin")，使 operator/viewer 等自定义角色
		// 可按权限码（agents:manage / workflows:run / scripts:edit / scripts:run）获得受限访问。
		// agents:manage
		agentsMgmt := api.Group("")
		agentsMgmt.Use(middleware.PermissionRequired(deps.DB, "agents:manage"))
		{
			agentsMgmt.POST("/teams", teamHandler.HandleCreate)
			agentsMgmt.POST("/agents/batch/trigger", batchHandler.HandleBatchTrigger)
			agentsMgmt.POST("/agents/:agentId/shutdown", agentHandler.HandleShutdown)
			agentsMgmt.PUT("/agents/:agentId/tags", agentHandler.HandleSetTags)
			agentsMgmt.POST("/agents/:agentId/triggers/toggle", triggerHandler.HandleToggle)
			agentsMgmt.POST("/agents/:agentId/triggers", triggerHandler.HandleCreate)
			agentsMgmt.PUT("/agents/:agentId/triggers/:triggerId", triggerHandler.HandleUpdate)
			agentsMgmt.DELETE("/agents/:agentId/triggers/:triggerId", triggerHandler.HandleRemove)
		}

		// workflows:run
		workflowsRun := api.Group("")
		workflowsRun.Use(middleware.PermissionRequired(deps.DB, "workflows:run"))
		{
			workflowsRun.POST("/workflows", wfHandler.HandleCreate)
			workflowsRun.POST("/workflows/:id/cancel", wfHandler.HandleCancel)
		}

		// scripts:edit（创建/删除/保存）
		scriptsEdit := api.Group("")
		scriptsEdit.Use(middleware.PermissionRequired(deps.DB, "scripts:edit"))
		{
			scriptsEdit.POST("/scripts", scriptHandlerAPI.HandleCreate)
			scriptsEdit.POST("/scripts/delete", scriptHandlerAPI.HandleDelete)
			scriptsEdit.DELETE("/scripts", scriptHandlerAPI.HandleDelete)
			scriptsEdit.POST("/scripts/save", scriptHandlerAPI.HandleSave)
		}

		// scripts:run（运行/停止/日志 + 批量操作）
		scriptsRun := api.Group("")
		scriptsRun.Use(middleware.PermissionRequired(deps.DB, "scripts:run"))
		{
			scriptsRun.POST("/scripts/run", scriptHandlerAPI.HandleRun)
			scriptsRun.POST("/scripts/stop", scriptHandlerAPI.HandleStop)
			scriptsRun.POST("/scripts/logs", scriptHandlerAPI.HandleLogs)
			scriptsRun.POST("/agents/batch/run-script", batchHandler.HandleBatchRunScript)
			scriptsRun.POST("/agents/batch/stop-script", batchHandler.HandleBatchStopScript)
		}

		// 设置 - dashboard 兼容路径（与 /api/v1/settings 等价，统一前端前缀为 /api）
		settingsView := api.Group("")
		settingsView.Use(middleware.PermissionRequired(deps.DB, "settings:view"))
		{
			settingsView.GET("/settings", settingsHandler.HandleGetSettings)
		}
		settingsEdit := api.Group("")
		settingsEdit.Use(middleware.PermissionRequired(deps.DB, "settings:edit"))
		{
			settingsEdit.PUT("/settings", settingsHandler.HandleUpdateSettings)
		}

		// 远程桌面（Guacamole 像素面）：票据签发。监看/接管任一权限即可申请；
		// 接管（readOnly=false）在 handler 内额外要求 desktop:control。
		if deps.Guacamole != nil {
			desktop := api.Group("")
			desktop.Use(middleware.PermissionRequired(deps.DB, "desktop:view", "desktop:control"))
			{
				desktop.POST("/remote/tickets", deps.Guacamole.HandleTicketCreate)
			}
		}
	}

	// 像素面 WS 隧道：票据即凭证（浏览器 WS 无法自定义 header，不挂
	// AuthRequired，与 /ws 先例一致），故挂在 /api 组之外、路径仍收敛在
	// /api 前缀下（与 cockpit 端点约定对齐——DG-6 同一套端点）。
	if deps.Guacamole != nil {
		r.GET("/api/remote/guacamole", deps.Guacamole.HandleWS)
	}

	// ====== Debugger 路由（仅 admin，直连模式） ======
	debuggerHandler := NewDebugHandler(deps.Registry)
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
		auditHandler := NewAuditHandler(deps.DB)
		apiAudit.GET("/audit", auditHandler.HandleList)
	}

	// ====== 用户/角色/权限管理路由（细粒度权限） ======
	// users:manage（内置仅 admin 拥有；自定义角色可按需授予）
	adminMgmt := r.Group("/api/admin")
	adminMgmt.Use(middleware.AuthRequired())
	{
		userHandler := NewUserHandler(deps.DB)
		usersMgmt := adminMgmt.Group("")
		usersMgmt.Use(middleware.PermissionRequired(deps.DB, "users:manage"))
		{
			usersMgmt.GET("/users", userHandler.HandleList)
			usersMgmt.POST("/users", userHandler.HandleCreate)
			usersMgmt.GET("/users/:id", userHandler.HandleGet)
			usersMgmt.PUT("/users/:id", userHandler.HandleUpdate)
			usersMgmt.DELETE("/users/:id", userHandler.HandleDelete)
			usersMgmt.POST("/users/:id/reset-password", userHandler.HandleResetPassword)
		}

		roleHandler := NewRoleHandler(deps.DB)
		rolesMgmt := adminMgmt.Group("")
		rolesMgmt.Use(middleware.PermissionRequired(deps.DB, "roles:manage"))
		{
			rolesMgmt.GET("/roles", roleHandler.HandleListRoles)
			rolesMgmt.GET("/roles/:code", roleHandler.HandleGetRole)
			rolesMgmt.POST("/roles", roleHandler.HandleCreateRole)
			rolesMgmt.PUT("/roles/:code", roleHandler.HandleUpdateRole)
			rolesMgmt.DELETE("/roles/:code", roleHandler.HandleDeleteRole)
		}

		// 权限目录为只读参考，任何能管理用户/角色的人均可查看。
		permView := adminMgmt.Group("")
		permView.Use(middleware.PermissionRequired(deps.DB, "users:manage", "roles:manage"))
		{
			permView.GET("/permissions", roleHandler.HandleListPermissions)
		}
	}

	r.GET("/ws", func(c *gin.Context) {
		websocket.HandleWebSocket(c, deps.WsHub)
	})
}
