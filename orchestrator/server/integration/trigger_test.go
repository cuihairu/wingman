package integration

import (
	"encoding/json"
	"net/http"
	"testing"
	"time"
)

// TestTriggerListAndToggleE2E 覆盖触发器透传链路：
// Dashboard → GET /agents/:id/triggers → trigger.list 命令到达 runtime →
// 列表返回；POST toggle → trigger.toggle 命令带 id → 状态回传 + 审计；
// 以及 RBAC（viewer 只读）与断连错误语义。
func TestTriggerListAndToggleE2E(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	viewer := env.login(t, "viewer")
	dash := newDashClient(t, env.httpSrv.URL, admin)

	a := newSimAgent(t, env.agentAddr, "it-trig-agent", "trig-host", func(method string, data map[string]any) map[string]any {
		switch method {
		case "trigger.list":
			return map[string]any{
				"success": true,
				"data": map[string]any{
					"triggers": []any{
						map[string]any{
							"id":            "1",
							"name":          "hp-watch",
							"enabled":       true,
							"type":          "ColorFound",
							"condition":     map[string]any{"type": "ColorFound", "value": "#ff0000"},
							"actions":       []any{map[string]any{"type": "RunScript", "value": "heal.lua"}},
							"oneShot":       false,
							"cooldown":      3000,
							"lastTriggered": false,
						},
					},
				},
			}
		case "trigger.toggle":
			return map[string]any{
				"success": true,
				"data":    map[string]any{"enabled": false},
			}
		}
		return map[string]any{"success": true}
	})
	env.waitForAgentStatus(t, admin, "it-trig-agent", "online")
	// 1) 列表：命令到达 runtime，响应透传给 Dashboard
	res := env.do(t, "GET", "/api/agents/it-trig-agent/triggers", admin, nil)
	if res.Status != http.StatusOK {
		t.Fatalf("GET triggers: %d %s", res.Status, res.Body)
	}
	var listResp struct {
		Data []map[string]any `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &listResp); err != nil {
		t.Fatalf("decode triggers: %v", err)
	}
	if len(listResp.Data) != 1 || listResp.Data[0]["name"] != "hp-watch" {
		t.Fatalf("unexpected trigger list: %s", res.Body)
	}
	cmds := a.waitForCommands(t, "trigger.list", 1, 3*time.Second)
	if cmds[0].Method != "trigger.list" {
		t.Errorf("trigger.list command should reach agent, got %+v", cmds[0])
	}

	// 2) toggle：命令带 id 到达 runtime，状态回传 + 审计落库
	res = env.do(t, "POST", "/api/agents/it-trig-agent/triggers/toggle", admin, map[string]any{"id": "1"})
	if res.Status != http.StatusOK {
		t.Fatalf("toggle: %d %s", res.Status, res.Body)
	}
	var toggleResp struct {
		Data struct {
			ID      string `json:"id"`
			Enabled bool   `json:"enabled"`
		} `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &toggleResp); err != nil {
		t.Fatalf("decode toggle: %v", err)
	}
	if toggleResp.Data.ID != "1" || toggleResp.Data.Enabled {
		t.Errorf("unexpected toggle response: %+v", toggleResp.Data)
	}
	toggleCmds := a.waitForCommands(t, "trigger.toggle", 1, 3*time.Second)
	if toggleCmds[0].Data["id"] != "1" {
		t.Errorf("trigger.toggle should carry id=1, got %+v", toggleCmds[0].Data)
	}
	waitAudit := func() bool {
		var count int64
		env.db.Table("audit_logs").Where("kind = ? AND target = ?", "agent.trigger_toggle", "it-trig-agent").Count(&count)
		return count == 1
	}
	waitUntil(t, 3*time.Second, "toggle 审计落库", waitAudit)

	// 3) RBAC：viewer 可读列表，无 agents:manage 不能 toggle
	if res := env.do(t, "GET", "/api/agents/it-trig-agent/triggers", viewer, nil); res.Status != http.StatusOK {
		t.Errorf("viewer read triggers: got %d want 200", res.Status)
	}
	if res := env.do(t, "POST", "/api/agents/it-trig-agent/triggers/toggle", viewer, map[string]any{"id": "1"}); res.Status != http.StatusForbidden {
		t.Errorf("viewer toggle: got %d want 403", res.Status)
	}

	// 4) 断连语义：agent 离线后列表/toggle 返回 502 + 明确错误
	a.close()
	env.waitForAgentStatus(t, admin, "it-trig-agent", "offline")
	res = env.do(t, "GET", "/api/agents/it-trig-agent/triggers", admin, nil)
	if res.Status != http.StatusBadGateway {
		t.Errorf("offline list: got %d want 502", res.Status)
	}
	var errResp struct {
		Error string `json:"error"`
	}
	json.Unmarshal(res.Body, &errResp)
	if errResp.Error == "" {
		t.Errorf("offline list should carry error message, got %s", res.Body)
	}

	// 5) runtime push trigger_fired → dashboard 收到 agent 事件
	a2 := newSimAgent(t, env.agentAddr, "it-trig-agent2", "trig-host2", nil)
	a2.pushEvent("trigger_fired", map[string]any{
		"id": 1, "name": "hp-watch", "triggered": true, "lastTriggerTime": 12345,
	})
	dash.waitForAgentEvent(3*time.Second, "trigger_fired", func(d map[string]any) bool {
		if d["agentId"] != "it-trig-agent2" {
			return false
		}
		nested, _ := d["data"].(map[string]any)
		return nested != nil && nested["name"] == "hp-watch"
	})
}

// TestTriggerCrudE2E 覆盖触发器管理链路：
// POST /agents/:id/triggers → trigger.add（config 原样到达 runtime，返回新 id）；
// PUT /agents/:id/triggers/:triggerId → trigger.update（带 id+config）；
// DELETE → trigger.remove（带 id）；runtime 报 not found → 404；
// RBAC：viewer 无 agents:manage 不能写；非法 id → 400。
func TestTriggerCrudE2E(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	viewer := env.login(t, "viewer")

	var updateConfig, addConfig map[string]any
	var updateID, removeID string
	a := newSimAgent(t, env.agentAddr, "it-trig-crud", "crud-host", func(method string, data map[string]any) map[string]any {
		switch method {
		case "trigger.add":
			addConfig, _ = data["config"].(map[string]any)
			return map[string]any{"success": true, "data": map[string]any{"id": "42"}}
		case "trigger.update":
			updateID, _ = data["id"].(string)
			updateConfig, _ = data["config"].(map[string]any)
			return map[string]any{"success": true}
		case "trigger.remove":
			removeID, _ = data["id"].(string)
			return map[string]any{"success": true}
		case "trigger.update.missing":
			return map[string]any{"success": false, "error": "Trigger not found"}
		}
		return map[string]any{"success": true}
	})
	env.waitForAgentStatus(t, admin, "it-trig-crud", "online")

	// 1) 新增：config 原样到达 runtime，返回 runtime 分配的 id
	config := map[string]any{
		"name":      "boss-alert",
		"cooldown":  5000,
		"condition": map[string]any{"type": "ImageFound", "value": "boss.png"},
		"actions":   []any{map[string]any{"type": "Click", "x": 100, "y": 200}},
	}
	res := env.do(t, "POST", "/api/agents/it-trig-crud/triggers", admin, config)
	if res.Status != http.StatusOK {
		t.Fatalf("create: %d %s", res.Status, res.Body)
	}
	var createResp struct {
		Data struct {
			ID string `json:"id"`
		} `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &createResp); err != nil {
		t.Fatalf("decode create: %v", err)
	}
	if createResp.Data.ID != "42" {
		t.Errorf("create should return runtime id 42, got %s", createResp.Data.ID)
	}
	addCmds := a.waitForCommands(t, "trigger.add", 1, 3*time.Second)
	if addCmds[0].Data["config"] == nil {
		t.Errorf("trigger.add should carry config, got %+v", addCmds[0].Data)
	}
	if addConfig["name"] != "boss-alert" {
		t.Errorf("config should reach runtime intact, got %+v", addConfig)
	}

	// 2) 更新：命令带 id + config
	res = env.do(t, "PUT", "/api/agents/it-trig-crud/triggers/42", admin, map[string]any{"cooldown": 9000})
	if res.Status != http.StatusOK {
		t.Fatalf("update: %d %s", res.Status, res.Body)
	}
	updCmds := a.waitForCommands(t, "trigger.update", 1, 3*time.Second)
	if updCmds[0].Data["id"] != "42" {
		t.Errorf("trigger.update should carry id=42, got %+v", updCmds[0].Data)
	}
	if cooldown, _ := updateConfig["cooldown"].(float64); cooldown != 9000 {
		t.Errorf("update config should reach runtime, got %+v", updateConfig)
	}
	_ = updateID

	// 3) 删除：命令带 id
	res = env.do(t, "DELETE", "/api/agents/it-trig-crud/triggers/42", admin, nil)
	if res.Status != http.StatusOK {
		t.Fatalf("remove: %d %s", res.Status, res.Body)
	}
	rmCmds := a.waitForCommands(t, "trigger.remove", 1, 3*time.Second)
	if rmCmds[0].Data["id"] != "42" {
		t.Errorf("trigger.remove should carry id=42, got %+v", rmCmds[0].Data)
	}
	_ = removeID

	// 4) 非法 id → 400（server 侧拦截，避免 runtime stoull 异常）
	if res := env.do(t, "PUT", "/api/agents/it-trig-crud/triggers/abc", admin, map[string]any{"name": "x"}); res.Status != http.StatusBadRequest {
		t.Errorf("invalid update id: got %d want 400", res.Status)
	}
	if res := env.do(t, "DELETE", "/api/agents/it-trig-crud/triggers/abc", admin, nil); res.Status != http.StatusBadRequest {
		t.Errorf("invalid remove id: got %d want 400", res.Status)
	}

	// 5) RBAC：viewer 无 agents:manage，写操作一律 403
	if res := env.do(t, "POST", "/api/agents/it-trig-crud/triggers", viewer, config); res.Status != http.StatusForbidden {
		t.Errorf("viewer create: got %d want 403", res.Status)
	}
	if res := env.do(t, "PUT", "/api/agents/it-trig-crud/triggers/42", viewer, map[string]any{"name": "x"}); res.Status != http.StatusForbidden {
		t.Errorf("viewer update: got %d want 403", res.Status)
	}
	if res := env.do(t, "DELETE", "/api/agents/it-trig-crud/triggers/42", viewer, nil); res.Status != http.StatusForbidden {
		t.Errorf("viewer delete: got %d want 403", res.Status)
	}

	// 6) not found 语义：runtime 明确报告 not found → 404
	missingAgent := newSimAgent(t, env.agentAddr, "it-trig-missing", "missing-host", func(method string, data map[string]any) map[string]any {
		if method == "trigger.update" || method == "trigger.remove" {
			return map[string]any{"success": false, "error": "Trigger not found"}
		}
		return map[string]any{"success": true}
	})
	_ = missingAgent
	env.waitForAgentStatus(t, admin, "it-trig-missing", "online")
	if res := env.do(t, "PUT", "/api/agents/it-trig-missing/triggers/99", admin, map[string]any{"name": "x"}); res.Status != http.StatusNotFound {
		t.Errorf("update missing trigger: got %d want 404", res.Status)
	}
	if res := env.do(t, "DELETE", "/api/agents/it-trig-missing/triggers/99", admin, nil); res.Status != http.StatusNotFound {
		t.Errorf("remove missing trigger: got %d want 404", res.Status)
	}
}
