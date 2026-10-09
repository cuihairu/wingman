package agent

import (
	"encoding/json"
	"net"
	"testing"
	"time"
)

// startTestListenerWithTokens 起 token 白名单开启的测试 listener。
func startTestListenerWithTokens(t *testing.T, tokens ...string) (string, *recordingRegistry) {
	t.Helper()
	reg := newRecordingRegistry()
	broadcast := &mockBroadcaster{}
	l, addr := startTestListener(t, reg, broadcast)
	l.SetAgentTokens(tokens)
	return addr, reg
}

// dialAndRegisterToken 连接并发送带/不带 token 的注册，返回 ack payload 与连接。
func dialAndRegisterToken(t *testing.T, addr, agentID string, token string, withToken bool) (map[string]any, net.Conn) {
	t.Helper()
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	t.Cleanup(func() { conn.Close() })

	msg := map[string]any{
		"type": "agent.register", "agentId": agentID, "hostname": "tk-host",
	}
	if withToken {
		msg["token"] = token
	}
	sendMessage(t, conn, Notify, 0, msg)

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	ack := readFrame(t, conn)
	var payload map[string]any
	if err := json.Unmarshal(ack.body, &payload); err != nil {
		t.Fatalf("register ack body: %v", err)
	}
	if payload["type"] != "agent.register_ack" {
		t.Fatalf("expected register_ack, got %v", payload["type"])
	}
	return payload, conn
}

// readFrameE 读取一帧但错误交还调用方（用于断连检测；readFrame 直接 Fatal）。
func readFrameE(conn net.Conn) (frame, error) {
	header, err := readN(conn, messageHeaderSize)
	if err != nil {
		return frame{}, err
	}
	f := frame{
		msgType:  MessageType(header[8]),
		sequence: testEndian.Uint32(header[4:8]),
	}
	length := testEndian.Uint32(header[0:4])
	if length > 0 {
		body, err := readN(conn, int(length))
		if err != nil {
			return f, err
		}
		f.body = body
	}
	return f, nil
}

// registeredWith 查询 recordingRegistry 是否已登记指定 agent。
func registeredWith(reg *recordingRegistry, agentID string) bool {
	registered, _, _, _ := reg.snapshot()
	for _, id := range registered {
		if id == agentID {
			return true
		}
	}
	return false
}

// TestRegisterTokenAuthAccepted 正确 token 注册成功。
func TestRegisterTokenAuthAccepted(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t, "secret-a", "secret-b")

	payload, conn := dialAndRegisterToken(t, addr, "tk-ok", "secret-b", true)
	if payload["success"] != true {
		t.Fatalf("expected success ack, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-ok") }, "tk-ok registered")
	_ = conn
}

// TestRegisterTokenAuthRejected 错误 token：ack success:false + 不入 Registry
// + 连接被服务端断开。
func TestRegisterTokenAuthRejected(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t, "secret-a")

	payload, conn := dialAndRegisterToken(t, addr, "tk-bad", "wrong-token", true)
	if payload["success"] != false {
		t.Fatalf("expected failure ack, got %v", payload)
	}
	if msg, _ := payload["error"].(string); msg == "" {
		t.Errorf("failure ack should carry error text, got %v", payload)
	}

	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-bad") {
		t.Errorf("rejected agent must not be registered")
	}

	// 服务端应在 ack 后主动断连
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	if _, err := readFrameE(conn); err == nil {
		t.Errorf("server should close connection after rejection")
	}
}

// TestRegisterTokenAuthMissing 开关开启但不带 token：同样拒绝。
func TestRegisterTokenAuthMissing(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t, "secret-a")

	payload, conn := dialAndRegisterToken(t, addr, "tk-none", "", false)
	if payload["success"] != false {
		t.Fatalf("expected failure ack for missing token, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-none") {
		t.Errorf("agent without token must not be registered")
	}
	_ = conn
}

// TestRegisterTokenAuthDisabled 开关关闭（默认）：不带 token 完全兼容。
func TestRegisterTokenAuthDisabled(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t) // 空 = 不鉴权

	payload, conn := dialAndRegisterToken(t, addr, "tk-legacy", "", false)
	if payload["success"] != true {
		t.Fatalf("auth disabled should accept token-less register, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-legacy") }, "tk-legacy registered")
	_ = conn
}

// startTestListenerWithStore 起 per-agent token DB 源启用的测试 listener。
func startTestListenerWithStore(t *testing.T, store *TokenStore) (string, *recordingRegistry) {
	t.Helper()
	reg := newRecordingRegistry()
	broadcast := &mockBroadcaster{}
	l, addr := startTestListener(t, reg, broadcast)
	l.SetTokenStore(store)
	return addr, reg
}

// TestRegisterDBTokenAccepted DB 源 token 校验通过：正常注册。
func TestRegisterDBTokenAccepted(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "pixel-8", "")

	addr, reg := startTestListenerWithStore(t, store)
	payload, conn := dialAndRegisterToken(t, addr, "tk-db", plain, true)
	if payload["success"] != true {
		t.Fatalf("DB token should be accepted, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-db") }, "tk-db registered")
	_ = conn
}

// TestRegisterDBTokenRejectedWhenRevoked 吊销后的 DB token：拒绝并断连。
func TestRegisterDBTokenRejectedWhenRevoked(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "pixel-8", "")
	if _, err := store.Revoke(1); err != nil {
		t.Fatalf("revoke: %v", err)
	}

	addr, reg := startTestListenerWithStore(t, store)
	payload, conn := dialAndRegisterToken(t, addr, "tk-revoked", plain, true)
	if payload["success"] != false {
		t.Fatalf("revoked token should be rejected, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-revoked") {
		t.Errorf("revoked-token agent must not be registered")
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	if _, err := readFrameE(conn); err == nil {
		t.Errorf("server should close connection after DB-token rejection")
	}
}

// TestRegisterStoreOnlyModeRejectsMissingToken env 白名单清空 + DB 源启用
// = 纯 DB 管理面模式：不带 token 拒绝（authRequired 覆盖 DB 源）。
func TestRegisterStoreOnlyModeRejectsMissingToken(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	mustCreateToken(t, store, "any", "")

	addr, reg := startTestListenerWithStore(t, store)
	payload, conn := dialAndRegisterToken(t, addr, "tk-none", "", false)
	if payload["success"] != false {
		t.Fatalf("store-only mode should reject token-less register, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-none") {
		t.Errorf("token-less agent must not be registered in store-only mode")
	}
	_ = conn
}

// TestRegisterEnvAndDBTokensBothAccepted 双源并存：env 命中与 DB 命中都放行。
func TestRegisterEnvAndDBTokensBothAccepted(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	dbPlain := mustCreateToken(t, store, "migrated", "")

	reg := newRecordingRegistry()
	broadcast := &mockBroadcaster{}
	l, addr := startTestListener(t, reg, broadcast)
	l.SetAgentTokens([]string{"env-token"})
	l.SetTokenStore(store)

	payload, conn := dialAndRegisterToken(t, addr, "tk-env", "env-token", true)
	if payload["success"] != true {
		t.Fatalf("env token should still be accepted, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-env") }, "tk-env registered")

	payload, conn2 := dialAndRegisterToken(t, addr, "tk-db2", dbPlain, true)
	if payload["success"] != true {
		t.Fatalf("DB token should be accepted alongside env, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-db2") }, "tk-db2 registered")
	_ = conn
	_ = conn2
}

// TestRegisterBoundTokenWrongAgentRejected 绑定 token 校验 agentId：
// 携带合法 token 但 agentId 不匹配 → 拒绝。
func TestRegisterBoundTokenWrongAgentRejected(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "pixel-8", "agent-pixel-8")

	addr, reg := startTestListenerWithStore(t, store)
	payload, conn := dialAndRegisterToken(t, addr, "agent-other", plain, true)
	if payload["success"] != false {
		t.Fatalf("bound token with wrong agentId should be rejected, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "agent-other") {
		t.Errorf("agent beyond binding must not be registered")
	}
	_ = conn
}

// TestRegisterEmptyStoreKeepsAuthClosed DB 源挂上但无任何有效 token、
// env 也空：鉴权保持关闭（向后兼容不变）。
func TestRegisterEmptyStoreKeepsAuthClosed(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t)) // 零 token

	addr, reg := startTestListenerWithStore(t, store)
	payload, conn := dialAndRegisterToken(t, addr, "tk-open", "", false)
	if payload["success"] != true {
		t.Fatalf("empty env + empty DB should keep auth closed, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-open") }, "tk-open registered")
	_ = conn
}
