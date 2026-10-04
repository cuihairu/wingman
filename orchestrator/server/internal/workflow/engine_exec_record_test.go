package workflow

import (
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// waitWorkflowDone 轮询等待工作流到达终态。
func waitWorkflowDone(t *testing.T, db *gorm.DB, wfID uint, want string) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	var status string
	for time.Now().Before(deadline) {
		var got models.Workflow
		db.First(&got, wfID)
		status = got.Status
		if status == "completed" || status == "failed" || status == "cancelled" {
			break
		}
		time.Sleep(20 * time.Millisecond)
	}
	if status != want {
		t.Fatalf("workflow status = %s, want %s", status, want)
	}
}

// script 步骤每轮下发落一条 Execution（ADR: Execution as the Platform Core
// Object）：成功行带 WorkflowID/StepID/AgentID 挂载与起止时间。
func TestWorkflowStepWritesExecutionSuccess(t *testing.T) {
	e, reg, db := newTestEngine(t)
	writeScript(t, e, "a.lua")
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{{"success": true}}})

	wf := &models.Workflow{Name: "exec-ok"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 5}})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}
	waitWorkflowDone(t, db, wf.ID, "completed")

	var execs []models.Execution
	db.Where("workflow_id = ?", wf.ID).Find(&execs)
	if len(execs) != 1 {
		t.Fatalf("expected 1 execution row, got %d", len(execs))
	}
	ex := execs[0]
	if ex.Status != models.ExecutionSucceeded {
		t.Errorf("status = %s, want %s", ex.Status, models.ExecutionSucceeded)
	}
	if ex.ExecutionID == "" || ex.StepID != "s1" || ex.AgentID != "a1" {
		t.Errorf("unexpected execution identity: %+v", ex)
	}
	if !strings.HasSuffix(ex.ScriptPath, "a.lua") {
		t.Errorf("scriptPath = %s, want suffix a.lua", ex.ScriptPath)
	}
	// finished ≥ started（非严格大于）：mock 下发瞬时完成时两者可能落在
	// Windows 时钟同一 tick（实测 started==finished 精确相等），断言起点是
	// 两个字段都已记录且顺序不倒挂。
	if ex.StartedAt == nil || ex.FinishedAt == nil || ex.FinishedAt.Before(*ex.StartedAt) {
		t.Errorf("startedAt/finishedAt not recorded properly: %+v", ex)
	}
}

// 重试场景每轮 attempt 各落一条：首轮 failed（带错误文本）、重试轮 succeeded。
func TestWorkflowStepRetriesWritePerAttemptExecutions(t *testing.T) {
	e, reg, db := newTestEngine(t)
	writeScript(t, e, "flaky.lua")
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{
		{"success": false, "message": "transient"},
		{"success": true},
	}})

	wf := &models.Workflow{Name: "exec-retry"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "s1", Script: "flaky.lua", TimeoutSeconds: 5, MaxRetries: 2},
	})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}
	waitWorkflowDone(t, db, wf.ID, "completed")

	var execs []models.Execution
	db.Where("workflow_id = ?", wf.ID).Order("id").Find(&execs)
	if len(execs) != 2 {
		t.Fatalf("expected 2 execution rows (failed attempt + retry), got %d", len(execs))
	}
	if execs[0].Status != models.ExecutionFailed || !strings.Contains(execs[0].Result, "transient") {
		t.Errorf("attempt 1 should be failed with error text, got %+v", execs[0])
	}
	if execs[1].Status != models.ExecutionSucceeded {
		t.Errorf("attempt 2 should be succeeded, got %+v", execs[1])
	}
}

// 下发超时 → Execution 落 timeout 终态（errStepTimeout 哨兵驱动）。
func TestWorkflowStepTimeoutWritesTimeoutExecution(t *testing.T) {
	e, reg, db := newTestEngine(t)
	writeScript(t, e, "slow.lua")
	registerAgent(reg, "a1", &mockConn{
		responses: []map[string]any{{"success": true}},
		delay:     1500 * time.Millisecond,
	})

	wf := &models.Workflow{Name: "exec-timeout"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "slow.lua", TimeoutSeconds: 1}})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}
	waitWorkflowDone(t, db, wf.ID, "failed")

	var execs []models.Execution
	db.Where("workflow_id = ?", wf.ID).Find(&execs)
	if len(execs) != 1 {
		t.Fatalf("expected 1 execution row, got %d", len(execs))
	}
	ex := execs[0]
	if ex.Status != models.ExecutionTimeout {
		t.Errorf("status = %s, want %s", ex.Status, models.ExecutionTimeout)
	}
	if ex.TimeoutSec != 1 {
		t.Errorf("timeoutSec = %d, want 1", ex.TimeoutSec)
	}
	if !strings.Contains(ex.Result, "timed out") {
		t.Errorf("result should carry timeout text, got %s", ex.Result)
	}
}
