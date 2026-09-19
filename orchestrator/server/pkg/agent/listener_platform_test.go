package agent

import (
	"encoding/json"
	"net"
	"testing"
	"time"
)

// TestHandleRegisterPlatform agent.register 上报的 platform 字段应透传给
// Registry（Android agent 依赖此标识区分设备类型，见 docs/android-agent-design.md §3.3）。
func TestHandleRegisterPlatform(t *testing.T) {
	reg := newRecordingRegistry()
	broadcast := &mockBroadcaster{}
	_, addr := startTestListener(t, reg, broadcast)

	conn := dialAndRegisterWithPlatform(t, addr, "android-001", "Pixel 8", "android")

	if got := len(reg.platforms); got != 1 {
		t.Fatalf("expected 1 platform record, got %d", got)
	}
	if reg.platforms[0] != "android" {
		t.Errorf("expected platform android, got %q", reg.platforms[0])
	}
	_ = conn
}

// TestHandleRegisterPlatformEmpty 旧版 agent 不上报 platform：透传空串，
// 由 Registry/展示层归一为 desktop。
func TestHandleRegisterPlatformEmpty(t *testing.T) {
	reg := newRecordingRegistry()
	broadcast := &mockBroadcaster{}
	_, addr := startTestListener(t, reg, broadcast)

	conn := dialAndRegister(t, addr, "desktop-001")

	_ = conn
	if got := len(reg.platforms); got != 1 {
		t.Fatalf("expected 1 platform record, got %d", got)
	}
	if reg.platforms[0] != "" {
		t.Errorf("expected empty platform, got %q", reg.platforms[0])
	}
}

// dialAndRegisterWithPlatform 在 dialAndRegister 基础上携带 platform 字段注册。
func dialAndRegisterWithPlatform(t *testing.T, addr, agentID, hostname, platform string) net.Conn {
	t.Helper()
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.register", "agentId": agentID, "hostname": hostname, "platform": platform,
	})

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	ack := readFrame(t, conn)
	var payload map[string]any
	if err := json.Unmarshal(ack.body, &payload); err != nil {
		t.Fatalf("register ack body: %v", err)
	}
	if payload["type"] != "agent.register_ack" {
		t.Fatalf("expected register_ack, got %v", payload["type"])
	}
	return conn
}
