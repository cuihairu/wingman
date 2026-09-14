package handlers

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
)

// toJSONMap 的三个分支：marshal 失败 / unmarshal 失败 / 正常往返。
func TestToJSONMapBranches(t *testing.T) {
	// chan 无法被 json.Marshal → 空 map
	if got := toJSONMap(map[string]any{"ch": make(chan int)}); len(got) != 0 {
		t.Errorf("marshal failure should return empty map, got %v", got)
	}
	// JSON 字符串无法 unmarshal 进 map → 空 map
	if got := toJSONMap("plain-string"); len(got) != 0 {
		t.Errorf("unmarshal failure should return empty map, got %v", got)
	}
	// 正常结构体往返
	got := toJSONMap(struct {
		A int    `json:"a"`
		B string `json:"b"`
	}{A: 1, B: "x"})
	if got["a"] != float64(1) || got["b"] != "x" {
		t.Errorf("unexpected roundtrip result: %v", got)
	}
}

// toMap 是 toJSONMap 的薄封装，验证 TriggerConfigRequest 的 JSON 往返。
func TestToMapThinWrapper(t *testing.T) {
	enabled := true
	req := &TriggerConfigRequest{Name: "t1", Enabled: &enabled}
	got := req.toMap()
	if got["name"] != "t1" || got["enabled"] != true {
		t.Errorf("unexpected toMap result: %v", got)
	}
}

// Workflow 创建的序列化失败分支：Steps/SharedContext 含不可 JSON 序列化的值时
// 应返回 500（SetSteps/SetContext 错误路径），且不落库。
func TestWorkflowCreateSerializationFailureBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)

	// SetSteps 失败：步骤参数含 chan
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Request = httptest.NewRequest("POST", "/workflows", nil)
	c.Set("user_id", uint(1))
	c.Set("username", "admin")
	c.Set("role", "admin")
	wh.handleCreateParsed(c, CreateWorkflowRequest{
		Name: "ser-fail-steps",
		Steps: []models.WorkflowStep{{
			ID:         "s1",
			Type:       "wait",
			Parameters: map[string]any{"bad": make(chan int)},
		}},
	})
	if c.Writer.Status() != http.StatusInternalServerError {
		t.Fatalf("SetSteps failure: expected 500, got %d", c.Writer.Status())
	}

	// SetContext 失败：SharedContext 含 chan
	wh.handleCreateParsed(c, CreateWorkflowRequest{
		Name:          "ser-fail-ctx",
		Steps:         []models.WorkflowStep{{ID: "s1", Type: "wait"}},
		SharedContext: map[string]any{"bad": make(chan int)},
	})
	if c.Writer.Status() != http.StatusInternalServerError {
		t.Fatalf("SetContext failure: expected 500, got %d", c.Writer.Status())
	}

	// 数据库无残留
	var wfCount int64
	db.Model(&models.Workflow{}).Where("name IN ?", []string{"ser-fail-steps", "ser-fail-ctx"}).Count(&wfCount)
	if wfCount != 0 {
		t.Errorf("no workflow should be persisted on serialization failure, got %d", wfCount)
	}
}
