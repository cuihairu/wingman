package agent

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// guac_relay 回环端到端与错误分支：中继是 Guacamole 会话的目标转发关键路径，
// 拆链语义（guacd 侧表现为立即失败而非挂到超时）逐分支钉住。全部走本机回环
// TCP，无外部依赖。

// relaySendRecorder 记录中继发往 agent 方向的 Notify 帧（channel 保序），
// fail 置位时模拟 agent 链路断（发送失败）。
type relaySendRecorder struct {
	fail atomic.Bool
	ch   chan map[string]any
}

func newRelaySendRecorder() *relaySendRecorder {
	return &relaySendRecorder{ch: make(chan map[string]any, 64)}
}

func (s *relaySendRecorder) send(_ string, msgType string, data map[string]any) error {
	if s.fail.Load() {
		return errors.New("agent link down")
	}
	m := map[string]any{"type": msgType}
	for k, v := range data {
		m[k] = v
	}
	s.ch <- m
	return nil
}

func waitForFrame(t *testing.T, ch <-chan map[string]any, msgType string) map[string]any {
	t.Helper()
	deadline := time.After(2 * time.Second)
	for {
		select {
		case f := <-ch:
			if f["type"] == msgType {
				return f
			}
		case <-deadline:
			t.Fatalf("timeout waiting for %s frame", msgType)
		}
	}
}

func dialRelay(t *testing.T, addr string) net.Conn {
	t.Helper()
	c, err := net.DialTimeout("tcp", addr, time.Second)
	if err != nil {
		t.Fatalf("dial relay %s: %v", addr, err)
	}
	return c
}

// expectClosed 断言 guacd 侧连接在时限内被服务端关闭（拆链语义的对外表现）。
func expectClosed(t *testing.T, c net.Conn, what string) {
	t.Helper()
	_ = c.SetReadDeadline(time.Now().Add(2 * time.Second))
	if _, err := c.Read(make([]byte, 16)); err == nil {
		t.Fatalf("%s: connection should be torn down, still readable", what)
	}
}

func TestGuacRelayEndToEndLoopback(t *testing.T) {
	rec := newRelaySendRecorder()
	const proxyID = "guac:e2e-1"
	r, addr, err := StartGuacRelay("agent-e2e", proxyID, "127.0.0.1:22", rec.send)
	if err != nil {
		t.Fatalf("start relay: %v", err)
	}
	defer r.Close()

	guacd := dialRelay(t, addr)
	defer guacd.Close()

	// 注册先于数据：proxy.new 携带目标与 connId
	newFrame := waitForFrame(t, rec.ch, "proxy.new")
	if newFrame["proxyId"] != proxyID || newFrame["target"] != "127.0.0.1:22" || newFrame["proxyType"] != "tcp" {
		t.Fatalf("proxy.new shape: %+v", newFrame)
	}
	connID, _ := newFrame["connId"].(string)
	if connID == "" {
		t.Fatalf("proxy.new missing connId: %+v", newFrame)
	}

	// guacd → 中继 → proxy.data（base64 载荷，读缓冲拷贝竞态的回归面）
	payload := []byte("hello-from-guacd")
	if _, err := guacd.Write(payload); err != nil {
		t.Fatalf("write to relay: %v", err)
	}
	dataFrame := waitForFrame(t, rec.ch, "proxy.data")
	if dataFrame["connId"] != connID || dataFrame["proxyId"] != proxyID {
		t.Fatalf("proxy.data routing fields: %+v", dataFrame)
	}
	got, err := base64.StdEncoding.DecodeString(dataFrame["data"].(string))
	if err != nil || !bytes.Equal(got, payload) {
		t.Fatalf("payload mismatch: %q %v", got, err)
	}

	// agent → guacd 回程（handleNotify 的 proxy.data 路由进 guacRelayDeliver）
	if err := guacRelayDeliver(proxyID, connID, []byte("reply")); err != nil {
		t.Fatalf("deliver: %v", err)
	}
	_ = guacd.SetReadDeadline(time.Now().Add(2 * time.Second))
	back := make([]byte, 5)
	if _, err := io.ReadFull(guacd, back); err != nil || string(back) != "reply" {
		t.Fatalf("guacd side read: %q %v", back, err)
	}

	// guacd 侧干净断开 → pump 走 EOF 分支退出 → proxy.close("guacd closed")
	_ = guacd.Close()
	closeFrame := waitForFrame(t, rec.ch, "proxy.close")
	if closeFrame["connId"] != connID || closeFrame["reason"] != "guacd closed" {
		t.Fatalf("proxy.close on EOF: %+v", closeFrame)
	}
}

func TestGuacRelaySendFailureTearsDown(t *testing.T) {
	rec := newRelaySendRecorder()
	r, addr, err := StartGuacRelay("agent-e2e", "guac:e2e-2", "127.0.0.1:22", rec.send)
	if err != nil {
		t.Fatalf("start relay: %v", err)
	}
	defer r.Close()

	// proxy.new 即失败：guacd 侧 connect 表现为连接被拆（非挂到超时）
	rec.fail.Store(true)
	g1 := dialRelay(t, addr)
	expectClosed(t, g1, "proxy.new failure")

	// proxy.data 中途失败：pump 退出并拆链
	rec.fail.Store(false)
	g2 := dialRelay(t, addr)
	waitForFrame(t, rec.ch, "proxy.new")
	rec.fail.Store(true)
	if _, err := g2.Write([]byte("x")); err != nil {
		t.Fatalf("write: %v", err)
	}
	expectClosed(t, g2, "pump send failure")

	// 非 EOF 读错误（RST 硬断）：同走拆链并通知 agent
	rec.fail.Store(false)
	g3 := dialRelay(t, addr).(*net.TCPConn)
	waitForFrame(t, rec.ch, "proxy.new")
	_ = g3.SetLinger(0) // 关闭即发 RST，中继 Read 得 ECONNRESET 而非 EOF
	_ = g3.Close()
	closeFrame := waitForFrame(t, rec.ch, "proxy.close")
	if closeFrame["reason"] != "guacd closed" {
		t.Fatalf("read-error teardown should notify agent: %+v", closeFrame)
	}
}

func TestGuacRelayRoutingErrors(t *testing.T) {
	// 未知 relay：deliver 报错、close/error 静默（无日志噪声）
	if err := guacRelayDeliver("guac:ghost", "c", []byte("x")); err == nil {
		t.Fatal("deliver to unknown relay should error")
	}
	guacRelayHandleClose("guac:ghost", "c", "x")
	guacRelayHandleError("guac:ghost", "c", "x")

	rec := newRelaySendRecorder()
	r, addr, err := StartGuacRelay("agent-rt", "guac:rt-1", "127.0.0.1:22", rec.send)
	if err != nil {
		t.Fatalf("start relay: %v", err)
	}
	defer r.Close()

	g1 := dialRelay(t, addr)
	defer g1.Close()
	connID, _ := waitForFrame(t, rec.ch, "proxy.new")["connId"].(string)

	if err := guacRelayDeliver("guac:rt-1", "no-such-conn", []byte("x")); err == nil {
		t.Fatal("deliver to unknown conn should error")
	}

	// agent 拨目标失败（带 connId）：只拆该连接并通知 agent
	guacRelayHandleError("guac:rt-1", connID, "dial refused")
	expectClosed(t, g1, "single-conn error")
	closeFrame := waitForFrame(t, rec.ch, "proxy.close")
	if closeFrame["reason"] != "agent dial failed: dial refused" {
		t.Fatalf("dial-failure close reason: %+v", closeFrame)
	}

	// agent 侧目标断开：同拆 guacd 侧连接
	g2 := dialRelay(t, addr)
	defer g2.Close()
	connID2, _ := waitForFrame(t, rec.ch, "proxy.new")["connId"].(string)
	guacRelayHandleClose("guac:rt-1", connID2, "target died")
	expectClosed(t, g2, "agent-side close")
	if f := waitForFrame(t, rec.ch, "proxy.close"); f["connId"] != connID2 {
		t.Fatalf("close frame conn mismatch: %+v", f)
	}
}

func TestGuacRelayErrorWithoutConnIDDropsRelay(t *testing.T) {
	rec := newRelaySendRecorder()
	r, addr, err := StartGuacRelay("agent-rt", "guac:rt-2", "127.0.0.1:22", rec.send)
	if err != nil {
		t.Fatalf("start relay: %v", err)
	}
	defer r.Close()

	g := dialRelay(t, addr)
	defer g.Close()
	waitForFrame(t, rec.ch, "proxy.new")

	// 无 connId 的错误无法定位连接 → 整条中继拆掉（注册表摘除 + listener 关）
	guacRelayHandleError("guac:rt-2", "", "agent broke")
	if guacRelayLookup("guac:rt-2") != nil {
		t.Fatal("relay should be deregistered after unlocated error")
	}
	r.Close() // 幂等：重复 Close 不得 panic
}

// ---------- agentConn 的 proxy.* 帧入口（handleNotify 路由分支） ----------

func TestAgentConnProxyFrameRouting(t *testing.T) {
	rec := newRelaySendRecorder()
	r, addr, err := StartGuacRelay("agent-ac", "guac:ac-1", "127.0.0.1:22", rec.send)
	if err != nil {
		t.Fatalf("start relay: %v", err)
	}
	defer r.Close()

	g := dialRelay(t, addr)
	defer g.Close()
	connID, _ := waitForFrame(t, rec.ch, "proxy.new")["connId"].(string)

	ac := &agentConn{id: "proxy-rt"} // 三个 handler 只消费 msg，不触 conn
	// 缺 proxyId/connId → 丢弃
	ac.handleProxyData(map[string]any{"proxyId": "guac:ac-1"})
	// 未知前缀 → 丢弃（端口转发等后续消费方的扩展点）
	ac.handleProxyData(map[string]any{"proxyId": "pfwd:1", "connId": connID, "data": "aGk="})
	// 坏 base64 → 丢弃
	ac.handleProxyData(map[string]any{"proxyId": "guac:ac-1", "connId": connID, "data": "not-base64!!"})
	// 正常投递：字节落到 guacd 侧
	ac.handleProxyData(map[string]any{"proxyId": "guac:ac-1", "connId": connID,
		"data": base64.StdEncoding.EncodeToString([]byte("down-link"))})
	_ = g.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, len("down-link"))
	if _, err := io.ReadFull(g, buf); err != nil || string(buf) != "down-link" {
		t.Fatalf("proxied data did not reach guacd side: %q %v", buf, err)
	}

	// close：未知前缀静默返回
	ac.handleProxyClose(map[string]any{"proxyId": "pfwd:1", "connId": connID})
	// error：未知前缀记日志返回
	ac.handleProxyError(map[string]any{"proxyId": "pfwd:1"})
	// close/error 的正常路由：拆 guacd 侧连接
	g2 := dialRelay(t, addr)
	defer g2.Close()
	connID2, _ := waitForFrame(t, rec.ch, "proxy.new")["connId"].(string)
	ac.handleProxyError(map[string]any{"proxyId": "guac:ac-1", "connId": connID2, "error": "dial refused"})
	expectClosed(t, g2, "routed error")
	g3 := dialRelay(t, addr)
	defer g3.Close()
	connID3, _ := waitForFrame(t, rec.ch, "proxy.new")["connId"].(string)
	ac.handleProxyClose(map[string]any{"proxyId": "guac:ac-1", "connId": connID3, "reason": "target died"})
	expectClosed(t, g3, "routed close")
}

// ---------- GuacRelaySendToAgent（server → agent 帧发送口） ----------

// stubPlainConn 只实现 AgentConn（无 SendNotify）→ 按不支持代理协议失败。
type stubPlainConn struct{}

func (stubPlainConn) SendCommand(string, map[string]any) (map[string]any, error) { return nil, nil }
func (stubPlainConn) SendCommandWithTimeout(string, map[string]any, time.Duration) (map[string]any, error) {
	return nil, nil
}

// stubNotifyConn 实现 AgentConn + notifySender，记录调用并按 err 返回。
type stubNotifyConn struct {
	mu  sync.Mutex
	got map[string]any
	err error
}

func (c *stubNotifyConn) SendCommand(string, map[string]any) (map[string]any, error) { return nil, nil }
func (c *stubNotifyConn) SendCommandWithTimeout(string, map[string]any, time.Duration) (map[string]any, error) {
	return nil, nil
}
func (c *stubNotifyConn) SendNotify(msgType string, data map[string]any) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	m := map[string]any{"type": msgType}
	for k, v := range data {
		m[k] = v
	}
	c.got = m
	return c.err
}

type stubRelayRegistry struct {
	conn AgentConn
	ok   bool
}

func (r *stubRelayRegistry) GetClient(string) (AgentConn, bool) { return r.conn, r.ok }

func TestGuacRelaySendToAgent(t *testing.T) {
	cases := []struct {
		name    string
		reg     NotifyRegistry
		wantErr string
	}{
		{"nil registry", nil, "registry unavailable"},
		{"agent offline", &stubRelayRegistry{conn: nil, ok: false}, "agent agent-x not connected"},
		{"registered nil conn", &stubRelayRegistry{conn: nil, ok: true}, "agent agent-x not connected"},
		{"no notify support", &stubRelayRegistry{conn: stubPlainConn{}, ok: true}, "agent agent-x connection does not support notify push"},
		{"send fails", &stubRelayRegistry{conn: &stubNotifyConn{err: errors.New("link down")}, ok: true}, "link down"},
	}
	for _, tc := range cases {
		err := GuacRelaySendToAgent(tc.reg, "agent-x", "proxy.new", map[string]any{"k": "v"})
		if err == nil || tc.wantErr == "" || err.Error() != tc.wantErr {
			t.Fatalf("%s: got %v, want %q", tc.name, err, tc.wantErr)
		}
	}

	// 成功路径：帧 type 与 data 合并送达
	sender := &stubNotifyConn{}
	if err := GuacRelaySendToAgent(&stubRelayRegistry{conn: sender, ok: true}, "agent-x", "proxy.data",
		map[string]any{"connId": "c1"}); err != nil {
		t.Fatalf("happy path: %v", err)
	}
	sender.mu.Lock()
	got := sender.got
	sender.mu.Unlock()
	if got["type"] != "proxy.data" || got["connId"] != "c1" {
		t.Fatalf("frame shape: %+v", got)
	}
}

// ---------- SendNotify（导出口）契约 ----------

// recordingConn 记录写出帧；Read 立即 EOF（无人读侧）。
type recordingConn struct {
	mu     sync.Mutex
	frames [][]byte
	closed atomic.Bool
}

func (c *recordingConn) Write(b []byte) (int, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	cp := make([]byte, len(b))
	copy(cp, b)
	c.frames = append(c.frames, cp)
	return len(b), nil
}
func (c *recordingConn) Read([]byte) (int, error)         { return 0, io.EOF }
func (c *recordingConn) Close() error                     { c.closed.Store(true); return nil }
func (c *recordingConn) LocalAddr() net.Addr              { return nil }
func (c *recordingConn) RemoteAddr() net.Addr             { return nil }
func (c *recordingConn) SetDeadline(time.Time) error      { return nil }
func (c *recordingConn) SetReadDeadline(time.Time) error  { return nil }
func (c *recordingConn) SetWriteDeadline(time.Time) error { return nil }

// SendNotify 写失败如实返回错误（中继以此判定拆链），且不自行关连接——
// 拆链是内部 sendNotify 包装的职责，语义分层不得混。
func TestSendNotifyExportedSemantics(t *testing.T) {
	rc := &recordingConn{}
	ac := &agentConn{id: "sn-ok", conn: rc, pending: map[uint32]*pendingResponse{}}
	if err := ac.SendNotify("proxy.new", map[string]any{"proxyId": "p1"}); err != nil {
		t.Fatalf("send: %v", err)
	}
	rc.mu.Lock()
	var frame map[string]any
	err := json.Unmarshal(rc.frames[len(rc.frames)-1], &frame)
	rc.mu.Unlock()
	if err != nil || frame["type"] != "proxy.new" || frame["proxyId"] != "p1" {
		t.Fatalf("frame body: %v %v", frame, err)
	}

	c := &shortBlockingConn{block: 500 * time.Millisecond}
	ac2 := &agentConn{id: "sn-fail", conn: c, pending: map[uint32]*pendingResponse{}}
	orig := notifyWriteTimeout
	notifyWriteTimeout = 100 * time.Millisecond
	defer func() { notifyWriteTimeout = orig }()
	if err := ac2.SendNotify("proxy.data", nil); err == nil {
		t.Fatal("write failure must be reported")
	}
	if c.closed.Load() {
		t.Fatal("exported SendNotify must not close the conn; teardown belongs to sendNotify")
	}
}

// ---------- TeamManager.ListTeams（Dashboard 列表渲染契约） ----------

func TestTeamListTeamsSortedViews(t *testing.T) {
	tm := NewTeamManager()
	if views := tm.ListTeams(); len(views) != 0 {
		t.Fatalf("fresh manager should list no teams, got %d", len(views))
	}
	tm.CreateTeamNamed("alpha", "d1", "leader-a", "agent-a")
	tm.CreateTeamNamed("beta", "d2", "leader-b", "agent-b")

	views := tm.ListTeams()
	if len(views) != 2 {
		t.Fatalf("should list 2 teams, got %d", len(views))
	}
	prev := ""
	for _, v := range views {
		id, _ := v["teamId"].(string)
		if id == "" {
			t.Fatalf("view missing teamId: %+v", v)
		}
		if prev != "" && id < prev {
			t.Fatalf("views must be sorted by teamId ascending: %q after %q", id, prev)
		}
		prev = id
		if v["name"] != "alpha" && v["name"] != "beta" {
			t.Fatalf("unexpected team name: %+v", v)
		}
	}
	if _, err := tm.GetTeamInfo(views[0]["teamId"].(string)); err != nil {
		t.Fatalf("list view id should resolve via GetTeamInfo: %v", err)
	}
}
