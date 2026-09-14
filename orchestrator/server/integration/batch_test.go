package integration

import (
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// simOK 命令统一成功应答
func simOK(method string, data map[string]any) map[string]any {
	return map[string]any{"success": true}
}

// batchSummary 批量响应解析
type batchSummary struct {
	Total     int `json:"total"`
	Succeeded int `json:"succeeded"`
	Failed    int `json:"failed"`
	Results   []struct {
		AgentID string `json:"agentId"`
		Success bool   `json:"success"`
		Error   string `json:"error"`
	} `json:"results"`
}

func decodeBatchSummary(t *testing.T, body []byte) (bool, batchSummary) {
	t.Helper()
	var resp struct {
		Success bool         `json:"success"`
		Data    batchSummary `json:"data"`
		Error   string       `json:"error"`
	}
	if err := json.Unmarshal(body, &resp); err != nil {
		t.Fatalf("decode batch summary: %v (%s)", err, body)
	}
	return resp.Success, resp.Data
}

// 端到端：打标（持久化）→ 按标签批量运行（含失败台）→ 批量下发触发器 → RBAC。
func TestIntegrationBatchRunScriptByTags(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	operator := env.login(t, "operator")
	viewer := env.login(t, "viewer")

	// 三台 sim agent；B 台对 run_script 上报失败
	handlerB := func(method string, data map[string]any) map[string]any {
		if method == "run_script" {
			return map[string]any{"success": false, "message": "script busy"}
		}
		return map[string]any{"success": true}
	}
	a := newSimAgent(t, env.agentAddr, "it-batch-a", "host-a", simOK)
	b := newSimAgent(t, env.agentAddr, "it-batch-b", "host-b", handlerB)
	c := newSimAgent(t, env.agentAddr, "it-batch-c", "host-c", simOK)

	// 打标：a/c → it-prod，b → it-dev（同时回归标签持久化：models.Agent 行应落库）
	for id, tags := range map[string][]string{
		"it-batch-a": {"it-prod"},
		"it-batch-b": {"it-dev"},
		"it-batch-c": {"it-prod", "edge"},
	} {
		res := env.do(t, "PUT", "/api/agents/"+id+"/tags", admin, map[string]any{"tags": tags})
		if res.Status != http.StatusOK {
			t.Fatalf("set tags %s: %d %s", id, res.Status, res.Body)
		}
	}
	waitUntil(t, 3*time.Second, "标签落库", func() bool {
		var count int64
		env.db.Model(&models.Agent{}).Where("tags LIKE ?", "%it-prod%").Count(&count)
		return count == 2
	})

	// 写脚本文件（批量 run 前服务端要 Resolve）
	if err := os.WriteFile(filepath.Join(env.scriptsDir, "it_batch.lua"), []byte("-- it"), 0644); err != nil {
		t.Fatalf("write script: %v", err)
	}

	// RBAC：viewer 无 scripts:run → 403
	res := env.do(t, "POST", "/api/agents/batch/run-script", viewer, map[string]any{
		"tags": []string{"it-prod"},
		"path": "it_batch.lua",
	})
	if res.Status != http.StatusForbidden {
		t.Fatalf("viewer batch run: expected 403, got %d %s", res.Status, res.Body)
	}

	// operator（内置角色含 scripts:run）→ 200
	res = env.do(t, "POST", "/api/agents/batch/run-script", operator, map[string]any{
		"tags": []string{"it-prod"},
		"path": "it_batch.lua",
	})
	if res.Status != http.StatusOK {
		t.Fatalf("operator batch run: %d %s", res.Status, res.Body)
	}
	ok, summary := decodeBatchSummary(t, res.Body)
	if !ok || summary.Total != 2 || summary.Succeeded != 2 || summary.Failed != 0 {
		t.Fatalf("unexpected summary: %+v", summary)
	}

	// a/c 都应收到 run_script 命令
	a.waitForCommands(t, "run_script", 1, 5*time.Second)
	c.waitForCommands(t, "run_script", 1, 5*time.Second)

	// b（不在选择器内）不应收到
	if len(b.commandsOf("run_script")) != 0 {
		t.Errorf("it-batch-b should not receive run_script")
	}

	// 失败台语义：按 tags 只选 it-prod，全部成功；再对 it-dev 单独验证失败文案
	res = env.do(t, "POST", "/api/agents/batch/run-script", admin, map[string]any{
		"agentIds": []string{"it-batch-b"},
		"path":     "it_batch.lua",
	})
	ok, summary = decodeBatchSummary(t, res.Body)
	if !ok || summary.Succeeded != 0 || summary.Failed != 1 {
		t.Fatalf("b should fail: %+v", summary)
	}
	if summary.Results[0].Error != "script busy" {
		t.Errorf("error should propagate runtime message, got %q", summary.Results[0].Error)
	}

	// 审计落库
	var runLogs int64
	env.db.Model(&models.AuditLog{}).Where("kind = ?", "script.batch_run").Count(&runLogs)
	if runLogs < 2 {
		t.Errorf("expected >=2 batch_run audit logs, got %d", runLogs)
	}
}

func TestIntegrationBatchTrigger(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	viewer := env.login(t, "viewer")

	sim := newSimAgent(t, env.agentAddr, "it-batch-trig", "trig-host", simOK)
	_ = sim

	res := env.do(t, "PUT", "/api/agents/it-batch-trig/tags", admin, map[string]any{"tags": []string{"it-trig"}})
	if res.Status != http.StatusOK {
		t.Fatalf("set tags: %d %s", res.Status, res.Body)
	}

	// viewer 无 agents:manage → 403
	res = env.do(t, "POST", "/api/agents/batch/trigger", viewer, map[string]any{
		"tags": []string{"it-trig"},
		"name": "it-batch-trigger",
	})
	if res.Status != http.StatusForbidden {
		t.Fatalf("viewer batch trigger: expected 403, got %d %s", res.Status, res.Body)
	}

	res = env.do(t, "POST", "/api/agents/batch/trigger", admin, map[string]any{
		"tags":      []string{"it-trig"},
		"name":      "it-batch-trigger",
		"condition": map[string]any{"type": "TimeElapsed", "value": "5000"},
	})
	if res.Status != http.StatusOK {
		t.Fatalf("batch trigger: %d %s", res.Status, res.Body)
	}
	ok, summary := decodeBatchSummary(t, res.Body)
	if !ok || summary.Succeeded != 1 {
		t.Fatalf("unexpected summary: %+v", summary)
	}

	// sim 收到的命令携带 config
	cmds := sim.waitForCommands(t, "trigger.add", 1, 5*time.Second)
	if len(cmds) != 1 {
		t.Fatalf("expected one trigger.add, got %d", len(cmds))
	}
	config, _ := cmds[0].Data["config"].(map[string]any)
	if config == nil || config["name"] != "it-batch-trigger" {
		t.Errorf("command should carry flattened config, got %v", cmds[0].Data)
	}

	var log models.AuditLog
	if err := env.db.Where("kind = ?", "agent.batch_trigger_add").First(&log).Error; err != nil {
		t.Fatalf("audit missing: %v", err)
	}
}
