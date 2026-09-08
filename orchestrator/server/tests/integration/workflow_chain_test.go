package integration

import (
	"strings"
	"testing"
	"time"
)

// TestWorkflowSubmitToAgentResponseCompletion 覆盖 Workflow 完整链路：
// Dashboard 提交 DAG 工作流 → 引擎按依赖调度 → run_script 命令下发到 mock agent →
// agent 回传 success（响应）→ 步骤状态落库（completed + worker 归属）→
// 工作流终态 completed；失败响应 → 步骤 failed → 工作流 failed。
func TestWorkflowSubmitToAgentResponseCompletion(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")

	a := newMockAgent(t, env.agentAddr, "mock-worker-1", "wf-host", func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": true, "message": "ok"}
	})
	env.waitForAgentStatus(t, admin, "mock-worker-1", "online")

	// 1) 提交 s1 → s2（依赖串行）工作流
	wfID := env.createWorkflow(t, admin, "ti2-e2e-chain", []map[string]any{
		{"id": "s1", "name": "第一步", "script": "e2e.lua"},
		{"id": "s2", "name": "第二步", "script": "e2e.lua", "dependsOn": []string{"s1"}},
	})

	// 2) 引擎完成执行（agent 回传 success → 步骤 completed → 工作流 completed）
	final := env.waitForWorkflowStatus(t, admin, wfID, "completed")
	if endTime, _ := final["endTime"].(string); endTime == "" {
		t.Errorf("完成的工作流应带 endTime, got %v", final["endTime"])
	}

	// 3) 两条 run_script 命令按依赖顺序到达 agent
	cmds := a.waitForCommands(t, "run_script", 2, 3*time.Second)
	if len(cmds) > 2 {
		t.Errorf("run_script 命令数: got %d want 2", len(cmds))
	}
	for i, c := range cmds {
		path, _ := c.Data["path"].(string)
		if !strings.HasSuffix(path, "e2e.lua") || !strings.HasPrefix(path, env.scriptsDir) {
			t.Errorf("cmd[%d] path 应为 scripts 目录下的 e2e.lua, got %q", i, path)
		}
		if _, ok := c.Data["timeout"]; !ok {
			t.Errorf("cmd[%d] run_script 应携带 timeout", i)
		}
	}
	if !cmds[0].At.Before(cmds[1].At) {
		t.Errorf("s2 的命令不应早于 s1: s1@%v s2@%v", cmds[0].At, cmds[1].At)
	}

	// 4) 步骤状态落库：completed + worker 归属 + 起止时间
	for _, stepID := range []string{"s1", "s2"} {
		status := env.getStepStatus(t, admin, wfID, stepID)
		if status["status"] != "completed" {
			t.Errorf("步骤 %s 状态: got %v want completed", stepID, status["status"])
		}
		if status["workerId"] != "mock-worker-1" {
			t.Errorf("步骤 %s worker: got %v want mock-worker-1", stepID, status["workerId"])
		}
		if startTime := status["startTime"]; startTime == nil {
			t.Errorf("步骤 %s 应记录 startTime", stepID)
		}
		if endTime := status["endTime"]; endTime == nil {
			t.Errorf("步骤 %s 应记录 endTime", stepID)
		}
	}

	// 5) 失败响应：agent 返回 success=false → 步骤 failed → 工作流 failed
	a.setHandler(func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": false, "message": "boom from agent"}
	})
	failID := env.createWorkflow(t, admin, "ti2-e2e-fail", []map[string]any{
		{"id": "f1", "script": "e2e.lua"},
	})
	env.waitForWorkflowStatus(t, admin, failID, "failed")
	failed := env.getStepStatus(t, admin, failID, "f1")
	if failed["status"] != "failed" {
		t.Errorf("f1 步骤状态: got %v want failed", failed["status"])
	}
	if msg, _ := failed["message"].(string); !strings.Contains(msg, "boom from agent") {
		t.Errorf("f1 失败消息应包含 agent 回传错误, got %q", msg)
	}
}
