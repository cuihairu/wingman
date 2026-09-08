package integration

import (
	"encoding/json"
	"net/http"
	"strings"
	"testing"
	"time"
)

// TestWorkflowSubmitAgentExecuteResultChain 覆盖完整链路：
// Dashboard 提交 DAG 工作流 → 引擎按依赖调度 → agent 顺序执行 run_script →
// 结果回传（成功/失败）→ 步骤状态落库 → WS 进度广播 → script_output 日志持久化。
func TestWorkflowSubmitAgentExecuteResultChain(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	dash := newDashClient(t, env.httpSrv.URL, admin)

	a := newSimAgent(t, env.agentAddr, "it-worker-1", "wf-host", func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": true, "message": "ok"}
	})
	env.waitForAgentStatus(t, admin, "it-worker-1", "online")

	// 1) 提交 s1 → s2（依赖串行）工作流
	wfID := env.createWorkflow(t, admin, "e2e-chain", []map[string]any{
		{"id": "s1", "name": "第一步", "script": "e2e.lua"},
		{"id": "s2", "name": "第二步", "script": "e2e.lua", "dependsOn": []string{"s1"}},
	})

	// 2) 引擎完成执行（agent 回传 success → 步骤 completed → 工作流 completed）
	env.waitForWorkflowStatus(t, admin, wfID, "completed")

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

	// 4) 步骤状态落库：completed + worker 归属
	for _, stepID := range []string{"s1", "s2"} {
		status := env.getStepStatus(t, admin, wfID, stepID)
		if status["status"] != "completed" {
			t.Errorf("步骤 %s 状态: got %v want completed", stepID, status["status"])
		}
		if status["workerId"] != "it-worker-1" {
			t.Errorf("步骤 %s worker: got %v want it-worker-1", stepID, status["workerId"])
		}
	}

	// 5) dashboard 收到 submitted / progress / status_changed 事件链
	dash.waitFor(3*time.Second, "workflow submitted", workflowEvent("submitted", func(inner map[string]any) bool {
		return inner["name"] == "e2e-chain"
	}))
	dash.waitFor(3*time.Second, "s1 completed 进度", workflowEvent("progress", func(inner map[string]any) bool {
		return inner["stepId"] == "s1" && inner["status"] == "completed"
	}))
	dash.waitFor(3*time.Second, "workflow completed 状态", workflowEvent("status_changed", func(inner map[string]any) bool {
		return inner["status"] == "completed" && inner["workflowId"] == float64(wfID)
	}))

	// 6) agent 推送 script_output → 落库 + script/output WS 广播 + 日志 API 可查
	a.pushEvent("script_output", map[string]any{
		"scriptId": "e2e",
		"message":  "hello from agent",
		"level":    "info",
	})
	dash.waitFor(3*time.Second, "script output 广播", func(m wsMsg) bool {
		return m.Type == "script" && m.Data["event"] == "output"
	})
	waitUntil(t, 3*time.Second, "script_output 持久化后可经 logs API 查询", func() bool {
		res := env.do(t, "POST", "/api/scripts/logs", admin, map[string]any{"executionId": "e2e"})
		if res.Status != http.StatusOK {
			return false
		}
		var resp struct {
			Data []struct {
				Level   string `json:"level"`
				Message string `json:"message"`
			} `json:"data"`
		}
		if err := json.Unmarshal(res.Body, &resp); err != nil {
			return false
		}
		return len(resp.Data) == 1 && resp.Data[0].Message == "hello from agent" && resp.Data[0].Level == "info"
	})

	// 7) 失败回传：agent 返回 success=false → 步骤 failed → 工作流 failed
	a.setHandler(func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": false, "message": "boom from agent"}
	})
	failID := env.createWorkflow(t, admin, "e2e-fail", []map[string]any{
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
	dash.waitFor(3*time.Second, "f1 failed 进度", workflowEvent("progress", func(inner map[string]any) bool {
		return inner["stepId"] == "f1" && inner["status"] == "failed"
	}))
}
