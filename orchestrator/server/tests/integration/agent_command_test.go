package integration

import (
	"encoding/json"
	"net/http"
	"strings"
	"testing"
	"time"
)

// TestAgentRegisterHeartbeatCommandResultChain 覆盖 Agent 完整控制链路：
// mock runtime outbound 连接 → agent.register/register_ack → 注册表在线 →
// agent.heartbeat 携带资源上报 → PING/PONG 保活 →
// Dashboard API 触发命令下发（run_script Request 帧）→ agent 回写
// Response 帧（结果回传）→ 成功/失败分支映射到 HTTP 响应。
func TestAgentRegisterHeartbeatCommandResultChain(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")

	a := newMockAgent(t, env.agentAddr, "mock-agent-1", "mock-host-1", func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": true, "data": map[string]any{"executionId": "demo"}}
	})

	// 1) 注册 ack 后：agent 出现在注册表且状态 online
	env.waitForAgentStatus(t, admin, "mock-agent-1", "online")
	registered := env.getAgentJSON(t, admin, "mock-agent-1")
	if registered["hostname"] != "mock-host-1" {
		t.Errorf("注册后 hostname: got %v want mock-host-1", registered["hostname"])
	}

	// 2) 心跳携带资源 → 状态 busy、CPU 资源可见（LastSeen 刷新）
	a.heartbeat("busy", map[string]any{
		"cpu":    map[string]any{"usage": 42.5, "cores": 8, "model": "test-cpu"},
		"memory": map[string]any{"total": 16000000000, "usage": 55.0},
	})
	var busyData map[string]any
	waitUntil(t, 3*time.Second, "心跳后 agent 状态变为 busy 且资源可见", func() bool {
		busyData = env.getAgentJSON(t, admin, "mock-agent-1")
		if busyData["status"] != "busy" {
			return false
		}
		resources, _ := busyData["resources"].(map[string]any)
		if resources == nil {
			return false
		}
		cpu, _ := resources["cpu"].(map[string]any)
		return cpu != nil && cpu["usage"] == 42.5
	})

	// 3) PING 保活 → 服务端回 PONG
	a.ping()
	a.waitForNotify(t, "PONG", 2*time.Second)

	// 4) 心跳恢复 online（scripts/run 的 agent 选择要求 online 状态）
	a.heartbeat("online", nil)
	env.waitForAgentStatus(t, admin, "mock-agent-1", "online")

	// 5) 命令下发 + 结果回传（成功分支）：
	//    POST /api/scripts/run → FrameListener SendCommand(run_script) →
	//    mock agent 收到 Request 帧 → 回 success → HTTP 200
	res := env.do(t, "POST", "/api/scripts/run", admin, map[string]any{
		"path":    "demo.lua",
		"agentId": "mock-agent-1",
	})
	if res.Status != http.StatusOK {
		t.Fatalf("scripts/run: got %d want 200 (%s)", res.Status, res.Body)
	}
	var runResp struct {
		Data struct {
			ExecutionID string `json:"executionId"`
			Agent       map[string]any `json:"agent"`
		} `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &runResp); err != nil {
		t.Fatalf("decode scripts/run response: %v (%s)", err, res.Body)
	}
	if runResp.Data.ExecutionID != "demo" {
		t.Errorf("executionId: got %q want demo", runResp.Data.ExecutionID)
	}
	if ok, _ := runResp.Data.Agent["success"].(bool); !ok {
		t.Errorf("agent 回传结果应透传给 dashboard: %v", runResp.Data.Agent)
	}

	// 6) 命令内容校验：run_script 命中 mock agent，path 为 scripts 目录下的绝对路径
	cmds := a.waitForCommands(t, "run_script", 1, 3*time.Second)
	if len(cmds) > 1 {
		t.Errorf("run_script 命令数: got %d want 1", len(cmds))
	}
	cmd := cmds[0]
	path, _ := cmd.Data["path"].(string)
	if !strings.HasSuffix(path, "demo.lua") || !strings.HasPrefix(path, env.scriptsDir) {
		t.Errorf("run_script path 应为 scripts 目录下的 demo.lua, got %q", path)
	}

	// 7) 结果回传（失败分支）：agent 回 success=false + error → HTTP 502 + 错误透传
	a.setHandler(func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": false, "error": "boom from mock agent"}
	})
	res = env.do(t, "POST", "/api/scripts/run", admin, map[string]any{
		"path":    "demo.lua",
		"agentId": "mock-agent-1",
	})
	if res.Status != http.StatusBadGateway {
		t.Fatalf("scripts/run 失败分支: got %d want 502 (%s)", res.Status, res.Body)
	}
	var errResp struct {
		Error string `json:"error"`
	}
	if err := json.Unmarshal(res.Body, &errResp); err != nil || errResp.Error != "boom from mock agent" {
		t.Errorf("失败分支应透传 agent 错误: %s", res.Body)
	}

	// 8) 指定不在线的 agent → 502 specified agent not found
	res = env.do(t, "POST", "/api/scripts/run", admin, map[string]any{
		"path":    "demo.lua",
		"agentId": "ghost-agent",
	})
	if res.Status != http.StatusBadGateway {
		t.Errorf("指定不存在 agent: got %d want 502", res.Status)
	}
}
