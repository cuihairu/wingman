package handlers

import (
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

func readJSON(t *testing.T, data []byte, v any) {
	t.Helper()
	if err := json.Unmarshal(data, v); err != nil {
		t.Fatalf("decode json: %v", err)
	}
}

func hashForTest(password string) (string, error) {
	return security.HashPassword(password)
}

// failingDB 注册指定操作（query/create/update/delete）的失败回调，
// 用于覆盖 handler 的数据库错误分支。
func failingDB(t *testing.T, ops ...string) *gorm.DB {
	t.Helper()
	db := newDB(t)
	force := func(tx *gorm.DB) { tx.AddError(errors.New("forced db failure")) }
	for _, op := range ops {
		switch op {
		case "query":
			db.Callback().Query().Before("gorm:query").Register("test:fail-query", force)
		case "create":
			db.Callback().Create().Before("gorm:create").Register("test:fail-create", force)
		case "update":
			db.Callback().Update().Before("gorm:update").Register("test:fail-update", force)
		case "delete":
			db.Callback().Delete().Before("gorm:delete").Register("test:fail-delete", force)
		}
	}
	return db
}

// ---------- Agent handler 错误分支 ----------

func TestAgentShutdownCommandError(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	reg.Register("a1", "h", "10.0.0.1", &handlerMockConn{errs: []error{errBoom}})
	db := newDB(t)
	ah := NewAgentHandler(reg, db)

	r := gin.New()
	r.POST("/agents/:agentId/shutdown", asAdmin(1), ah.HandleShutdown)
	w := doJSON(r, "POST", "/agents/a1/shutdown", nil)
	if w.Code != http.StatusBadGateway {
		t.Errorf("command error: expected 502, got %d %s", w.Code, w.Body.String())
	}
	// 状态不应被置为 offline（命令失败）
	if info, ok := reg.Get("a1"); ok && info.Status == agent.StatusOffline {
		t.Error("agent should stay online on failed shutdown")
	}
}

func TestAgentSetTagsInvalidBody(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	reg.Register("a1", "h", "10.0.0.1", nil)
	db := newDB(t)
	ah := NewAgentHandler(reg, db)

	r := gin.New()
	r.PUT("/agents/:agentId/tags", asAdmin(1), ah.HandleSetTags)
	req := httptestBody("PUT", "/agents/a1/tags", "not-json")
	w := requestRaw(r, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}
}

// ---------- Audit 时间过滤与查询失败 ----------

func TestAuditListTimeFiltersAndQueryError(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	WriteAuditLog(db, "alice", "login", "auth.login", nil)

	ah := NewAuditHandler(db)
	r := gin.New()
	r.GET("/audit", ah.HandleList)

	totalOf := func(query string) int64 {
		w := httptest.NewRecorder()
		r.ServeHTTP(w, httptest.NewRequest("GET", "/audit"+query, nil))
		if w.Code != http.StatusOK {
			t.Fatalf("audit %s: %d", query, w.Code)
		}
		var resp struct {
			Total int64 `json:"total"`
		}
		readJSON(t, w.Body.Bytes(), &resp)
		return resp.Total
	}

	if got := totalOf("?start=2000-01-01T00:00:00Z&end=2999-01-01T00:00:00Z"); got != 1 {
		t.Errorf("wide window should include event, got %d", got)
	}
	if got := totalOf("?start=2999-01-01T00:00:00Z"); got != 0 {
		t.Errorf("future start should exclude event, got %d", got)
	}
	if got := totalOf("?end=2000-01-01T00:00:00Z"); got != 0 {
		t.Errorf("past end should exclude event, got %d", got)
	}
	if got := totalOf("?start=bad&end=bad"); got != 1 {
		t.Errorf("invalid time params should be ignored, got %d", got)
	}
	if got := totalOf("?page=0&size=-1&kinds=,"); got != 1 {
		t.Errorf("invalid paging should fall back, got %d", got)
	}

	// 查询失败 → 500
	failDB := failingDB(t, "query")
	ahFail := NewAuditHandler(failDB)
	rFail := gin.New()
	rFail.GET("/audit", ahFail.HandleList)
	w := httptest.NewRecorder()
	rFail.ServeHTTP(w, httptest.NewRequest("GET", "/audit", nil))
	if w.Code != http.StatusInternalServerError {
		t.Errorf("query failure: expected 500, got %d", w.Code)
	}
}

// ---------- Feedback 校验与创建失败 ----------

func TestFeedbackValidationAndFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "fbuser", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	fh := NewFeedbackHandler(db)
	r := gin.New()
	r.POST("/feedback", asUser(user), fh.HandleCreate)

	// 非 JSON body → 400
	req := httptestBody("POST", "/feedback", "not-json")
	w := requestRaw(r, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}

	// 空 content → 400
	w = doJSON(r, "POST", "/feedback", map[string]any{"content": "   "})
	if w.Code != http.StatusBadRequest {
		t.Errorf("blank content: expected 400, got %d", w.Code)
	}

	// 缺 content → 400
	w = doJSON(r, "POST", "/feedback", map[string]any{"category": "general"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing content: expected 400, got %d", w.Code)
	}

	// 默认值兜底
	w = doJSON(r, "POST", "/feedback", map[string]any{"content": "hello"})
	if w.Code != http.StatusCreated {
		t.Fatalf("defaults: %d %s", w.Code, w.Body.String())
	}

	// 创建失败 → 500
	failDB := failingDB(t, "create")
	fhFail := NewFeedbackHandler(failDB)
	rFail := gin.New()
	rFail.POST("/feedback", asUser(user), fhFail.HandleCreate)
	w = doJSON(rFail, "POST", "/feedback", map[string]any{"content": "boom"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("create failure: expected 500, got %d", w.Code)
	}
}

// ---------- Messages 错误与零值分支 ----------

func TestMessagesErrorAndZeroUserBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "mzero", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Create(&models.Message{Recipient: "*", Title: "bc", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}
	mh := NewMessageHandler(db)

	// userID == 0：可见性与 readSet 早退
	r0 := gin.New()
	r0.GET("/messages", asUser(models.User{}), mh.HandleList)
	w := doJSON(r0, "GET", "/messages", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("zero-user list: %d", w.Code)
	}

	// markRead 零值直接返回 nil
	if err := mh.markRead(0, 1); err != nil {
		t.Errorf("markRead zero user: %v", err)
	}
	if err := mh.markRead(1, 0); err != nil {
		t.Errorf("markRead zero message: %v", err)
	}

	// 查询失败 → 500（列表）
	failDB := failingDB(t, "query")
	mhFail := NewMessageHandler(failDB)
	rFail := gin.New()
	rFail.GET("/messages", asUser(user), mhFail.HandleList)
	rFail.GET("/unread", asUser(user), mhFail.HandleUnreadCount)
	rFail.POST("/read-all", asUser(user), mhFail.HandleMarkAllRead)
	w = doJSON(rFail, "GET", "/messages", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("list query failure: expected 500, got %d", w.Code)
	}
	w = doJSON(rFail, "POST", "/read-all", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("mark-all query failure: expected 500, got %d", w.Code)
	}
}

// ---------- Roles 查询失败与空 code ----------

func TestRolesErrorBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)

	// 查询失败 → 500
	failDB := failingDB(t, "query")
	rhFail := NewRoleHandler(failDB)
	rFail := gin.New()
	rFail.GET("/roles", rhFail.HandleListRoles)
	rFail.GET("/roles/:code", rhFail.HandleGetRole)
	rFail.GET("/permissions", rhFail.HandleListPermissions)

	w := doJSON(rFail, "GET", "/roles", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("list roles failure: expected 500, got %d", w.Code)
	}
	w = doJSON(rFail, "GET", "/roles/viewer", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("get role failure: expected 500, got %d", w.Code)
	}
	w = doJSON(rFail, "GET", "/permissions", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("list permissions failure: expected 500, got %d", w.Code)
	}

	_ = db
}

// ---------- Users 数据库错误分支 ----------

func TestUsersDBErrorBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// 列表查询失败 → 500
	uhList := NewUserHandler(failingDB(t, "query"))
	r := gin.New()
	r.GET("/users", asAdmin(1), uhList.HandleList)
	if w := doJSON(r, "GET", "/users", nil); w.Code != http.StatusInternalServerError {
		t.Errorf("list failure: expected 500, got %d", w.Code)
	}

	// 单个查询失败（非 NotFound）→ 500
	uhGet := NewUserHandler(failingDB(t, "query"))
	r2 := gin.New()
	r2.GET("/users/:id", asAdmin(1), uhGet.HandleGet)
	if w := doJSON(r2, "GET", "/users/5", nil); w.Code != http.StatusInternalServerError {
		t.Errorf("get failure: expected 500, got %d", w.Code)
	}

	// 更新失败 → 500
	dbOK := newDB(t)
	target := models.User{Username: "errupd", Password: "x", Role: "viewer", Active: true}
	if err := dbOK.Create(&target).Error; err != nil {
		t.Fatal(err)
	}
	failUpd := failingDB(t, "update")
	// 复制数据到失败库
	if err := failUpd.Create(&target).Error; err != nil {
		t.Fatal(err)
	}
	// failUpd 的 create 回调只注册 update，上面的 create 正常。
	// 但重新 Create 已带 ID 的记录会冲突，改为在失败库新建用户。
	uhUpd := NewUserHandler(failUpd)
	r3 := gin.New()
	r3.PUT("/users/:id", asAdmin(1), uhUpd.HandleUpdate)
	w := doJSON(r3, "PUT", "/users/1", map[string]any{"active": false})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("update failure: expected 500, got %d", w.Code)
	}

	// 删除失败 → 500
	failDel := failingDB(t, "delete")
	del := models.User{Username: "errdel", Password: "x", Role: "viewer", Active: true}
	if err := failDel.Create(&del).Error; err != nil {
		t.Fatal(err)
	}
	uhDel := NewUserHandler(failDel)
	r4 := gin.New()
	r4.DELETE("/users/:id", asAdmin(999), uhDel.HandleDelete)
	w = doJSON(r4, "DELETE", "/users/1", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("delete failure: expected 500, got %d", w.Code)
	}

	// 重置密码更新失败 → 500
	failRP := failingDB(t, "update")
	rp := models.User{Username: "errrp", Password: "x", Role: "viewer", Active: true}
	if err := failRP.Create(&rp).Error; err != nil {
		t.Fatal(err)
	}
	uhRP := NewUserHandler(failRP)
	r5 := gin.New()
	r5.POST("/users/:id/reset-password", asAdmin(1), uhRP.HandleResetPassword)
	w = doJSON(r5, "POST", "/users/1/reset-password", map[string]any{"newPassword": "Str0ng!pw"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("reset password failure: expected 500, got %d", w.Code)
	}
}

// ---------- Profile 数据库错误分支 ----------

func TestProfileDBErrorBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)

	failUpd := failingDB(t, "update")
	user := models.User{Username: "perr", Password: "x", Role: "viewer", Active: true}
	if err := failUpd.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	hash, _ := hashForTest("Str0ng!pw")
	pwUser := models.User{Username: "perr2", Password: hash, Role: "viewer", Active: true}
	if err := failUpd.Create(&pwUser).Error; err != nil {
		t.Fatal(err)
	}

	ph := NewProfileHandler(failUpd)
	r := gin.New()
	r.PUT("/profile", asUser(user), ph.HandleUpdateProfile)
	r.PUT("/password", asUser(pwUser), ph.HandleUpdatePassword)

	w := doJSON(r, "PUT", "/profile", map[string]any{"nickname": "boom"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("update profile failure: expected 500, got %d", w.Code)
	}
	w = doJSON(r, "PUT", "/password", map[string]any{"oldPassword": "Str0ng!pw", "newPassword": "Str0ng!new"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("update password failure: expected 500, got %d", w.Code)
	}
}

// ---------- Script 创建写盘失败 ----------

func TestScriptCreateWriteFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, _ := newRegistry(t)
	// 在脚本目录的路径组件上放置一个同名普通文件：MkdirAll 因父组件不是目录
	// 而失败 → 500。（原用 /proc/wingman-test 假设不可写，Windows 上该路径是
	// 当前盘相对路径，MkdirAll 实际会成功。）
	base := t.TempDir()
	blocker := filepath.Join(base, "blocker")
	if err := os.WriteFile(blocker, []byte("x"), 0644); err != nil {
		t.Fatalf("create blocker file: %v", err)
	}
	sh := NewScriptHandler(db, filepath.Join(blocker, "scripts"), reg)
	r := gin.New()
	r.POST("/scripts", asAdmin(1), sh.HandleCreate)
	w := doJSON(r, "POST", "/scripts", map[string]any{"name": "unwritable"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("write failure: expected 500, got %d", w.Code)
	}
}

// ---------- Workflow Submit 失败回滚 ----------

func TestWorkflowCreateSubmitFailureRollback(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t) // 无 agent
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)

	r := gin.New()
	r.POST("/workflows", asAdmin(1), wh.HandleCreate)

	// 非法步骤类型 → validateSteps 失败 → 400 且数据库无残留
	w := doJSON(r, "POST", "/workflows", map[string]any{
		"name":  "rollback-demo",
		"steps": []map[string]any{{"id": "s1", "type": "bogus"}},
	})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("submit failure: expected 400, got %d %s", w.Code, w.Body.String())
	}
	var wfCount, ssCount int64
	db.Model(&models.Workflow{}).Where("name = ?", "rollback-demo").Count(&wfCount)
	db.Model(&models.StepStatus{}).Count(&ssCount)
	if wfCount != 0 || ssCount != 0 {
		t.Errorf("rollback failed: workflows=%d stepStatuses=%d", wfCount, ssCount)
	}
}

func TestWorkflowListQueryFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := failingDB(t, "query")
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)
	r := gin.New()
	r.GET("/workflows", asAdmin(1), wh.HandleList)
	if w := doJSON(r, "GET", "/workflows", nil); w.Code != http.StatusInternalServerError {
		t.Errorf("list failure: expected 500, got %d", w.Code)
	}
}

// ---------- Workflow workers 去重 ----------

func TestWorkflowGetWorkersDedup(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)

	wf := models.Workflow{Name: "workers-demo", Status: "completed"}
	if err := db.Create(&wf).Error; err != nil {
		t.Fatal(err)
	}
	for _, stepID := range []string{"s1", "s2"} {
		start := time.Unix(100, 0)
		end := time.Unix(200, 0)
		if err := db.Create(&models.StepStatus{
			WorkflowID: wf.ID,
			StepID:     stepID,
			Status:     "completed",
			WorkerID:   "worker-9",
			StartTime:  &start,
			EndTime:    &end,
		}).Error; err != nil {
			t.Fatal(err)
		}
	}
	// 空 WorkerID 应被跳过
	if err := db.Create(&models.StepStatus{WorkflowID: wf.ID, StepID: "s3", Status: "pending"}).Error; err != nil {
		t.Fatal(err)
	}

	r := gin.New()
	r.GET("/workflows/:id/workers", asAdmin(1), wh.HandleGetWorkers)
	w := doJSON(r, "GET", "/workflows/1/workers", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("workers: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data []map[string]any `json:"data"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if len(resp.Data) != 1 {
		t.Fatalf("expected 1 deduped worker, got %v", resp.Data)
	}
	if resp.Data[0]["workerId"] != "worker-9" {
		t.Errorf("unexpected worker: %v", resp.Data[0])
	}
}
