package main

import (
	"fmt"
	"log"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/cuihaitao/wingman/server/internal/handlers"
	"github.com/cuihaitao/wingman/server/internal/middleware"
	"github.com/cuihaitao/wingman/server/internal/models"
	"github.com/cuihaitao/wingman/server/pkg/websocket"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

const (
	Port         = 9527
	DbPath       = "./data/wingman.db"
	StaticDir    = "../build/dist"
	AgentAddress = "localhost:8888" // C++ Agent HTTP API
)

func main() {
	// 创建数据目录
	os.MkdirAll("./data", 0755)

	// 初始化数据库
	db, err := gorm.Open(sqlite.Open(DbPath), &gorm.Config{})
	if err != nil {
		log.Fatalf("Failed to connect database: %v", err)
	}

	// 自动迁移
	if err := models.AutoMigrate(db); err != nil {
		log.Fatalf("Failed to migrate database: %v", err)
	}

	// 初始化管理员
	authHandler := handlers.NewAuthHandler(db)
	authHandler.InitAdmin()

	// WebSocket Hub
	wsHub := websocket.NewHub()
	go wsHub.Run()

	// Gin 路由
	r := gin.Default()

	// CORS 中间件
	r.Use(middleware.CORS())

	// 静态文件服务
	r.Static("/assets", StaticDir+"/assets")
	r.StaticFile("/favicon.ico", StaticDir+"/favicon.ico")
	r.GET("/", func(c *gin.Context) {
		c.File(StaticDir + "/index.html")
	})
	// SPA 路由支持
	r.GET("//*filepath", func(c *gin.Context) {
		c.File(StaticDir + "/index.html")
	})

	// API v1 路由
	v1 := r.Group("/api/v1")
	{
		// 认证
		v1.POST("/auth/login", authHandler.HandleLogin)
		v1.POST("/auth/logout", authHandler.HandleLogout)

		// 需要认证的路由
		auth := v1.Group("")
		auth.Use(middleware.AuthRequired())
		{
			// 状态
			statusHandler := handlers.NewStatusHandler(db)
			auth.GET("/status", statusHandler.HandleStatus)
			auth.GET("/health", statusHandler.HandleHealth)

			// 脚本管理
			scriptHandler := handlers.NewScriptHandler(db)
			auth.GET("/scripts", scriptHandler.HandleList)
			auth.POST("/scripts", scriptHandler.HandleCreate)
			auth.DELETE("/scripts", scriptHandler.HandleDelete)
			auth.POST("/scripts/content", scriptHandler.HandleGetContent)
			auth.POST("/scripts/save", scriptHandler.HandleSave)
			auth.POST("/scripts/run", scriptHandler.HandleRun)
			auth.POST("/scripts/stop", scriptHandler.HandleStop)
			auth.POST("/scripts/logs", scriptHandler.HandleLogs)

			// 窗口管理
			windowHandler := handlers.NewWindowHandler()
			auth.GET("/windows", windowHandler.HandleList)

			// 设置
			auth.GET("/settings", handlers.HandleGetSettings)
			auth.PUT("/settings", handlers.HandleUpdateSettings)
		}
	}

	// Debugger API (无认证，VSCode 扩展使用)
	debugger := r.Group("/api/debugger")
	{
		debugger.POST("/connect", handlers.HandleDebuggerConnect)
		debugger.POST("/command", handlers.HandleDebuggerCommand)
		debugger.GET("/breakpoints", handlers.HandleDebuggerGetBreakpoints)
		debugger.POST("/breakpoints", handlers.HandleDebuggerSetBreakpoints)
	}

	// WebSocket
	r.GET("/ws", func(c *gin.Context) {
		websocket.HandleWebSocket(c, wsHub)
	})

	// 启动服务器
	addr := fmt.Sprintf("0.0.0.0:%d", Port)
	log.Printf("Server starting on http://%s", addr)
	if err := r.Run(addr); err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
}
