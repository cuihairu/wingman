package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// execScriptRouter 同时挂 run_script 写入路径与 executions 只读查询，
// 供执行生命周期（创建 → 终态 → 可查）用例复用。
func execScriptRouter(t *testing.T) (*gin.Engine, *agent.Registry, *gorm.DB, string) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	sh := NewScriptHandler(db, dir, reg)
	eh := NewExecutionHandler(db)

	r := gin.New()
	r.POST("/api/scripts/run", asAdmin(1), sh.HandleRun)
	r.GET("/api/executions", asAdmin(1), eh.HandleList)
	r.GET("/api/executions/:id", asAdmin(1), eh.HandleGet)
	return r, reg, db, dir
}

func writeDemoScript(t *testing.T, dir string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(dir, "demo.lua"), []byte("print('ok')"), 0644); err != nil {
		t.Fatal(err)
	}
}

// run_script 成功 → Execution 记录应创建并在终态落库（running → succeeded +
// finishedAt + Result 摘要），随后列表/详情可查到。
func TestRunScriptWritesExecutionLifecycle(t *testing.T) {
	r, reg, db, dir := execScriptRouter(t)
	writeDemoScript(t, dir)
	reg.Register("a1", "h", "10.0.0.1",
		&handlerMockConn{responses: []map[string]any{{"success": true, "data": map[string]any{"out": "ok"}}}})

	w := doJSON(r, "POST", "/api/scripts/run", map[string]any{"path": "demo.lua"})
	if w.Code != http.StatusOK {
		t.Fatalf("run: expected 200, got %d %s", w.Code, w.Body.String())
	}

	var execs []models.Execution
	if err := db.Find(&execs).Error; err != nil || len(execs) != 1 {
		t.Fatalf("expected exactly 1 execution row, got %d (err=%v)", len(execs), err)
	}
	e := execs[0]
	if e.Status != models.ExecutionSucceeded {
		t.Errorf("status = %s, want %s", e.Status, models.ExecutionSucceeded)
	}
	if e.ExecutionID == "" {
		t.Error("executionId should be generated")
	}
	if e.AgentID != "a1" || e.ScriptPath != filepath.Join(dir, "demo.lua") || e.TimeoutSec != 30 {
		t.Errorf("unexpected execution fields: %+v", e)
	}
	if e.StartedAt == nil || e.FinishedAt == nil || !e.FinishedAt.After(*e.StartedAt) {
		t.Errorf("startedAt/finishedAt not recorded properly: %+v", e)
	}
	if !strings.Contains(e.Result, `"out":"ok"`) {
		t.Errorf("result should embed agent response, got %s", e.Result)
	}

	// 列表可见（status 过滤命中）
	w = doJSON(r, "GET", "/api/executions?status=succeeded", nil)
	var list struct {
		Success bool                   `json:"success"`
		Data    []models.ExecutionView `json:"data"`
		Total   int64                  `json:"total"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &list); err != nil {
		t.Fatalf("decode list: %v", err)
	}
	if !list.Success || list.Total != 1 || len(list.Data) != 1 {
		t.Fatalf("list = success:%v total:%d items:%d", list.Success, list.Total, len(list.Data))
	}
	view := list.Data[0]
	if view.ExecutionID != e.ExecutionID || view.Status != models.ExecutionSucceeded {
		t.Errorf("view fields mismatch: %+v", view)
	}
	if view.Artifacts == nil || len(view.Artifacts) != 0 {
		t.Errorf("artifacts should default to [] for v1, got %v", view.Artifacts)
	}
	respMap, ok := view.Result.(map[string]any)
	if !ok || respMap["status"] != "succeeded" {
		t.Errorf("result view = %v, want {status: succeeded, ...}", view.Result)
	}

	// 详情
	w = doJSON(r, "GET", fmt.Sprintf("/api/executions/%d", view.ID), nil)
	var detail struct {
		Success bool                 `json:"success"`
		Data    models.ExecutionView `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &detail); err != nil {
		t.Fatalf("decode detail: %v", err)
	}
	if !detail.Success || detail.Data.ExecutionID != e.ExecutionID {
		t.Errorf("detail mismatch: %+v", detail.Data)
	}
}

// run_script 失败路径（命令错误 / success=false）→ Execution 落 failed + 错误文本。
func TestRunScriptExecutionFailedTerminalStates(t *testing.T) {
	cases := []struct {
		name string
		conn *handlerMockConn
		want int
	}{
		{"command-error", &handlerMockConn{errs: []error{fmt.Errorf("boom")}}, http.StatusBadGateway},
		{"agent-rejected", &handlerMockConn{responses: []map[string]any{{"success": false, "message": "agent says no"}}}, http.StatusBadGateway},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			r, reg, db, dir := execScriptRouter(t)
			writeDemoScript(t, dir)
			reg.Register("a1", "h", "10.0.0.1", tc.conn)

			w := doJSON(r, "POST", "/api/scripts/run", map[string]any{"path": "demo.lua"})
			if w.Code != tc.want {
				t.Fatalf("expected %d, got %d %s", tc.want, w.Code, w.Body.String())
			}

			var execs []models.Execution
			if err := db.Find(&execs).Error; err != nil || len(execs) != 1 {
				t.Fatalf("expected 1 execution row, got %d (err=%v)", len(execs), err)
			}
			e := execs[0]
			if e.Status != models.ExecutionFailed {
				t.Errorf("status = %s, want %s", e.Status, models.ExecutionFailed)
			}
			if e.FinishedAt == nil || strings.Contains(e.Result, `"status":"succeeded"`) {
				t.Errorf("unexpected terminal record: %+v", e)
			}
			// 错误文本应进入 Result 摘要
			wantText := "boom"
			if tc.name == "agent-rejected" {
				wantText = "agent says no"
			}
			if !strings.Contains(e.Result, wantText) {
				t.Errorf("result should carry error text %q, got %s", wantText, e.Result)
			}

			// failed 状态过滤可查
			w = doJSON(r, "GET", "/api/executions?status=failed", nil)
			if !strings.Contains(w.Body.String(), `"total":1`) {
				t.Errorf("failed filter should hit the record: %s", w.Body.String())
			}
		})
	}
}

// Execution 查询：分页/过滤/详情/错误分支。
func TestExecutionsQueryBranchAndFilters(t *testing.T) {
	r, _, db, _ := execScriptRouter(t)
	now := time.Now()
	for i, st := range []models.ExecutionStatus{models.ExecutionSucceeded, models.ExecutionFailed, models.ExecutionRunning} {
		db.Create(&models.Execution{
			ExecutionID: fmt.Sprintf("exec-%d", i),
			AgentID:     fmt.Sprintf("a%d", i%2+1),
			ScriptPath:  "x.lua",
			Status:      st,
			StartedAt:   &now,
			Result:      fmt.Sprintf(`{"status":%q}`, st),
			Artifacts:   "[]",
		})
	}

	// 全量
	w := doJSON(r, "GET", "/api/executions", nil)
	if !strings.Contains(w.Body.String(), `"total":3`) {
		t.Errorf("all: %s", w.Body.String())
	}
	// agentId / status 过滤
	w = doJSON(r, "GET", "/api/executions?agentId=a1", nil)
	if !strings.Contains(w.Body.String(), `"total":2`) {
		t.Errorf("agentId filter: %s", w.Body.String())
	}
	w = doJSON(r, "GET", "/api/executions?status=running", nil)
	if !strings.Contains(w.Body.String(), `"executionId":"exec-2"`) {
		t.Errorf("status filter: %s", w.Body.String())
	}
	// 非法分页参数收敛到默认
	w = doJSON(r, "GET", "/api/executions?page=0&pageSize=1000", nil)
	if !strings.Contains(w.Body.String(), `"page":1`) || !strings.Contains(w.Body.String(), `"pageSize":20`) {
		t.Errorf("page clamp: %s", w.Body.String())
	}

	// 详情错误分支
	if w = doJSON(r, "GET", "/api/executions/99999", nil); w.Code != http.StatusNotFound {
		t.Errorf("detail 404: got %d", w.Code)
	}
	if w = doJSON(r, "GET", "/api/executions/abc", nil); w.Code != http.StatusBadRequest {
		t.Errorf("detail 400: got %d", w.Code)
	}
	// 未登录 → 401（handler 挂在 AuthRequired 之后，与 routes.go 注册一致）
	r2 := gin.New()
	eh := NewExecutionHandler(db)
	r2.Use(middleware.AuthRequired())
	r2.GET("/api/executions", eh.HandleList)
	if w = doJSON(r2, "GET", "/api/executions", nil); w.Code != http.StatusUnauthorized {
		t.Errorf("unauthenticated: expected 401, got %d", w.Code)
	}
}

// batch run_script 落逐台 Execution（fan-out 后串行落库；offline 未下发不建行，
// 与单发 selectAgent 失败不建行语义一致）。
func TestBatchRunScriptWritesExecutions(t *testing.T) {
	env := setupBatchEnv(t)
	name := env.writeScript(t, "batch.lua")

	env.addAgent("b1")
	b2 := env.addAgent("b2")
	b2.errs = []error{errBoom}
	env.addAgent("b3")
	env.registry.Unregister("b3") // 离线：不下发、不建行

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script",
		map[string]any{"agentIds": []string{"b1", "b2", "b3"}, "path": name})
	if w.Code != http.StatusOK {
		t.Fatalf("batch run: %d %s", w.Code, w.Body.String())
	}

	var rows []models.Execution
	env.db.Order("id").Find(&rows)
	if len(rows) != 2 {
		t.Fatalf("expected 2 execution rows (offline skipped), got %d", len(rows))
	}
	byAgent := map[string]models.Execution{}
	for _, r := range rows {
		byAgent[r.AgentID] = r
	}
	if byAgent["b1"].Status != models.ExecutionSucceeded {
		t.Errorf("b1 should be succeeded, got %+v", byAgent["b1"])
	}
	if byAgent["b2"].Status != models.ExecutionFailed || !strings.Contains(byAgent["b2"].Result, "boom") {
		t.Errorf("b2 should be failed with error text, got %+v", byAgent["b2"])
	}
	if _, ok := byAgent["b3"]; ok {
		t.Error("offline agent should not get an execution row")
	}
}
