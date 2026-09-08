package integration

import (
	"testing"
	"time"
)

// TestAgentLifecycleRegisterHeartbeatTimeout 覆盖完整生命周期：
// 注册→在线广播→心跳带资源→PING/PONG→持续心跳保持在线→
// 停止心跳→超时离线广播→心跳恢复→连接断开→注销广播。
func TestAgentLifecycleRegisterHeartbeatTimeout(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	dash := newDashClient(t, env.httpSrv.URL, admin)

	// 缩短心跳超时窗口（默认 90s 不适合测试）
	env.registry.SetHeartbeatTimeout(250 * time.Millisecond)

	a := newSimAgent(t, env.agentAddr, "it-agent-1", "it-host-1", nil)

	// 1) 注册 ack 后：agent 列表在线 + dashboard 收到 connected 广播
	env.waitForAgentStatus(t, admin, "it-agent-1", "online")
	dash.waitForAgentEvent(3*time.Second, "connected", func(d map[string]any) bool {
		return d["agentId"] == "it-agent-1" && d["hostname"] == "it-host-1"
	})

	// 2) 心跳携带资源 → 状态 busy、资源可见、status_changed 广播
	a.heartbeat("busy", map[string]any{
		"cpu":    map[string]any{"usage": 42.5, "cores": 8, "model": "test-cpu"},
		"memory": map[string]any{"total": 16000000000, "usage": 55.0},
	})
	var busyData map[string]any
	waitUntil(t, 3*time.Second, "心跳后 agent 状态变为 busy 且资源可见", func() bool {
		busyData = env.getAgentJSON(t, admin, "it-agent-1")
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
	dash.waitForAgentEvent(3*time.Second, "status_changed", func(d map[string]any) bool {
		return d["agentId"] == "it-agent-1" && d["status"] == "busy"
	})

	// 3) PING 保活 → 服务端回 PONG
	a.ping()
	a.waitForNotify(t, "PONG", 2*time.Second)

	// 4) 持续心跳期间反复触发超时检查 → 应保持在线（心跳刷新 LastSeen）
	deadline := time.Now().Add(600 * time.Millisecond)
	for time.Now().Before(deadline) {
		a.heartbeat("busy", nil)
		time.Sleep(100 * time.Millisecond)
		env.registry.CheckHeartbeatsNow()
	}
	if data := env.getAgentJSON(t, admin, "it-agent-1"); data["status"] != "busy" {
		t.Fatalf("持续心跳期间不应离线, got status=%v", data["status"])
	}

	// 5) 停止心跳 → 超过阈值 → 离线 + disconnected(reason=heartbeat_timeout) 广播
	time.Sleep(400 * time.Millisecond)
	env.registry.CheckHeartbeatsNow()
	env.waitForAgentStatus(t, admin, "it-agent-1", "offline")
	dash.waitForAgentEvent(3*time.Second, "disconnected", func(d map[string]any) bool {
		return d["agentId"] == "it-agent-1" && d["reason"] == "heartbeat_timeout"
	})

	// 6) 心跳恢复 → 重新上线（registry 保留条目，状态回升）
	a.heartbeat("online", nil)
	env.waitForAgentStatus(t, admin, "it-agent-1", "online")
	dash.waitForAgentEvent(3*time.Second, "status_changed", func(d map[string]any) bool {
		return d["agentId"] == "it-agent-1" && d["status"] == "online"
	})

	// 7) TCP 连接断开 → 服务端 Unregister → offline + disconnected 广播（无 reason）
	a.close()
	env.waitForAgentStatus(t, admin, "it-agent-1", "offline")
	dash.waitForAgentEvent(3*time.Second, "disconnected", func(d map[string]any) bool {
		return d["agentId"] == "it-agent-1" && d["reason"] == nil
	})
}
