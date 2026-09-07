package agent

import (
	"encoding/json"
	"net"
	"sync"
	"testing"
	"time"
)

// ---------- 捕获型 mock ----------

type recordingRegistry struct {
	mu          sync.Mutex
	registered  []string
	unregistered []string
	statuses    map[string]string
	heartbeats  []string
	clients     map[string]any
}

func newRecordingRegistry() *recordingRegistry {
	return &recordingRegistry{
		statuses: make(map[string]string),
		clients:  make(map[string]any),
	}
}

func (r *recordingRegistry) Register(agentID, hostname, ip string, conn any) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.registered = append(r.registered, agentID)
}

func (r *recordingRegistry) Unregister(agentID string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.unregistered = append(r.unregistered, agentID)
}

func (r *recordingRegistry) UpdateStatus(agentID string, status string, resources any) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.statuses[agentID] = status
}

func (r *recordingRegistry) UpdateHeartbeat(agentID string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.heartbeats = append(r.heartbeats, agentID)
}

func (r *recordingRegistry) SetClient(agentID string, conn any) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.clients[agentID] = conn
}

func (r *recordingRegistry) snapshot() (registered, unregistered, heartbeats []string, statuses map[string]string) {
	r.mu.Lock()
	defer r.mu.Unlock()
	statuses = make(map[string]string, len(r.statuses))
	for k, v := range r.statuses {
		statuses[k] = v
	}
	return append([]string{}, r.registered...), append([]string{}, r.unregistered...),
		append([]string{}, r.heartbeats...), statuses
}

type recordingBroadcaster struct {
	mu        sync.Mutex
	agentEvts []string
	events    []string
}

func (b *recordingBroadcaster) BroadcastAgentEvent(eventType string, data any) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.agentEvts = append(b.agentEvts, eventType)
}

func (b *recordingBroadcaster) BroadcastEvent(eventType string, data any) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.events = append(b.events, eventType)
}

// ---------- 帧读取辅助 ----------

type frame struct {
	msgType  MessageType
	sequence uint32
	body     []byte
}

func readFrame(t *testing.T, conn net.Conn) frame {
	t.Helper()
	header, err := readN(conn, messageHeaderSize)
	if err != nil {
		t.Fatalf("read header: %v", err)
	}
	f := frame{
		msgType:  MessageType(header[8]),
		sequence: testEndian.Uint32(header[4:8]),
	}
	length := testEndian.Uint32(header[0:4])
	if length > 0 {
		body, err := readN(conn, int(length))
		if err != nil {
			t.Fatalf("read body: %v", err)
		}
		f.body = body
	}
	return f
}

func readN(conn net.Conn, n int) ([]byte, error) {
	buf := make([]byte, n)
	read := 0
	for read < n {
		got, err := conn.Read(buf[read:])
		if err != nil {
			return nil, err
		}
		read += got
	}
	return buf, nil
}

func startTestListener(t *testing.T, registry AgentRegistrar, broadcast Broadcaster) (*FrameListener, string) {
	t.Helper()
	listener := NewFrameListener(registry, broadcast)
	startErr := make(chan error, 1)
	go func() { startErr <- listener.Start("127.0.0.1:0") }()
	addr := waitForListenerAddr(t, listener, startErr)
	t.Cleanup(listener.Stop)
	return listener, addr
}

func dialAndRegister(t *testing.T, addr, agentID string) net.Conn {
	t.Helper()
	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "agent.register", "agentId": agentID, "hostname": "h"})

	// 消费 register_ack，避免污染后续帧读取
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

func waitForCond(t *testing.T, timeout time.Duration, fn func() bool, msg string) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if fn() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal(msg)
}

// ---------- 监听器与连接管理 ----------

func TestListenerTeamManagerAccessors(t *testing.T) {
	listener := NewFrameListener(newRecordingRegistry(), &recordingBroadcaster{})
	if listener.GetTeamManager() == nil {
		t.Fatal("default team manager should exist")
	}
	custom := NewTeamManager()
	listener.SetTeamManager(custom)
	if listener.GetTeamManager() != custom {
		t.Error("SetTeamManager/GetTeamManager mismatch")
	}
}

func TestListenerStartOnBusyPortFails(t *testing.T) {
	blocker, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	defer blocker.Close()

	listener := NewFrameListener(newRecordingRegistry(), &recordingBroadcaster{})
	if err := listener.Start(blocker.Addr().String()); err == nil {
		listener.Stop()
		t.Fatal("start on occupied port should fail")
	}
}

func TestListenerStopWithoutStart(t *testing.T) {
	listener := NewFrameListener(newRecordingRegistry(), &recordingBroadcaster{})
	listener.Stop() // 不应 panic
}

func TestListenerStopCleansConns(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	waitForCond(t, time.Second, func() bool {
		listener.mu.RLock()
		defer listener.mu.RUnlock()
		return len(listener.conns) == 1
	}, "connection not accepted")

	listener.Stop()
	listener.mu.RLock()
	remaining := len(listener.conns)
	listener.mu.RUnlock()
	if remaining != 0 {
		t.Errorf("expected conns cleaned, got %d", remaining)
	}
}

func TestAcceptLoopSurvivesListenerClose(t *testing.T) {
	registry := newRecordingRegistry()
	listener := NewFrameListener(registry, &recordingBroadcaster{})
	startErr := make(chan error, 1)
	go func() { startErr <- listener.Start("127.0.0.1:0") }()
	addr := waitForListenerAddr(t, listener, startErr)
	t.Cleanup(listener.Stop)
	_ = addr

	// 直接关闭底层 listener：acceptLoop 记录错误并继续；随后 Stop 收尾
	ln := listener.getListener()
	if ln == nil {
		t.Fatal("listener not set")
	}
	ln.Close()
	time.Sleep(50 * time.Millisecond)
	listener.Stop()

	if _, err := net.Dial("tcp", addr); err == nil {
		t.Error("dial after close should fail")
	}
}

// ---------- 注册与心跳 ----------

func TestRegisterGeneratesAgentIDFromAddrWhenMissing(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	// 无 agentId → 使用 remote addr 生成
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "agent.register", "hostname": "auto-id"})

	// 期待 register_ack
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	ack := readFrame(t, conn)
	var payload map[string]any
	if err := json.Unmarshal(ack.body, &payload); err != nil {
		t.Fatalf("ack body: %v", err)
	}
	if payload["type"] != "agent.register_ack" {
		t.Errorf("expected register_ack, got %v", payload["type"])
	}
	generated, _ := payload["agentId"].(string)
	if generated == "" || generated == "agent_" {
		t.Errorf("expected generated agent id, got %q", generated)
	}
}

func TestPingPongAndHeartbeat(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "ping-agent")

	// PING notify（4 字节 body）→ PONG + 心跳更新
	writeMessageHeader(t, conn, 4, Notify, 0)
	if _, err := conn.Write([]byte("PING")); err != nil {
		t.Fatal(err)
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if pong.msgType != Notify || string(pong.body) != "PONG" {
		t.Fatalf("expected PONG notify, got type=%d body=%q", pong.msgType, pong.body)
	}

	reg, _, hb, _ := registry.snapshot()
	if len(reg) != 1 || reg[0] != "ping-agent" {
		t.Fatalf("agent not registered: %v", reg)
	}
	waitForCond(t, time.Second, func() bool {
		_, _, hb, _ = registry.snapshot()
		return len(hb) >= 1
	}, "heartbeat not updated after PING")
}

func TestPingBeforeRegisterSkipsHeartbeat(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	writeMessageHeader(t, conn, 4, Notify, 0)
	if _, err := conn.Write([]byte("PING")); err != nil {
		t.Fatal(err)
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if string(pong.body) != "PONG" {
		t.Fatalf("expected PONG, got %q", pong.body)
	}

	// 未注册 → 不应记录心跳
	time.Sleep(100 * time.Millisecond)
	_, _, hb, _ := registry.snapshot()
	if len(hb) != 0 {
		t.Errorf("unregistered PING should not update heartbeat, got %v", hb)
	}
}

func TestAgentHeartbeatUpdatesStatus(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "hb-agent")

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type":  "agent.heartbeat",
		"agentId": "hb-agent",
		"status": "busy",
		"resources": map[string]any{"cpu": map[string]any{"usage": 10.0}},
	})
	waitForCond(t, time.Second, func() bool {
		_, _, _, statuses := registry.snapshot()
		return statuses["hb-agent"] == "busy"
	}, "heartbeat status not applied")
}

func TestAgentHeartbeatWithoutRegistrationIgnored(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "agent.heartbeat", "status": "busy"})
	time.Sleep(100 * time.Millisecond)
	_, _, _, statuses := registry.snapshot()
	if len(statuses) != 0 {
		t.Errorf("unregistered heartbeat should be ignored, got %v", statuses)
	}
}

// ---------- agent.event 分发 ----------

func TestAgentEventScriptOutputInvokesHandler(t *testing.T) {
	registry := newRecordingRegistry()
	broadcast := &recordingBroadcaster{}
	listener, addr := startTestListener(t, registry, broadcast)

	got := make(chan map[string]any, 4)
	listener.SetScriptOutputHandler(func(agentID string, data map[string]any) {
		data["agentID"] = agentID
		got <- data
	})
	conn := dialAndRegister(t, addr, "evt-agent")

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type":  "agent.event",
		"event": "script_output",
		"data":  map[string]any{"scriptId": "s1", "message": "line"},
	})
	select {
	case data := <-got:
		if data["scriptId"] != "s1" || data["agentID"] != "evt-agent" {
			t.Errorf("unexpected script output: %+v", data)
		}
	case <-time.After(time.Second):
		t.Fatal("script output handler not invoked")
	}
	waitForCond(t, time.Second, func() bool {
		broadcast.mu.Lock()
		defer broadcast.mu.Unlock()
		return len(broadcast.events) > 0 && broadcast.events[0] == "script"
	}, "script broadcast missing")

	// data 非 map → handler 收到空 map
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.event", "event": "script_output", "data": "raw-text",
	})
	select {
	case data := <-got:
		if data == nil {
			t.Error("handler should receive non-nil map")
		}
	case <-time.After(time.Second):
		t.Fatal("script output handler not invoked for non-map data")
	}
}

func TestAgentEventOtherKinds(t *testing.T) {
	registry := newRecordingRegistry()
	broadcast := &recordingBroadcaster{}
	_, addr := startTestListener(t, registry, broadcast)
	conn := dialAndRegister(t, addr, "evt2-agent")

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.event", "event": "trigger_fired", "data": map[string]any{"k": 1},
	})
	waitForCond(t, time.Second, func() bool {
		broadcast.mu.Lock()
		defer broadcast.mu.Unlock()
		for _, e := range broadcast.agentEvts {
			if e == "trigger_fired" {
				return true
			}
		}
		return false
	}, "trigger_fired not broadcast")

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.event", "event": "script_state", "data": map[string]any{"state": "paused"},
	})
	waitForCond(t, time.Second, func() bool {
		broadcast.mu.Lock()
		defer broadcast.mu.Unlock()
		return len(broadcast.events) >= 1
	}, "script_state not broadcast")

	// 未知事件仅记录日志，不应 panic
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "agent.event", "event": "mystery"})
	time.Sleep(100 * time.Millisecond)
}

// ---------- 请求响应与畸形消息 ----------

func TestRequestFrameGetsResponse(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "req-agent")

	body, _ := json.Marshal(map[string]any{"type": "query"})
	writeMessageHeader(t, conn, uint32(len(body)), Request, 77)
	if _, err := conn.Write(body); err != nil {
		t.Fatal(err)
	}

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	resp := readFrame(t, conn)
	if resp.msgType != Response || resp.sequence != 77 {
		t.Fatalf("expected Response seq=77, got type=%d seq=%d", resp.msgType, resp.sequence)
	}
	var payload map[string]any
	if err := json.Unmarshal(resp.body, &payload); err != nil {
		t.Fatal(err)
	}
	if payload["success"] != true {
		t.Errorf("expected success=true, got %v", payload["success"])
	}
}

func TestUnknownMessageTypeAndBadJSON(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "junk-agent")

	// Error 类型（4）→ handleMessage default 分支
	body := []byte(`{}`)
	writeMessageHeader(t, conn, uint32(len(body)), Error, 0)
	conn.Write(body)

	// 非法 JSON notify
	badJSON := []byte("{bad :")
	writeMessageHeader(t, conn, uint32(len(badJSON)), Notify, 0)
	conn.Write(badJSON)

	// 未知 notify 类型
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "mystery.notify"})

	// 连接应仍然存活：再发一次 PING 能收到 PONG
	writeMessageHeader(t, conn, 4, Notify, 0)
	conn.Write([]byte("PING"))
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if string(pong.body) != "PONG" {
		t.Fatalf("listener should survive malformed input, got %q", pong.body)
	}
}

func TestTruncatedBodyClosesConnection(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()

	// 声明 100 字节但只写 10 字节即断开 → ReadFull 失败
	writeMessageHeader(t, conn, 100, Notify, 0)
	conn.Write(make([]byte, 10))
	conn.Close()

	// 服务端 readLoop 应退出并清理连接
	waitForCond(t, time.Second, func() bool {
		listener.mu.RLock()
		defer listener.mu.RUnlock()
		return len(listener.conns) == 0
	}, "connection not cleaned after truncated body")
}

// ---------- SendCommand 往返 ----------

func findAgentConn(t *testing.T, listener *FrameListener, agentID string) *agentConn {
	t.Helper()
	var found *agentConn
	waitForCond(t, 2*time.Second, func() bool {
		listener.mu.RLock()
		defer listener.mu.RUnlock()
		for _, ac := range listener.conns {
			if ac.getAgentID() == agentID {
				found = ac
				return true
			}
		}
		return false
	}, "agent conn not found")
	return found
}

func TestSendCommandRoundTrip(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "cmd-agent")
	ac := findAgentConn(t, listener, "cmd-agent")

	type result struct {
		resp map[string]any
		err  error
	}
	done := make(chan result, 1)
	go func() {
		resp, err := ac.SendCommand("run_script", map[string]any{"path": "x.lua"})
		done <- result{resp, err}
	}()

	// 读取请求帧并回复 Response（sequence 匹配）
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	req := readFrame(t, conn)
	if req.msgType != Request {
		t.Fatalf("expected Request frame, got %d", req.msgType)
	}
	var reqBody map[string]any
	if err := json.Unmarshal(req.body, &reqBody); err != nil {
		t.Fatal(err)
	}
	if reqBody["type"] != "run_script" || reqBody["path"] != "x.lua" {
		t.Errorf("unexpected request body: %v", reqBody)
	}

	respBody, _ := json.Marshal(map[string]any{"success": true, "data": "ok"})
	writeMessageHeader(t, conn, uint32(len(respBody)), Response, req.sequence)
	conn.Write(respBody)

	select {
	case r := <-done:
		if r.err != nil {
			t.Fatalf("SendCommand: %v", r.err)
		}
		if r.resp["success"] != true {
			t.Errorf("unexpected response: %v", r.resp)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("SendCommand did not return")
	}
}

func TestSendCommandWithInvalidJSONResponse(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "badjson-agent")
	ac := findAgentConn(t, listener, "badjson-agent")

	done := make(chan error, 1)
	go func() {
		_, err := ac.SendCommandWithTimeout("x", nil, 2*time.Second)
		done <- err
	}()

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	req := readFrame(t, conn)
	bad := []byte("{not-json")
	writeMessageHeader(t, conn, uint32(len(bad)), Response, req.sequence)
	conn.Write(bad)

	select {
	case err := <-done:
		if err == nil {
			t.Fatal("invalid JSON response should error")
		}
	case <-time.After(2 * time.Second):
		t.Fatal("SendCommand did not return")
	}
}

func TestOrphanResponseLoggedOnly(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "orphan-agent")

	// 无 pending 命令时发送 Response → 仅记录，不影响后续
	body, _ := json.Marshal(map[string]any{"success": true})
	writeMessageHeader(t, conn, uint32(len(body)), Response, 999)
	conn.Write(body)

	writeMessageHeader(t, conn, 4, Notify, 0)
	conn.Write([]byte("PING"))
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if string(pong.body) != "PONG" {
		t.Fatalf("orphan response should not break connection, got %q", pong.body)
	}
}

func TestSendCommandMarshalError(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	dialAndRegister(t, addr, "marshal-agent")
	ac := findAgentConn(t, listener, "marshal-agent")

	if _, err := ac.SendCommand("bad", map[string]any{"ch": make(chan int)}); err == nil {
		t.Fatal("unserializable command data should error")
	}
}

func TestSendCommandOnClosedConn(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "closed-agent")
	ac := findAgentConn(t, listener, "closed-agent")

	conn.Close()
	time.Sleep(150 * time.Millisecond)

	if _, err := ac.SendCommandWithTimeout("x", nil, time.Second); err == nil {
		t.Fatal("command on closed conn should error")
	}
}

func TestAgentConnClose(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "close-agent")
	ac := findAgentConn(t, listener, "close-agent")

	ac.Close()
	// 客户端应观察到连接关闭
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 16)
	if _, err := conn.Read(buf); err == nil {
		t.Error("expected read error after server-side close")
	}
}
