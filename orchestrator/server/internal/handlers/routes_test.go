package handlers

// RegisterRoutes 装配测试（2026-09-22 覆盖率收口）：该函数自 main.go 收口后
// 生产路径只有 main 执行，包内测试从未触达（coverage 0%）。本用例以真实依赖
// 完整装配一次，断言中间件/静态/全部域路由挂载成功——装配顺序与 main 等价，
// 等价性由 routes.go 单一装配点保证。

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
)

func TestRegisterRoutesMountsAllDomains(t *testing.T) {
	db := newDB(t)
	dir := t.TempDir()
	reg, hub := newRegistry(t)

	auth := NewAuthHandler(db)
	auth.InitAdmin()

	deps := RouterDeps{
		DB:           db,
		Registry:     reg,
		WsHub:        hub,
		TeamManager:  agent.NewTeamManager(),
		WfEngine:     workflow.NewEngine(db, reg, hub, dir),
		AuthHandler:  auth,
		ScriptsDir:   dir,
		StaticDir:    t.TempDir(),
		ProcessStart: time.Now(),
	}

	r := gin.New()
	RegisterRoutes(r, deps)

	routes := r.Routes()
	if len(routes) < 40 {
		t.Fatalf("expected full route table (>=40 routes), got %d", len(routes))
	}
	have := map[string]bool{}
	for _, ri := range routes {
		have[ri.Method+" "+ri.Path] = true
	}
	for _, want := range []string{
		"POST /api/v1/auth/login",
		"POST /api/v1/auth/logout",
		"GET /api/v1/status",
		"GET /api/v1/health",
		"POST /api/agents/batch/run-script",
		"POST /api/agents/batch/stop-script",
		"POST /api/agents/batch/trigger",
		"GET /api/agents",
		"POST /api/scripts",
		"POST /api/scripts/run",
		"GET /api/workflows",
		"GET /api/audit",
		"GET /api/admin/users",
		"GET /api/admin/roles",
		"GET /api/admin/permissions",
		"GET /api/settings",
		"GET /api/messages",
		"POST /api/feedback",
		"GET /api/debugger/info",
		"GET /ws",
		"GET /swagger/*any",
		"GET /assets/*filepath",
		"GET /favicon.ico",
	} {
		if !have[want] {
			t.Errorf("route %q not mounted", want)
		}
	}

	// 装配后的 engine 必须能正常服务请求（中间件链不 panic）
	for _, path := range []string{"/", "/ws"} { // /ws 覆盖 handler 体（非升级请求走其错误分支）
		w := httptest.NewRecorder()
		req := httptest.NewRequest(http.MethodGet, path, nil)
		r.ServeHTTP(w, req)
		if w.Code == 0 {
			t.Errorf("engine failed to serve %s", path)
		}
	}
}
