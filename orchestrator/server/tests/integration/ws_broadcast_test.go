package integration

import (
	"testing"
	"time"
)

// TestWebSocketEventBroadcast 覆盖 Go server 边界的 WebSocket 事件广播：
// dashboard 连接欢迎消息 → agent 注册/心跳触发 agent 事件广播 →
// agent 事件上报（script_output）转 script 广播 →
// workflow 提交触发 submitted/progress/status_changed 事件链 →
// 心跳超时触发 disconnected 广播。
func TestWebSocketEventBroadcast(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")

	// dashboard WS 连接（newDashClient 内部已等待 connected 欢迎消息）
	dash := newDashClient(t, env.httpSrv.URL, admin)

	// 1) mock agent 注册 → agent connected 广播
	a := newMockAgent(t, env.agentAddr, "mock-agent-ws", "ws-host", nil)
	env.waitForAgentStatus(t, admin, "mock-agent-ws", "online")
	dash.waitForAgentEvent(3*time.Second, "connected", func(d map[string]any) bool {
		return d["agentId"] == "mock-agent-ws" && d["hostname"] == "ws-host" && d["status"] == "online"
	})

	// 2) 心跳 → agent status_changed 广播
	a.heartbeat("busy", map[string]any{"cpu": map[string]any{"usage": 10.0}})
	dash.waitForAgentEvent(3*time.Second, "status_changed", func(d map[string]any) bool {
		return d["agentId"] == "mock-agent-ws" && d["status"] == "busy"
	})

	// 3) agent 上报 script_output → script/output 广播（携带原始 data）
	a.pushEvent("script_output", map[string]any{
		"scriptId": "e2e",
		"message":  "hello from mock agent",
		"level":    "info",
	})
	msg := dash.waitFor(3*time.Second, "script output 广播", func(m wsMsg) bool {
		if m.Type != "script" {
			return false
		}
		event, _ := m.Data["event"].(string)
		if event != "output" {
			return false
		}
		data, _ := m.Data["data"].(map[string]any)
		return data != nil && data["message"] == "hello from mock agent"
	})
	if msg.Data == nil {
		t.Fatalf("script 广播应携带 data")
	}

	// 4) workflow 提交 → submitted / progress / status_changed 事件链广播
	a.setHandler(func(method string, data map[string]any) map[string]any {
		return map[string]any{"success": true}
	})
	wfID := env.createWorkflow(t, admin, "ti2-ws-broadcast", []map[string]any{
		{"id": "w1", "script": "e2e.lua"},
	})

	dash.waitFor(3*time.Second, "workflow submitted", workflowEvent("submitted", func(inner map[string]any) bool {
		return inner["name"] == "ti2-ws-broadcast"
	}))
	dash.waitFor(3*time.Second, "w1 running 进度", workflowEvent("progress", func(inner map[string]any) bool {
		return inner["stepId"] == "w1" && inner["status"] == "running"
	}))
	dash.waitFor(3*time.Second, "w1 completed 进度", workflowEvent("progress", func(inner map[string]any) bool {
		return inner["stepId"] == "w1" && inner["status"] == "completed"
	}))
	completed := dash.waitFor(5*time.Second, "workflow completed 状态", workflowEvent("status_changed", func(inner map[string]any) bool {
		return inner["status"] == "completed" && inner["workflowId"] == float64(wfID)
	}))
	if completed.Type != "workflow" {
		t.Fatalf("status_changed 应为 workflow 类型消息, got %s", completed.Type)
	}

	// 5) 心跳超时 → disconnected(reason=heartbeat_timeout) 广播
	env.registry.SetHeartbeatTimeout(200 * time.Millisecond)
	time.Sleep(350 * time.Millisecond)
	env.registry.CheckHeartbeatsNow()
	dash.waitForAgentEvent(3*time.Second, "disconnected", func(d map[string]any) bool {
		return d["agentId"] == "mock-agent-ws" && d["reason"] == "heartbeat_timeout"
	})
}
