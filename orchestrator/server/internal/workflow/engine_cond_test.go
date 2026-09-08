package workflow

import (
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// condition 步骤评估为 false 时应走 failStep 并携带 evaluateCondition 的文案。
func TestConditionStepFalseDefaultsMessage(t *testing.T) {
	engine, _, db := newTestEngine(t)

	wf := &models.Workflow{
		Name:   "cond-false-default",
		Status: "pending",
	}
	if err := wf.SetSteps([]models.WorkflowStep{
		{ID: "c1", Type: "condition", Parameters: map[string]any{"value": false}},
	}); err != nil {
		t.Fatal(err)
	}
	if err := db.Create(wf).Error; err != nil {
		t.Fatal(err)
	}
	if err := engine.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}

	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		var stored models.Workflow
		if err := db.First(&stored, wf.ID).Error; err == nil {
			if stored.Status == "failed" {
				break
			}
		}
		time.Sleep(50 * time.Millisecond)
	}
	var ss models.StepStatus
	if err := db.Where("workflow_id = ? AND step_id = ?", wf.ID, "c1").First(&ss).Error; err != nil {
		t.Fatalf("step status: %v", err)
	}
	if ss.Status != "failed" {
		t.Fatalf("expected failed step, got %s", ss.Status)
	}
	if ss.Message != "condition failed: truthy" {
		t.Errorf("expected operator message, got %q", ss.Message)
	}
	if ss.EndTime == nil {
		t.Error("failed step should record endTime")
	}
}
