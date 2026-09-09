package handlers

import (
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
)

// ---------- Trigger dispatch 分支 ----------

// 直接调用私有 dispatch：param 缺失（agentId 为空）且 registry 无可用 agent →
// 走 "no available agent" 分支。
func TestTriggerDispatchNoAgentParamAndNoAgent(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	th := NewTriggerHandler(reg, newDB(t))

	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)

	resp, ok := th.dispatch(c, "trigger.list", nil, time.Second)
	if ok || resp != nil {
		t.Errorf("dispatch should fail without agents, got resp=%v ok=%v", resp, ok)
	}
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "no available agent") {
		t.Errorf("unexpected body: %s", w.Body.String())
	}
}

// agent 通道传输错误（SendCommandWithTimeout 返回 err）→ 502 透传错误详情。
func TestTriggerDispatchTransportError(t *testing.T) {
	conn := &handlerMockConn{errs: []error{errors.New("link reset")}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "GET", "/agents/a1/triggers", nil)
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "trigger command failed: link reset") {
		t.Errorf("unexpected body: %s", w.Body.String())
	}
}

// runtime 拒绝时 error 字段缺失：先回落 message，再回落通用文案。
func TestTriggerDispatchFailureTextFallbacks(t *testing.T) {
	// 只有 message → 使用 message
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": false,
		"message": "meh",
	}}}
	r := setupTriggerRouter(t, conn)
	w := doJSON(r, "GET", "/agents/a1/triggers", nil)
	if w.Code != http.StatusBadGateway || !strings.Contains(w.Body.String(), "meh") {
		t.Errorf("message fallback: %d %s", w.Code, w.Body.String())
	}

	// error/message 均缺失 → 通用文案
	conn2 := &handlerMockConn{responses: []map[string]any{{
		"success": false,
	}}}
	r2 := setupTriggerRouter(t, conn2)
	w2 := doJSON(r2, "GET", "/agents/a1/triggers", nil)
	if w2.Code != http.StatusBadGateway || !strings.Contains(w2.Body.String(), "trigger command failed") {
		t.Errorf("generic fallback: %d %s", w2.Code, w2.Body.String())
	}
}

// create/update/remove 在 dispatch 失败后应直接返回（不再写审计日志）。
func TestTriggerMutationsOnDispatchFailure(t *testing.T) {
	conn := &handlerMockConn{errs: []error{errors.New("down"), errors.New("down")}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "POST", "/agents/a1/triggers", map[string]any{"name": "t1"})
	if w.Code != http.StatusBadGateway {
		t.Errorf("create dispatch failure: expected 502, got %d", w.Code)
	}

	w = doJSON(r, "DELETE", "/agents/a1/triggers/3", nil)
	if w.Code != http.StatusBadGateway {
		t.Errorf("remove dispatch failure: expected 502, got %d", w.Code)
	}
}

// update 请求体为非法 JSON → 400。
func TestTriggerUpdateMalformedJSON(t *testing.T) {
	r := setupTriggerRouter(t, &handlerMockConn{})

	req := httptest.NewRequest("PUT", "/agents/a1/triggers/3", strings.NewReader("{not-json"))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("malformed update body: expected 400, got %d", w.Code)
	}
}

// ---------- Profile 权限 ----------

// inactive 用户的权限码解析为 nil → 返回空数组而非 null。
func TestProfilePermissionsInactiveUserEmptyCodes(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "sleeper", Password: "x", Role: "viewer", Active: false}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}

	handler := NewProfileHandler(db)
	r := gin.New()
	r.GET("/profile/permissions", asUser(user), handler.HandleGetPermissions)

	w := doJSON(r, "GET", "/profile/permissions", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("permissions: %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"permissions":[]`) {
		t.Errorf("expected empty permissions array, got %s", w.Body.String())
	}
}

// ---------- Users ----------

// 角色校验查询失败（db 已关闭）→ 500 failed to validate role。
func TestUserCreateRoleValidationDBError(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	sqlDB, err := db.DB()
	if err != nil {
		t.Fatal(err)
	}
	if err := sqlDB.Close(); err != nil {
		t.Fatal(err)
	}

	handler := NewUserHandler(db)
	r := gin.New()
	r.POST("/users", asAdmin(1), handler.HandleCreate)

	w := doJSON(r, "POST", "/users", map[string]any{
		"username": "newbie",
		"password": "Str0ng!pw",
		"role":     "viewer",
	})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("expected 500 on db failure, got %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "failed to validate role") {
		t.Errorf("unexpected body: %s", w.Body.String())
	}
}

// ---------- Workflow step status EndTime 快照 ----------

// s1（短 wait）完成后、s2（长 wait）仍在执行时，execution 仍在 running，
// 查询 s1 应同时命中 StartTime 与 EndTime 快照分支。
func TestWorkflowStepStatusEndTimeSnapshot(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)
	r := gin.New()
	r.POST("/workflows", asAdmin(1), wh.HandleCreate)
	r.GET("/workflows/:id/steps/:stepId", asAdmin(1), wh.HandleGetStepStatus)

	w := doJSON(r, "POST", "/workflows", map[string]any{
		"name": "snapshot-demo",
		"steps": []map[string]any{
			{"id": "s1", "type": "wait", "parameters": map[string]any{"seconds": 1}},
			{"id": "s2", "type": "wait", "parameters": map[string]any{"seconds": 30}, "dependsOn": []string{"s1"}},
		},
	})
	if w.Code != http.StatusOK {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	var created struct {
		Data struct {
			WorkflowID uint `json:"workflowId"`
		} `json:"data"`
	}
	readJSON(t, w.Body.Bytes(), &created)
	id := created.Data.WorkflowID

	// 等 s1 完成（EndTime 非空）而 s2 仍运行（execution 仍在 map 中）
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		w = doJSON(r, "GET", "/workflows/"+itoa(id)+"/steps/s1", nil)
		var resp struct {
			Data struct {
				Status string         `json:"status"`
				End    *time.Time     `json:"endTime"`
				Start  *time.Time     `json:"startTime"`
				Extra  map[string]any `json:"-"`
				Raw    map[string]any `json:"-"`
			} `json:"data"`
		}
		readJSON(t, w.Body.Bytes(), &resp)
		if resp.Data.Status == "completed" && resp.Data.End != nil {
			return // 命中 EndTime 分支
		}
		time.Sleep(100 * time.Millisecond)
	}
	t.Fatal("s1 should complete with endTime while workflow still running")
}

// ---------- InitAdmin ----------

// 已有用户时 InitAdmin 不应重复创建；bootstrap 密码缺失时只记录日志。
func TestInitAdminIdempotentAndNoPassword(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatal(err)
	}
	if err := db.Create(&models.User{Username: "admin", Password: hash, Role: "admin", Active: true}).Error; err != nil {
		t.Fatal(err)
	}

	h := NewAuthHandler(db)
	h.InitAdmin() // 已有用户 → 计数非零，直接跳过

	var count int64
	db.Model(&models.User{}).Where("username = ?", "admin").Count(&count)
	if count != 1 {
		t.Errorf("admin should not be duplicated, got %d", count)
	}

	// 空库 + 无 WINGMAN_ADMIN_PASSWORD → 只提示不创建
	db2 := newDB(t)
	t.Setenv("WINGMAN_ADMIN_PASSWORD", "")
	NewAuthHandler(db2).InitAdmin()
	db2.Model(&models.User{}).Where("username = ?", "admin").Count(&count)
	if count != 0 {
		t.Errorf("no admin should be created without password, got %d", count)
	}
}
