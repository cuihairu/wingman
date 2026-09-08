// Package integration 提供 GUI IPC Runtime 集成测试（tests/integration 目录）。
//
// 测试装配与 main.go 同构的服务端栈（真实中间件链 + SQLite 内存库 + RBAC 种子 +
// FrameListener + Workflow Engine + WebSocket Hub），用 TCP mock agent 模拟
// C++ runtime 的 outbound 连接（替代真实 runtime），用 WebSocket 模拟
// dashboard 客户端，覆盖三条链路：
//  1. Agent 注册→心跳→命令下发→结果回传完整链路（agent_command_test.go）
//  2. Workflow 提交→step 执行→agent 响应→workflow 完成（workflow_chain_test.go）
//  3. WebSocket 事件广播（ws_broadcast_test.go）
package integration

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"math/rand"
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/handlers"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// testEnv 聚合一次集成测试所需的全部服务端组件。
type testEnv struct {
	db         *gorm.DB
	hub        *ws.Hub
	registry   *agent.Registry
	listener   *agentPkg.FrameListener
	httpSrv    *httptest.Server
	agentAddr  string
	scriptsDir string
}

// newTestEnv 启动一套隔离的编排器栈：内存 SQLite + RBAC 种子 + 内置用户 +
// WebSocket Hub + Agent Registry + FrameListener + Workflow Engine + 真实中间件路由。
func newTestEnv(t *testing.T) *testEnv {
	t.Helper()
	gin.SetMode(gin.TestMode)
	t.Setenv("WINGMAN_JWT_SECRET", "tests-integration-jwt-secret-0123456789abcdef")

	db := newMemoryDB(t)
	if err := rbac.Seed(db); err != nil {
		t.Fatalf("seed rbac: %v", err)
	}

	// 内置用户：admin 超级用户 / operator 操作员 / viewer 只读
	builtinUsers := []struct {
		username string
		password string
		role     string
	}{
		{"admin", "Admin123!", "admin"},
		{"operator", "Operator123!", "operator"},
		{"viewer", "Viewer123!", "viewer"},
	}
	for _, u := range builtinUsers {
		hashed, err := security.HashPassword(u.password)
		if err != nil {
			t.Fatalf("hash password for %s: %v", u.username, err)
		}
		user := models.User{Username: u.username, Password: hashed, Role: u.role, Active: true}
		if err := db.Create(&user).Error; err != nil {
			t.Fatalf("create user %s: %v", u.username, err)
		}
	}

	hub := ws.NewHub()
	go hub.Run()

	registry := agent.NewRegistry(hub)

	listener := agentPkg.NewFrameListener(registry, hub)
	// 与 main.go 一致：脚本输出事件持久化到 ExecutionLog
	listener.SetScriptOutputHandler(func(agentID string, data map[string]any) {
		scriptID, _ := data["scriptId"].(string)
		message, _ := data["message"].(string)
		level, _ := data["level"].(string)
		if level == "" {
			level = "info"
		}
		db.Create(&models.ExecutionLog{ScriptID: scriptID, Output: message, Level: level})
	})

	scriptsDir := t.TempDir()
	wfEngine := workflow.NewEngine(db, registry, hub, scriptsDir)

	agentAddr := reserveAddr(t)
	if err := listener.Start(agentAddr); err != nil {
		t.Fatalf("start frame listener on %s: %v", agentAddr, err)
	}

	env := &testEnv{
		db:         db,
		hub:        hub,
		registry:   registry,
		listener:   listener,
		agentAddr:  agentAddr,
		scriptsDir: scriptsDir,
	}
	env.httpSrv = httptest.NewServer(buildRouter(db, registry, hub, wfEngine, scriptsDir))
	t.Cleanup(env.httpSrv.Close)
	t.Cleanup(listener.Stop)
	return env
}

// buildRouter 装配与 main.go 同构的路由子集（真实 AuthRequired /
// PermissionRequired 中间件链），覆盖三个测试场景所需的端点。
func buildRouter(db *gorm.DB, registry *agent.Registry, hub *ws.Hub, wfEngine *workflow.Engine, scriptsDir string) *gin.Engine {
	authHandler := handlers.NewAuthHandler(db)
	agentHandler := handlers.NewAgentHandler(registry, db)
	scriptHandler := handlers.NewScriptHandler(db, scriptsDir, registry)
	wfHandler := handlers.NewWorkflowHandler(wfEngine, db)

	r := gin.New()

	v1 := r.Group("/api/v1")
	v1.POST("/auth/login", authHandler.HandleLogin)

	api := r.Group("/api")
	api.Use(middleware.AuthRequired())
	{
		// 只读接口 - 所有登录用户可访问
		api.GET("/agents", agentHandler.HandleList)
		api.GET("/agents/:agentId", agentHandler.HandleGet)
		api.GET("/workflows", wfHandler.HandleList)
		api.GET("/workflows/:id", wfHandler.HandleGet)
		api.GET("/workflows/:id/steps/:stepId/status", wfHandler.HandleGetStepStatus)

		// scripts:run
		scriptsRun := api.Group("")
		scriptsRun.Use(middleware.PermissionRequired(db, "scripts:run"))
		{
			scriptsRun.POST("/scripts/run", scriptHandler.HandleRun)
		}

		// workflows:run
		workflowsRun := api.Group("")
		workflowsRun.Use(middleware.PermissionRequired(db, "workflows:run"))
		{
			workflowsRun.POST("/workflows", wfHandler.HandleCreate)
		}
	}

	r.GET("/ws", func(c *gin.Context) { ws.HandleWebSocket(c, hub) })
	return r
}

// ---------- 数据库 ----------

func newMemoryDB(t *testing.T) *gorm.DB {
	t.Helper()
	safe := strings.Map(func(r rune) rune {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9', r == '_':
			return r
		default:
			return '_'
		}
	}, t.Name())
	dsn := fmt.Sprintf("file:ti2_%s_%d?mode=memory&cache=shared&_busy_timeout=5000", safe, rand.Int63())
	db, err := gorm.Open(sqlite.Open(dsn), &gorm.Config{})
	if err != nil {
		t.Fatalf("open sqlite: %v", err)
	}
	// 单连接串行化访问：shared-cache 内存库在引擎写/HTTP 读并发时会出现
	// "database table is locked"（busy_timeout 对 SQLITE_LOCKED 不生效）。
	sqlDB, err := db.DB()
	if err != nil {
		t.Fatalf("raw db handle: %v", err)
	}
	sqlDB.SetMaxOpenConns(1)
	if err := models.AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return db
}

// reserveAddr 探测一个空闲的本地端口供 FrameListener 使用。
func reserveAddr(t *testing.T) string {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("reserve addr: %v", err)
	}
	defer ln.Close()
	return ln.Addr().String()
}

// ---------- HTTP 辅助 ----------

type apiResult struct {
	Status int
	Body   []byte
}

func (e *testEnv) do(t *testing.T, method, path, token string, body any) apiResult {
	t.Helper()
	var reader io.Reader
	if body != nil {
		raw, err := json.Marshal(body)
		if err != nil {
			t.Fatalf("marshal request body for %s %s: %v", method, path, err)
		}
		reader = bytes.NewReader(raw)
	}
	req, err := http.NewRequest(method, e.httpSrv.URL+path, reader)
	if err != nil {
		t.Fatalf("build request %s %s: %v", method, path, err)
	}
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	resp, err := e.httpSrv.Client().Do(req)
	if err != nil {
		t.Fatalf("%s %s: %v", method, path, err)
	}
	defer resp.Body.Close()
	raw, _ := io.ReadAll(resp.Body)
	return apiResult{Status: resp.StatusCode, Body: raw}
}

func builtinPassword(username string) string {
	switch username {
	case "admin":
		return "Admin123!"
	case "operator":
		return "Operator123!"
	case "viewer":
		return "Viewer123!"
	}
	return ""
}

// login 以指定内置用户登录，要求返回 wantStatus，成功时返回 JWT。
func (e *testEnv) loginWith(t *testing.T, username string, wantStatus int) string {
	t.Helper()
	res := e.do(t, "POST", "/api/v1/auth/login", "", map[string]string{
		"username": username,
		"password": builtinPassword(username),
	})
	if res.Status != wantStatus {
		t.Fatalf("login %s: got %d want %d (%s)", username, res.Status, wantStatus, res.Body)
	}
	if wantStatus != http.StatusOK {
		return ""
	}
	var resp struct {
		Token string `json:"token"`
	}
	if err := json.Unmarshal(res.Body, &resp); err != nil || resp.Token == "" {
		t.Fatalf("login %s: no token in response (%s)", username, res.Body)
	}
	return resp.Token
}

func (e *testEnv) login(t *testing.T, username string) string {
	t.Helper()
	return e.loginWith(t, username, http.StatusOK)
}

func (e *testEnv) getAgentJSON(t *testing.T, token, agentID string) map[string]any {
	t.Helper()
	res := e.do(t, "GET", "/api/agents/"+agentID, token, nil)
	if res.Status != http.StatusOK {
		t.Fatalf("GET agent %s: %d %s", agentID, res.Status, res.Body)
	}
	var resp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &resp); err != nil {
		t.Fatalf("decode agent %s: %v", agentID, err)
	}
	return resp.Data
}

func (e *testEnv) waitForAgentStatus(t *testing.T, token, agentID, want string) map[string]any {
	t.Helper()
	var data map[string]any
	waitUntil(t, 5*time.Second, fmt.Sprintf("agent %s 状态变为 %s", agentID, want), func() bool {
		data = e.getAgentJSON(t, token, agentID)
		return data["status"] == want
	})
	return data
}

func (e *testEnv) createWorkflow(t *testing.T, token, name string, steps []map[string]any) uint {
	t.Helper()
	res := e.do(t, "POST", "/api/workflows", token, map[string]any{"name": name, "steps": steps})
	if res.Status != http.StatusOK {
		t.Fatalf("create workflow %s: %d %s", name, res.Status, res.Body)
	}
	var resp struct {
		Data struct {
			WorkflowID uint `json:"workflowId"`
		} `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &resp); err != nil || resp.Data.WorkflowID == 0 {
		t.Fatalf("create workflow %s: bad response %s", name, res.Body)
	}
	return resp.Data.WorkflowID
}

func (e *testEnv) getStepStatus(t *testing.T, token string, workflowID uint, stepID string) map[string]any {
	t.Helper()
	res := e.do(t, "GET", fmt.Sprintf("/api/workflows/%d/steps/%s/status", workflowID, stepID), token, nil)
	if res.Status != http.StatusOK {
		t.Fatalf("GET step status %s/%d: %d %s", stepID, workflowID, res.Status, res.Body)
	}
	var resp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &resp); err != nil {
		t.Fatalf("decode step status: %v", err)
	}
	return resp.Data
}

func (e *testEnv) waitForWorkflowStatus(t *testing.T, token string, workflowID uint, want string) map[string]any {
	t.Helper()
	var last map[string]any
	waitUntil(t, 10*time.Second, fmt.Sprintf("workflow %d 达到状态 %s", workflowID, want), func() bool {
		res := e.do(t, "GET", fmt.Sprintf("/api/workflows/%d", workflowID), token, nil)
		if res.Status != http.StatusOK {
			return false
		}
		var resp struct {
			Data map[string]any `json:"data"`
		}
		if err := json.Unmarshal(res.Body, &resp); err != nil {
			return false
		}
		last = resp.Data
		return resp.Data["status"] == want
	})
	return last
}

// ---------- 通用等待 ----------

func waitUntil(t *testing.T, timeout time.Duration, desc string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(15 * time.Millisecond)
	}
	t.Fatalf("condition not met within %v: %s", timeout, desc)
}
