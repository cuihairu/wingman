// @title           Wingman Orchestrator API
// @version         1.0
// @description     Wingman 远程中控编排器 HTTP API（auth / RBAC / agent / script / workflow / messages / settings）。
// @description     架构约束：Dashboard 与远程客户端只连接本 server，不直连 runtime。
// @host            localhost:9527
// @BasePath        /api
// @securityDefinitions.apikey BearerAuth
// @in header
// @name Authorization
// @description "Bearer <JWT>"（由 POST /api/v1/auth/login 获取）
package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/config"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/handlers"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func main() {
	if err := run(); err != nil {
		log.Fatalf("server exited with error: %v", err)
	}
}

// seedRBAC 内置 RBAC 角色/权限种子（包级变量以便测试注入失败场景）。
var seedRBAC = rbac.Seed

// processStartedAt 进程启动时间（uptime 指标基准）。
var processStartedAt = time.Now()

// run 装配并启动整个编排器。从 main 中提取以便测试覆盖：
// 配置/数据库/迁移失败与端口占用均以 error 返回而非直接退出进程。
// gin 中间件与全部路由注册收口在 handlers.RegisterRoutes。
func run() error {
	cfg, err := config.Load()
	if err != nil {
		return fmt.Errorf("invalid configuration: %w", err)
	}

	if err := os.MkdirAll("./data", 0755); err != nil {
		return fmt.Errorf("failed to create data directory: %w", err)
	}

	db, err := gorm.Open(sqlite.Open(cfg.DBPath), &gorm.Config{})
	if err != nil {
		return fmt.Errorf("failed to connect database: %w", err)
	}

	if err := models.AutoMigrate(db); err != nil {
		return fmt.Errorf("failed to migrate database: %w", err)
	}

	authHandler := handlers.NewAuthHandler(db)
	authHandler.InitAdmin()

	// 内置 RBAC 角色/权限种子
	if err := seedRBAC(db); err != nil {
		log.Printf("Failed to seed RBAC: %v", err)
	}

	wsHub := websocket.NewHub()
	go wsHub.Run()

	// Agent 注册表
	registry := agent.NewRegistry(wsHub)
	registry.SetTagStore(handlers.NewAgentTagStore(db))
	go registry.StartHeartbeatCheck()

	// TCP Frame Listener（接受 runtime outbound 连接）
	// Note: cfg.AgentAddr is the listening address for runtime connections,
	// not a target address for dialing (deprecated old approach).
	frameListener := agent.NewFrameListener(registry, wsHub)
	// agent 注册 token 鉴权（WINGMAN_AGENT_TOKENS，空=关闭；
	// docs/agent-token-auth-design.md §3）
	frameListener.SetAgentTokens(cfg.AgentTokens)
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

	// 像素面票据管理器 + Guacamole 网关（远程桌面经 guacd 翻译；
	// docs/remote-gateway-guacamole-design.md §4/DG-6）。会话录像检索
	// 经共享卷目录读取（设计 §16），录制双路径未配置时 handler 侧 501
	desktopTickets := remoteticket.NewManager()
	defer desktopTickets.Stop()
	guacamoleHandler := handlers.NewGuacamoleHandler(db, registry, desktopTickets,
		cfg.GuacdAddr, cfg.GuacdDrivePath, cfg.GuacdRecordingPath, cfg.RecordingDir)
	recordingsHandler := handlers.NewRecordingsHandler(db, cfg.RecordingDir)

	// gin engine 与全部路由（中间件/静态资源/API）由 handlers 包统一装配
	r := gin.New()
	handlers.RegisterRoutes(r, handlers.RouterDeps{
		DB:           db,
		Registry:     registry,
		WsHub:        wsHub,
		TeamManager:  frameListener.GetTeamManager(),
		WfEngine:     wfEngine,
		AuthHandler:  authHandler,
		Guacamole:    guacamoleHandler,
		Recordings:   recordingsHandler,
		ScriptsDir:   cfg.ScriptsDir,
		StaticDir:    cfg.StaticDir,
		ProcessStart: processStartedAt,
	})

	addr := config.Addr(cfg)
	log.Printf("Server starting on http://%s", addr)

	srv := &http.Server{Addr: addr, Handler: r}

	// 信号驱动优雅关闭：SIGTERM/SIGINT 触发 drain，而非进程立即退出。
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	errCh := make(chan error, 1)
	go func() {
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			errCh <- err
			return
		}
		errCh <- nil
	}()

	return waitHTTPServer(ctx, srv, errCh, frameListener, registry, db)
}

// waitHTTPServer 阻塞等待 HTTP server 结束（启动失败）或收到退出信号，
// 执行相应关闭流程并返回 run() 的最终错误（提取为独立函数以便测试覆盖各分支）。
func waitHTTPServer(ctx context.Context, srv *http.Server, errCh <-chan error, frameListener *agent.FrameListener, registry *agent.Registry, db *gorm.DB) error {
	select {
	case err := <-errCh:
		// HTTP 启动失败（如端口占用）：清理已启动的后台组件后返回错误。
		shutdownComponents(frameListener, registry, db)
		if err != nil {
			return fmt.Errorf("failed to start server: %w", err)
		}
		return nil

	case <-ctx.Done():
		// SIGTERM/SIGINT：先优雅排空在途 HTTP 请求（带超时），再停组件。
		log.Printf("Shutdown signal received, draining HTTP connections...")
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
		defer cancel()
		if err := srv.Shutdown(shutdownCtx); err != nil {
			log.Printf("HTTP server shutdown incomplete: %v", err)
		}
		shutdownComponents(frameListener, registry, db)
		log.Printf("Server exited gracefully")
		return nil
	}
}

// shutdownComponents 按依赖逆序停止后台组件并释放数据库连接。
// 各组件的 Stop 均由 sync.Once 保护，重复调用安全。
func shutdownComponents(frameListener *agent.FrameListener, registry *agent.Registry, db *gorm.DB) {
	frameListener.Stop()
	registry.Stop()
	if sqlDB, err := db.DB(); err == nil {
		if err := sqlDB.Close(); err != nil {
			log.Printf("Failed to close database: %v", err)
		}
	}
}
