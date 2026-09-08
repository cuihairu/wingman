package handlers

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
)

// findByCodeParam：路由参数为纯空白（TrimSpace 后为空）且无 :id 兜底 → 400。
func TestFindByCodeParamBlankCode(t *testing.T) {
	gin.SetMode(gin.TestMode)
	rh := NewRoleHandler(newDB(t))

	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Params = gin.Params{{Key: "code", Value: "   "}}
	_, ok := rh.findByCodeParam(c)
	if ok {
		t.Fatal("blank code should not resolve")
	}
	if c.Writer.Status() != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", c.Writer.Status())
	}

	// :id 形式路由的空白兜底
	c2, _ := gin.CreateTestContext(httptest.NewRecorder())
	c2.Params = gin.Params{{Key: "id", Value: "  "}}
	_, ok = rh.findByCodeParam(c2)
	if ok {
		t.Fatal("blank id should not resolve")
	}
	if c2.Writer.Status() != http.StatusBadRequest {
		t.Errorf("expected 400 for blank id, got %d", c2.Writer.Status())
	}
}

// HandleGetStepStatus：运行中步骤应带 startTime，完成后应带 endTime
// （来自 execution 内存快照，覆盖 state.StartTime/EndTime 非 nil 分支）。
func TestGetStepStatusRunningTimes(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)

	r := gin.New()
	r.POST("/workflows", asAdmin(1), wh.HandleCreate)
	r.GET("/workflows/:id/steps/:stepId", asAdmin(1), wh.HandleGetStepStatus)

	w := doJSON(r, "POST", "/workflows", map[string]any{
		"name":  "times-demo",
		"steps": []map[string]any{{"id": "w1", "type": "wait", "parameters": map[string]any{"seconds": 2}}},
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

	fetchStatus := func() map[string]any {
		w := doJSON(r, "GET", "/workflows/"+itoa(id)+"/steps/w1", nil)
		if w.Code != http.StatusOK {
			t.Fatalf("step status: %d %s", w.Code, w.Body.String())
		}
		var resp struct {
			Data map[string]any `json:"data"`
		}
		readJSON(t, w.Body.Bytes(), &resp)
		return resp.Data
	}

	// running 期间：startTime 已设置、endTime 为空
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		if fetchStatus()["status"] == "running" {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if st := fetchStatus(); st["status"] == "running" {
		if st["startTime"] == nil {
			t.Errorf("running step should expose startTime: %v", st)
		}
		if st["endTime"] != nil {
			t.Errorf("running step should not expose endTime: %v", st)
		}
	} else {
		t.Fatalf("workflow did not reach running in time: %v", fetchStatus())
	}

	// 完成后：endTime 已设置
	deadline = time.Now().Add(4 * time.Second)
	for time.Now().Before(deadline) {
		if fetchStatus()["status"] == "completed" {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if st := fetchStatus(); st["status"] == "completed" {
		if st["endTime"] == nil {
			t.Errorf("completed step should expose endTime: %v", st)
		}
	} else {
		t.Fatalf("workflow did not complete in time: %v", fetchStatus())
	}
}
