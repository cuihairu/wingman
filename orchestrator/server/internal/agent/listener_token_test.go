package agent

import (
	"encoding/json"
	"net"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
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

// ---- challenge-response（§6.1）----

// dialRegisterChallenge 发送 challenge 型注册（不携明文 token），读取服务端
// 下发的 auth.challenge Request，返回连接、请求 sequence 与 nonce。
func dialRegisterChallenge(t *testing.T, addr, agentID string) (net.Conn, uint32, string) {
	t.Helper()
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	t.Cleanup(func() { conn.Close() })

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.register", "agentId": agentID, "hostname": "ch-host",
		"challenge": true,
	})

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	f, err := readFrameE(conn)
	if err != nil {
		t.Fatalf("read challenge request: %v", err)
	}
	if f.msgType != Request {
		t.Fatalf("expected Request frame for auth.challenge, got %v", f.msgType)
	}
	var payload map[string]any
	if err := json.Unmarshal(f.body, &payload); err != nil {
		t.Fatalf("challenge body: %v", err)
	}
	if payload["type"] != "auth.challenge" {
		t.Fatalf("expected auth.challenge, got %v", payload["type"])
	}
	nonce, _ := payload["nonce"].(string)
	if len(nonce) != 64 { // 32 字节 hex
		t.Fatalf("nonce should be 64 hex chars (32 bytes), got %d: %q", len(nonce), nonce)
	}
	return conn, f.sequence, nonce
}

// answerChallenge 以正确/错误的 HMAC 应答挑战（Response 帧、回显 sequence）。
func answerChallenge(t *testing.T, conn net.Conn, seq uint32, macHex string) {
	t.Helper()
	sendMessage(t, conn, Response, seq, map[string]any{"hmac": macHex})
}

// readRegisterAck 读下一帧并断言为 register_ack，返回 payload。
func readRegisterAck(t *testing.T, conn net.Conn) map[string]any {
	t.Helper()
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	f, err := readFrameE(conn)
	if err != nil {
		t.Fatalf("read register ack: %v", err)
	}
	var payload map[string]any
	if err := json.Unmarshal(f.body, &payload); err != nil {
		t.Fatalf("ack body: %v", err)
	}
	if payload["type"] != "agent.register_ack" {
		t.Fatalf("expected register_ack, got %v", payload["type"])
	}
	return payload
}

// TestRegisterChallengeEnvTokenAccepted env 白名单 token 的挑战应答通过：
// 明文 token 全程不过网，服务端以 sha256hex(token) 为 HMAC 密钥验签。
func TestRegisterChallengeEnvTokenAccepted(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t, "env-secret")

	conn, seq, nonce := dialRegisterChallenge(t, addr, "tk-ch-env")
	answerChallenge(t, conn, seq, tokenMAC(sha256Hex("env-secret"), nonce))

	payload := readRegisterAck(t, conn)
	if payload["success"] != true {
		t.Fatalf("challenge with correct HMAC should register, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-ch-env") }, "tk-ch-env registered")
}

// TestRegisterChallengeDBTokenAccepted DB 源 token 挑战通过：服务端直接用
// TokenHash 列作 HMAC 密钥，LastSeenAt 在验签命中时刷新。
func TestRegisterChallengeDBTokenAccepted(t *testing.T) {
	db := newTokenTestDB(t)
	store := NewTokenStore(db)
	plain := mustCreateToken(t, store, "pixel-8", "")

	addr, reg := startTestListenerWithStore(t, store)
	conn, seq, nonce := dialRegisterChallenge(t, addr, "tk-ch-db")
	answerChallenge(t, conn, seq, tokenMAC(sha256Hex(plain), nonce))

	payload := readRegisterAck(t, conn)
	if payload["success"] != true {
		t.Fatalf("DB-token challenge should register, got %v", payload)
	}
	waitForCond(t, time.Second, func() bool { return registeredWith(reg, "tk-ch-db") }, "tk-ch-db registered")

	// 验签命中即刷新 LastSeenAt（先于 completeRegistration，注册成功必已落）
	var rec models.AgentToken
	if err := db.First(&rec, 1).Error; err != nil {
		t.Fatalf("load token record: %v", err)
	}
	if rec.LastSeenAt == nil {
		t.Errorf("LastSeenAt should be touched on challenge verify")
	}
}

// TestRegisterChallengeWrongMACRejected 错误 HMAC：ack success:false +
// 不入 Registry + 断连。
func TestRegisterChallengeWrongMACRejected(t *testing.T) {
	addr, reg := startTestListenerWithTokens(t, "env-secret")

	conn, seq, nonce := dialRegisterChallenge(t, addr, "tk-ch-bad")
	answerChallenge(t, conn, seq, tokenMAC(sha256Hex("wrong-secret"), nonce))

	payload := readRegisterAck(t, conn)
	if payload["success"] != false {
		t.Fatalf("wrong HMAC should be rejected, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-ch-bad") {
		t.Errorf("agent with wrong HMAC must not be registered")
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	if _, err := readFrameE(conn); err == nil {
		t.Errorf("server should close connection after failed challenge")
	}
}

// TestRegisterChallengeBoundTokenWrongAgentRejected 绑定 token 挑战：
// HMAC 正确但 agentId 不匹配绑定 → 拒绝。
func TestRegisterChallengeBoundTokenWrongAgentRejected(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "pixel-8", "agent-pixel-8")

	addr, reg := startTestListenerWithStore(t, store)
	conn, seq, nonce := dialRegisterChallenge(t, addr, "agent-other")
	answerChallenge(t, conn, seq, tokenMAC(sha256Hex(plain), nonce))

	payload := readRegisterAck(t, conn)
	if payload["success"] != false {
		t.Fatalf("bound token beyond binding should be rejected, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "agent-other") {
		t.Errorf("agent beyond binding must not be registered")
	}
	_ = conn
}

// TestRegisterChallengeDuplicateRegisterRejected 挑战进行中同连接再来一次
// register：CAS 拒绝 + 断连（防并发双重注册）。挑战 goroutine 随超时收尾，
// 已 settled 的拒绝路径保持幂等。
func TestRegisterChallengeDuplicateRegisterRejected(t *testing.T) {
	old := challengeTimeout
	challengeTimeout = 150 * time.Millisecond
	t.Cleanup(func() { challengeTimeout = old })

	addr, reg := startTestListenerWithTokens(t, "env-secret")

	conn, _, _ := dialRegisterChallenge(t, addr, "tk-ch-dup")
	// 未应答挑战，先插第二次 register（协议违规）
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.register", "agentId": "tk-ch-dup", "hostname": "ch-host",
		"challenge": true,
	})

	payload := readRegisterAck(t, conn)
	if payload["success"] != false {
		t.Fatalf("duplicate register during pending challenge should be rejected, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-ch-dup") {
		t.Errorf("duplicate register must not be registered")
	}
	// 等挑战 goroutine 超时收尾（150ms），确保 cleanup 恢复 challengeTimeout
	// 前无在途读者
	time.Sleep(300 * time.Millisecond)
	_ = conn
}

// TestRegisterChallengeTimeoutNoAnswer 挑战超时不应答：拒绝 + 断连。
func TestRegisterChallengeTimeoutNoAnswer(t *testing.T) {
	old := challengeTimeout
	challengeTimeout = 150 * time.Millisecond
	t.Cleanup(func() { challengeTimeout = old })

	addr, reg := startTestListenerWithTokens(t, "env-secret")

	conn, _, _ := dialRegisterChallenge(t, addr, "tk-ch-timeout")
	// 不应答，直接等服务端超时判定
	payload := readRegisterAck(t, conn)
	if payload["success"] != false {
		t.Fatalf("unanswered challenge should be rejected on timeout, got %v", payload)
	}
	time.Sleep(100 * time.Millisecond)
	if registeredWith(reg, "tk-ch-timeout") {
		t.Errorf("unanswered agent must not be registered")
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	if _, err := readFrameE(conn); err == nil {
		t.Errorf("server should close connection after challenge timeout")
	}
}
