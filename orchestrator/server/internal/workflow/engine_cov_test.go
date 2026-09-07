package workflow

import (
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// Execution 步骤状态并发辅助方法的直接单测。
func TestExecutionStepStateHelpers(t *testing.T) {
	start := time.Unix(1000, 0)
	exec := &Execution{
		StepState: map[string]*models.StepStatus{
			"s1": {StepID: "s1", Status: "running", WorkerID: "w1", StartTime: &start},
			"s2": {StepID: "s2", Status: "failed"},
		},
	}

	// StepSnapshot 命中与未命中
	snap, ok := exec.StepSnapshot("s1")
	if !ok || snap.Status != "running" || snap.WorkerID != "w1" {
		t.Fatalf("unexpected snapshot: %+v ok=%v", snap, ok)
	}
	if snap.StartTime == nil || !snap.StartTime.Equal(start) {
		t.Errorf("snapshot should carry StartTime: %+v", snap.StartTime)
	}
	if _, ok := exec.StepSnapshot("ghost"); ok {
		t.Error("unknown step should not snapshot")
	}

	// 值拷贝：修改 snapshot 不影响内部状态
	snap.Status = "hacked"
	if exec.StepState["s1"].Status == "hacked" {
		t.Fatal("snapshot must be a copy")
	}

	// stepStatusIn 命中/未命中/未知步骤
	if !exec.stepStatusIn("s1", "running", "completed") {
		t.Error("s1 should match running")
	}
	if exec.stepStatusIn("s1", "failed") {
		t.Error("s1 should not match failed")
	}
	if exec.stepStatusIn("ghost", "running") {
		t.Error("ghost should never match")
	}

	// anyStepStatus
	if !exec.anyStepStatus("failed") {
		t.Error("should find failed step")
	}
	if exec.anyStepStatus("pending") {
		t.Error("no pending steps expected")
	}

	// setStepState 命中/未知步骤（不 panic）
	exec.setStepState("s2", func(s *models.StepStatus) {
		s.Status = "completed"
		s.Message = "done"
	})
	if snap, _ := exec.StepSnapshot("s2"); snap.Status != "completed" || snap.Message != "done" {
		t.Errorf("setStepState not applied: %+v", snap)
	}
	exec.setStepState("ghost", func(s *models.StepStatus) {
		t.Error("ghost update must not run")
	})

	// stepStateRef
	if ref := exec.stepStateRef("s1"); ref == nil || ref.StepID != "s1" {
		t.Errorf("stepStateRef: %+v", ref)
	}
	if ref := exec.stepStateRef("ghost"); ref != nil {
		t.Errorf("stepStateRef ghost should be nil, got %+v", ref)
	}
}
