package main

import (
	"log"
	"os"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/config"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/handlers"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
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

	r := gin.Default()
	r.Use(middleware.CORS())

	r.Static("/assets", cfg.StaticDir+"/assets")
	r.StaticFile("/favicon.ico", cfg.StaticDir+"/favicon.ico")
	r.GET("/", func(c *gin.Context) {
		c.File(cfg.StaticDir + "/index.html")
	})

	v1 := r.Group("/api/v1")
	{
		v1.POST("/auth/login", authHandler.HandleLogin)
		v1.POST("/auth/logout", authHandler.HandleLogout)

		auth := v1.Group("")
		auth.Use(middleware.AuthRequired())
		{
			statusHandler := handlers.NewStatusHandler(db, cfg.AgentAddr)
			auth.GET("/status", statusHandler.HandleStatus)
			auth.GET("/health", statusHandler.HandleHealth)

			scriptHandler := handlers.NewScriptHandler(db, cfg.ScriptsDir, cfg.AgentAddr)
			auth.GET("/scripts", scriptHandler.HandleList)
			auth.POST("/scripts", scriptHandler.HandleCreate)
			auth.DELETE("/scripts", scriptHandler.HandleDelete)
			auth.POST("/scripts/content", scriptHandler.HandleGetContent)
			auth.POST("/scripts/save", scriptHandler.HandleSave)
			auth.POST("/scripts/run", scriptHandler.HandleRun)
			auth.POST("/scripts/stop", scriptHandler.HandleStop)
			auth.POST("/scripts/logs", scriptHandler.HandleLogs)

			windowHandler := handlers.NewWindowHandler(cfg.AgentAddr)
			auth.GET("/windows", windowHandler.HandleList)

			auth.GET("/settings", handlers.HandleGetSettings)
			auth.PUT("/settings", handlers.HandleUpdateSettings)

			screenshotHandler := handlers.NewScreenshotHandler(wsHub)
			auth.POST("/screenshot", screenshotHandler.HandleScreenshot)
		}
	}

	debugger := r.Group("/api/debugger")
	debugger.Use(middleware.AuthRequired())
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
