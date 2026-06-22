package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// handlerMockConn 满足 agent.AgentConn，可配置返回与计数。
type handlerMockConn struct {
	mu        sync.Mutex
	responses []map[string]any
	errs      []error
	calls     int
	delay     time.Duration
}

func (m *handlerMockConn) next() (map[string]any, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls++
	idx := m.calls - 1
	var err error
	if idx < len(m.errs) {
		err = m.errs[idx]
	}
	resp := map[string]any{"success": true}
	if idx < len(m.responses) && m.responses[idx] != nil {
		resp = m.responses[idx]
	}
	if m.delay > 0 {
		time.Sleep(m.delay)
	}
	return resp, err
}

func (m *handlerMockConn) SendCommand(method string, data map[string]any) (map[string]any, error) {
	return m.next()
}
func (m *handlerMockConn) SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error) {
	return m.next()
}
func (m *handlerMockConn) callCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.calls
}

func newDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{})
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	if err := models.AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return db
}

func newRegistry(t *testing.T) (*agent.Registry, *ws.Hub) {
	t.Helper()
	hub := ws.NewHub()
	go hub.Run()
	return agent.NewRegistry(hub), hub
}

// ---------- Script handler ----------

func setupScriptRouter(t *testing.T) (*gin.Engine, string, *gorm.DB) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	sh := NewScriptHandler(db, dir, reg)

	r := gin.New()
	g := r.Group("/api/scripts").Use(asAdmin(1))
	g.GET("", sh.HandleList)
	g.POST("", sh.HandleCreate)
	g.POST("/delete", sh.HandleDelete)
	g.POST("/content", sh.HandleGetContent)
	g.POST("/save", sh.HandleSave)
	g.POST("/run", sh.HandleRun)
	g.POST("/stop", sh.HandleStop)
	g.POST("/logs", sh.HandleLogs)
	return r, dir, db
}

func TestScriptCreateAndList(t *testing.T) {
	r, dir, db := setupScriptRouter(t)

	w := doJSON(r, "POST", "/api/scripts", map[string]any{"name": "hello", "description": "d"})
	if w.Code != http.StatusOK {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	// 文件应已落盘
	if _, err := os.Stat(filepath.Join(dir, "hello.lua")); err != nil {
		t.Errorf("expected hello.lua created: %v", err)
	}
	// 数据库应有记录
	var count int64
	db.Model(&models.Script{}).Count(&count)
	if count != 1 {
		t.Errorf("expected 1 script row, got %d", count)
	}

	w = doJSON(r, "GET", "/api/scripts", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d", w.Code)
	}
	var resp struct {
		Data []models.Script `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Data) != 1 {
		t.Errorf("expected 1 script in list, got %d", len(resp.Data))
	}
}

func TestScriptCreateValidation(t *testing.T) {
	r, _, _ := setupScriptRouter(t)
	cases := []struct {
		name string
		body map[string]any
	}{
		{"traversal", map[string]any{"name": "../etc"}},
		{"slash", map[string]any{"name": "a/b"}},
		{"nullbyte", map[string]any{"name": "a\x00b"}},
	}
	for _, tc := range cases {
		w := doJSON(r, "POST", "/api/scripts", tc.body)
		if w.Code != http.StatusBadRequest {
			t.Errorf("%s: expected 400, got %d %s", tc.name, w.Code, w.Body.String())
		}
	}
}

func TestScriptSaveGetContent(t *testing.T) {
	r, _, _ := setupScriptRouter(t)
	doJSON(r, "POST", "/api/scripts", map[string]any{"name": "edit"})

	w := doJSON(r, "POST", "/api/scripts/save", map[string]any{"path": "edit.lua", "content": "print(1)"})
	if w.Code != http.StatusOK {
		t.Fatalf("save: %d %s", w.Code, w.Body.String())
	}
	w = doJSON(r, "POST", "/api/scripts/content", map[string]any{"path": "edit.lua"})
	if w.Code != http.StatusOK {
		t.Fatalf("content: %d", w.Code)
	}
	var resp struct {
		Data string `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Data != "print(1)" {
		t.Errorf("expected print(1), got %q", resp.Data)
	}
}

func TestScriptRunRequiresAgent(t *testing.T) {
	r, _, _ := setupScriptRouter(t) // 无 agent 注册
	w := doJSON(r, "POST", "/api/scripts/run", map[string]any{"path": "x.lua"})
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502 no agent, got %d", w.Code)
	}
}

func TestScriptRunWithMockAgent(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	reg.Register("a1", "h1", "10.0.0.1", &handlerMockConn{responses: []map[string]any{{"success": true}}})

	sh := NewScriptHandler(db, dir, reg)
	r := gin.New()
	r.POST("/run", asAdmin(1), sh.HandleRun)

	w := doJSON(r, "POST", "/run", map[string]any{"path": "demo.lua"})
	if w.Code != http.StatusOK {
		t.Fatalf("run: %d %s", w.Code, w.Body.String())
	}
}

func TestScriptLogs(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	sh := NewScriptHandler(db, dir, reg)

	db.Create(&models.ExecutionLog{ScriptID: "demo", Output: "line1", Level: "info"})
	db.Create(&models.ExecutionLog{ScriptID: "demo", Output: "line2", Level: "warn"})

	r := gin.New()
	r.POST("/logs", asAdmin(1), sh.HandleLogs)
	w := doJSON(r, "POST", "/logs", map[string]any{"executionId": "demo"})
	if w.Code != http.StatusOK {
		t.Fatalf("logs: %d", w.Code)
	}
	var resp struct {
		Data []map[string]any `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Data) != 2 {
		t.Errorf("expected 2 log lines, got %d", len(resp.Data))
	}
}

// ---------- Agent handler ----------

func TestAgentListAndGet(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)
	db := newDB(t)
	ah := NewAgentHandler(reg, db)

	r := gin.New()
	r.GET("/agents", ah.HandleList)
	r.GET("/agents/:agentId", ah.HandleGet)

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/agents", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d", w.Code)
	}

	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/agents/a1", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("get a1: %d", w.Code)
	}

	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/agents/ghost", nil))
	if w.Code != http.StatusNotFound {
		t.Errorf("expected 404 for ghost, got %d", w.Code)
	}
}

func TestAgentShutdown(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	conn := &handlerMockConn{responses: []map[string]any{{"success": true}}}
	reg.Register("a1", "host1", "10.0.0.1", conn)
	db := newDB(t)
	ah := NewAgentHandler(reg, db)

	r := gin.New()
	r.POST("/agents/:agentId/shutdown", asAdmin(1), ah.HandleShutdown)

	w := doJSON(r, "POST", "/agents/a1/shutdown", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("shutdown: %d %s", w.Code, w.Body.String())
	}
	if conn.callCount() != 1 {
		t.Errorf("expected 1 command sent, got %d", conn.callCount())
	}
	// 应更新为 offline
	info, _ := reg.Get("a1")
	if info.Status != agent.StatusOffline {
		t.Errorf("expected agent offline after shutdown, got %s", info.Status)
	}

	// 未连接的 agent → 404
	w = doJSON(r, "POST", "/agents/ghost/shutdown", nil)
	if w.Code != http.StatusNotFound {
		t.Errorf("expected 404 for ghost shutdown, got %d", w.Code)
	}
}

func TestAgentSetTags(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)
	db := newDB(t)
	ah := NewAgentHandler(reg, db)

	r := gin.New()
	r.PUT("/agents/:agentId/tags", asAdmin(1), ah.HandleSetTags)

	w := doJSON(r, "PUT", "/agents/a1/tags", map[string]any{"tags": []string{"prod", "gpu"}})
	if w.Code != http.StatusOK {
		t.Fatalf("set tags: %d %s", w.Code, w.Body.String())
	}
	info, _ := reg.Get("a1")
	if len(info.Tags) != 2 {
		t.Errorf("expected 2 tags, got %+v", info.Tags)
	}

	// 未知 agent → 404
	w = doJSON(r, "PUT", "/agents/ghost/tags", map[string]any{"tags": []string{"x"}})
	if w.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", w.Code)
	}
}

// ---------- Screenshot handler ----------

type mockHub struct {
	mu      sync.Mutex
	events  []string
	lastRaw any
}

func (m *mockHub) BroadcastEvent(eventType string, data interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.events = append(m.events, eventType)
	m.lastRaw = data
}

// fieldFromData 从广播 data（gin.H 或 map[string]any）取字段
func fieldFromData(data any, key string) (any, bool) {
	switch d := data.(type) {
	case map[string]any:
		v, ok := d[key]
		return v, ok
	case gin.H:
		v, ok := d[key]
		return v, ok
	}
	return nil, false
}

func TestScreenshotBroadcast(t *testing.T) {
	gin.SetMode(gin.TestMode)
	hub := &mockHub{}
	sh := NewScreenshotHandler(hub)
	r := gin.New()
	r.POST("/screenshot", sh.HandleScreenshot)

	w := doJSON(r, "POST", "/screenshot", map[string]any{"image": "base64data", "width": 100, "height": 200})
	if w.Code != http.StatusOK {
		t.Fatalf("screenshot: %d %s", w.Code, w.Body.String())
	}
	hub.mu.Lock()
	defer hub.mu.Unlock()
	if len(hub.events) != 1 || hub.events[0] != "screenshot" {
		t.Errorf("expected 1 screenshot broadcast, got %v", hub.events)
	}
	if img, _ := fieldFromData(hub.lastRaw, "image"); img != "base64data" {
		t.Errorf("unexpected broadcast image: %v", img)
	}
}

func TestScreenshotValidation(t *testing.T) {
	gin.SetMode(gin.TestMode)
	hub := &mockHub{}
	sh := NewScreenshotHandler(hub)
	r := gin.New()
	r.POST("/screenshot", sh.HandleScreenshot)

	// 缺 image
	w := doJSON(r, "POST", "/screenshot", map[string]any{"width": 100})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing image: expected 400, got %d", w.Code)
	}
	// 过大
	big := make([]byte, maxScreenshotSize+1)
	for i := range big {
		big[i] = 'A'
	}
	w = doJSON(r, "POST", "/screenshot", map[string]any{"image": string(big)})
	if w.Code != http.StatusBadRequest {
		t.Errorf("oversized: expected 400, got %d", w.Code)
	}
	hub.mu.Lock()
	got := len(hub.events)
	hub.mu.Unlock()
	if got != 0 {
		t.Errorf("no broadcast expected on validation failure, got %d", got)
	}
}

// ---------- Audit handler ----------

func TestAuditListFilters(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	// 写入多条审计
	WriteAuditLog(db, "alice", "login", "auth.login", map[string]any{"ip": "1.1.1.1"})
	WriteAuditLog(db, "bob", "script.run", "script.demo", nil)
	WriteAuditLog(db, "alice", "login_fail", "auth.login", nil)

	ah := NewAuditHandler(db)
	r := gin.New()
	r.GET("/audit", ah.HandleList)

	// 全量
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/audit?page=1&size=10", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("audit list: %d", w.Code)
	}
	var resp struct {
		Total int64 `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 3 {
		t.Errorf("expected total 3, got %d", resp.Total)
	}

	// 按 actor 过滤
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/audit?actor=alice", nil))
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 2 {
		t.Errorf("alice expected 2, got %d", resp.Total)
	}

	// 按 kinds 过滤
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/audit?kinds=login,login_fail", nil))
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 2 {
		t.Errorf("login kinds expected 2, got %d", resp.Total)
	}
}

func TestAuditEmpty(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	ah := NewAuditHandler(db)
	r := gin.New()
	r.GET("/audit", ah.HandleList)

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/audit", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	var resp struct {
		Events []any `json:"events"`
		Total  int64 `json:"total"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Total != 0 || len(resp.Events) != 0 {
		t.Errorf("expected empty, got total=%d events=%d", resp.Total, len(resp.Events))
	}
}

func TestFeedbackCreatesMessagesAndMessageActions(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "alice", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}

	fh := NewFeedbackHandler(db)
	mh := NewMessageHandler(db)
	r := gin.New()
	r.POST("/feedback", asUser(user), fh.HandleCreate)
	r.GET("/messages", asUser(user), mh.HandleList)
	r.GET("/messages/unread-count", asUser(user), mh.HandleUnreadCount)
	r.POST("/messages/:id/read", asUser(user), mh.HandleMarkRead)

	w := doJSON(r, "POST", "/feedback", map[string]any{
		"category": "permission_request",
		"content":  "need scripts:run",
		"priority": "normal",
		"source":   "profile_permission_apply",
	})
	if w.Code != http.StatusCreated {
		t.Fatalf("feedback create: %d %s", w.Code, w.Body.String())
	}

	w = doJSON(r, "GET", "/messages?status=unread", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("message list: %d %s", w.Code, w.Body.String())
	}
	var listResp struct {
		Items []struct {
			ID     uint   `json:"id"`
			Status string `json:"status"`
		} `json:"items"`
		Total int64 `json:"total"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &listResp); err != nil {
		t.Fatalf("decode messages: %v", err)
	}
	if listResp.Total != 2 || len(listResp.Items) != 2 {
		t.Fatalf("expected self + broadcast messages, got %+v", listResp)
	}

	w = doJSON(r, "GET", "/messages/unread-count", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("unread count: %d %s", w.Code, w.Body.String())
	}
	var countResp struct {
		Count int64 `json:"count"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &countResp); err != nil {
		t.Fatalf("decode count: %v", err)
	}
	if countResp.Count != 2 {
		t.Fatalf("expected unread count 2, got %d", countResp.Count)
	}

	w = doJSON(r, "POST", "/messages/"+itoa(listResp.Items[0].ID)+"/read", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("mark read: %d %s", w.Code, w.Body.String())
	}
}

// TestBroadcastMessagePerUserReadState 验证广播消息（recipient='*'）的已读状态
// 在多用户间相互独立：一个用户标记已读不影响其他用户的未读计数。
// 回归测试：修复前共享单行 status 字段会导致已读状态污染。
func TestBroadcastMessagePerUserReadState(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)

	alice := models.User{Username: "alice", Password: "x", Role: "viewer", Active: true}
	bob := models.User{Username: "bob", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&alice).Error; err != nil {
		t.Fatalf("create alice: %v", err)
	}
	if err := db.Create(&bob).Error; err != nil {
		t.Fatalf("create bob: %v", err)
	}

	// 一条广播消息
	broadcast := models.Message{Recipient: "*", Title: "global", Content: "hi", Status: "unread"}
	if err := db.Create(&broadcast).Error; err != nil {
		t.Fatalf("create broadcast: %v", err)
	}

	mh := NewMessageHandler(db)

	// 为两个用户构造独立 engine 以隔离上下文。
	rAlice := gin.New()
	rAlice.GET("/messages/unread-count", asUser(alice), mh.HandleUnreadCount)
	rAlice.POST("/messages/:id/read", asUser(alice), mh.HandleMarkRead)
	rBob := gin.New()
	rBob.GET("/messages/unread-count", asUser(bob), mh.HandleUnreadCount)

	countOf := func(eng *gin.Engine) int64 {
		w := doJSON(eng, "GET", "/messages/unread-count", nil)
		if w.Code != http.StatusOK {
			t.Fatalf("unread count: %d %s", w.Code, w.Body.String())
		}
		var resp struct {
			Count int64 `json:"count"`
		}
		json.Unmarshal(w.Body.Bytes(), &resp)
		return resp.Count
	}

	if c := countOf(rAlice); c != 1 {
		t.Fatalf("alice initial unread = %d, want 1", c)
	}
	if c := countOf(rBob); c != 1 {
		t.Fatalf("bob initial unread = %d, want 1", c)
	}

	// alice 标记已读
	w := doJSON(rAlice, "POST", "/messages/"+itoa(broadcast.ID)+"/read", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("alice mark read: %d %s", w.Code, w.Body.String())
	}

	// alice 现在未读为 0
	if c := countOf(rAlice); c != 0 {
		t.Fatalf("alice after read = %d, want 0", c)
	}
	// bob 仍然未读为 1（修复前会被污染成 0）
	if c := countOf(rBob); c != 1 {
		t.Fatalf("REGRESSION: bob after alice read = %d, want 1 (broadcast read state polluted)", c)
	}
}

// ---------- Workflow handler ----------

func setupWorkflowRouter(t *testing.T) (*gin.Engine, *gorm.DB) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)

	r := gin.New()
	r.GET("/api/workflow-templates", asAdmin(1), wh.HandleListTemplates)
	r.POST("/api/workflows", asAdmin(1), wh.HandleCreate)
	r.GET("/api/workflows/:id", asAdmin(1), wh.HandleGet)
	return r, db
}

func TestWorkflowCreateRejectsInvalidDefinition(t *testing.T) {
	r, db := setupWorkflowRouter(t)

	w := doJSON(r, "POST", "/api/workflows", map[string]any{
		"name": "bad-workflow",
		"steps": []map[string]any{
			{"id": "a", "script": "a.lua", "dependsOn": []string{"b"}},
			{"id": "b", "script": "b.lua", "dependsOn": []string{"a"}},
		},
	})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400 for invalid workflow, got %d %s", w.Code, w.Body.String())
	}

	var count int64
	db.Model(&models.Workflow{}).Count(&count)
	if count != 0 {
		t.Fatalf("invalid workflow should not persist, got %d row(s)", count)
	}
}

func TestWorkflowCreateAcceptsScreenshotAndConditionSteps(t *testing.T) {
	r, db := setupWorkflowRouter(t)

	w := doJSON(r, "POST", "/api/workflows", map[string]any{
		"name": "rich-workflow",
		"steps": []map[string]any{
			{
				"id":   "guard",
				"type": "condition",
				"parameters": map[string]any{
					"value":    true,
					"operator": "truthy",
				},
			},
			{
				"id":        "capture",
				"type":      "screenshot",
				"dependsOn": []string{"guard"},
				"parameters": map[string]any{
					"displayId": 0,
				},
			},
		},
	})
	if w.Code != http.StatusOK {
		t.Fatalf("expected screenshot/condition workflow to be accepted, got %d %s", w.Code, w.Body.String())
	}

	var count int64
	db.Model(&models.Workflow{}).Count(&count)
	if count != 1 {
		t.Fatalf("expected 1 persisted workflow, got %d", count)
	}
}

func TestWorkflowTemplatesIncludeNewStepTypesInDescription(t *testing.T) {
	r, _ := setupWorkflowRouter(t)

	w := doJSON(r, "GET", "/api/workflow-templates", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("templates: %d %s", w.Code, w.Body.String())
	}

	var resp struct {
		Data []struct {
			ID          string `json:"id"`
			Description string `json:"description"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode templates: %v", err)
	}
	if len(resp.Data) == 0 {
		t.Fatal("expected non-empty templates")
	}
}
