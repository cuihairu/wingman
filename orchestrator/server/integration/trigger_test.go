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
