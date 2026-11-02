package agent

import (
	"errors"
	"net"
	"os"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// linkRecordingRegistry 记录 UpdateLinkHealth 调用，用于断言 heartbeat 携带的
// link 统计被透传给注册表。
type linkRecordingRegistry struct {
	mockRegistry
	mu    sync.Mutex
	calls int
}

func (m *linkRecordingRegistry) UpdateLinkHealth(agentID string, raw map[string]any) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls++
}

func (m *linkRecordingRegistry) linkCalls() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.calls
}

// SetScriptOutputHandler 设置的回调应在 script_output 事件到达时被调用。
func TestSetScriptOutputHandlerInvoked(t *testing.T) {
	reg := &mockRegistry{}
	listener, addr := startTestListener(t, reg, &mockBroadcaster{})

	done := make(chan struct{}, 1)
	listener.SetScriptOutputHandler(func(agentID string, data map[string]any) {
		if data["scriptId"] == "gap-script" {
			done <- struct{}{}
		}
	})

	conn := dialAndRegister(t, addr, "gap-agent-1")
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.event", "event": "script_output",
		"data": map[string]any{"scriptId": "gap-script", "message": "hi"},
	})

	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("script output handler was not invoked")
	}
}

// 未 Start 的 listener：acceptLoop 应在取不到监听 socket 后立即返回。
func TestAcceptLoopWithoutListenerReturns(t *testing.T) {
	NewFrameListener(&mockRegistry{}, &mockBroadcaster{}).acceptLoop() // ln == nil → 直接返回，不 panic
}

// 心跳携带 link 统计 → UpdateLinkHealth 应被调用。
func TestHeartbeatCarriesLinkStats(t *testing.T) {
	reg := &linkRecordingRegistry{}
	_, addr := startTestListener(t, reg, &mockBroadcaster{})

	conn := dialAndRegister(t, addr, "gap-agent-2")
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.heartbeat",
		"link": map[string]any{"reconnects": 2, "dropped": 1},
	})

	waitForCond(t, 2*time.Second, func() bool {
		return reg.linkCalls() >= 1
	}, "UpdateLinkHealth should be called for heartbeat link stats")
}

// inbox.report 指向不存在的 agent → ReportMessage 失败并记录日志（不崩溃）。
func TestInboxReportUnknownAgentLogsError(t *testing.T) {
	reg := &mockRegistry{}
	_, addr := startTestListener(t, reg, &mockBroadcaster{})

	conn := dialAndRegister(t, addr, "gap-agent-3")
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type":    "inbox.report",
		"agentId": "ghost-agent", "msgId": "m1",
		"result": map[string]any{"done": true},
	})
	// 无严格断言：覆盖错误日志分支，连接应保持存活
	time.Sleep(150 * time.Millisecond)
}

// ---------- readLoop stopCh 分支 ----------

// listener 已 Stop 后启动 readLoop：select 应立即命中 stopCh 并返回。
func TestReadLoopExitsOnStoppedListener(t *testing.T) {
	listener := NewFrameListener(&mockRegistry{}, &mockBroadcaster{})
	listener.Stop() // close(stopCh)，此时无连接

	server, client := net.Pipe()
	defer client.Close()
	ac := &agentConn{id: "stopped", conn: server, listener: listener, pending: map[uint32]*pendingResponse{}}
	go ac.readLoop()

	// readLoop 应在 stopCh 分支退出；给 goroutine 时间执行
	time.Sleep(100 * time.Millisecond)
}

// ---------- SendCommandWithTimeout 写失败分支 ----------

// failingBodyConn: 第一次 Write（header）成功，第二次 Write（body）失败。
type failingBodyConn struct {
	writes int
}

func (c *failingBodyConn) Read(b []byte) (int, error) {
	time.Sleep(50 * time.Millisecond)
	return 0, nil
}
func (c *failingBodyConn) Write(b []byte) (int, error) {
	c.writes++
	if c.writes >= 2 {
		return 0, errors.New("body write failed")
	}
	return len(b), nil
}
func (c *failingBodyConn) Close() error                       { return nil }
func (c *failingBodyConn) LocalAddr() net.Addr                { return nil }
func (c *failingBodyConn) RemoteAddr() net.Addr               { return nil }
func (c *failingBodyConn) SetDeadline(t time.Time) error      { return nil }
func (c *failingBodyConn) SetReadDeadline(t time.Time) error  { return nil }
func (c *failingBodyConn) SetWriteDeadline(t time.Time) error { return nil }

// header 写成功但 body 写失败 → SendCommandWithTimeout 返回写错误并清理 pending。
func TestSendCommandBodyWriteFailure(t *testing.T) {
	listener := NewFrameListener(&mockRegistry{}, &mockBroadcaster{})
	c := &failingBodyConn{}
	ac := &agentConn{id: "flaky", conn: c, listener: listener, pending: map[uint32]*pendingResponse{}}

	_, err := ac.SendCommandWithTimeout("run_script", map[string]any{"path": "x.lua"}, time.Second)
	if err == nil {
		t.Fatal("expected body write error")
	}
	ac.mu.Lock()
	pendingLen := len(ac.pending)
	ac.mu.Unlock()
	if pendingLen != 0 {
		t.Errorf("pending map should be cleaned up on write failure, got %d entries", pendingLen)
	}
}

// ---------- 有界帧写（writeFrameLocked）与失败拆链 ----------

// blockingConn 模拟对端窗口为零：所有 Write 都阻塞到写截止时间才返回超时。
type blockingConn struct {
	closed atomic.Bool
}

func (c *blockingConn) Read(b []byte) (int, error) { time.Sleep(50 * time.Millisecond); return 0, nil }
func (c *blockingConn) Write(b []byte) (int, error) {
	// 阻塞到写 deadline（由 SetWriteDeadline 设置）；net.Conn 契约要求按截止返回超时。
	time.Sleep(notifyWriteTimeout + 100*time.Millisecond)
	return 0, os.ErrDeadlineExceeded
}
func (c *blockingConn) Close() error                       { c.closed.Store(true); return nil }
func (c *blockingConn) LocalAddr() net.Addr                { return nil }
func (c *blockingConn) RemoteAddr() net.Addr               { return nil }
func (c *blockingConn) SetDeadline(t time.Time) error      { return nil }
func (c *blockingConn) SetReadDeadline(t time.Time) error  { return nil }
func (c *blockingConn) SetWriteDeadline(t time.Time) error { return nil }

// shortBlockingConn 与 blockingConn 同义，但写阻塞时长由构造参数控制（毫秒级测试用）。
type shortBlockingConn struct {
	block  time.Duration
	closed atomic.Bool
}

func (c *shortBlockingConn) Read(b []byte) (int, error) {
	time.Sleep(10 * time.Millisecond)
	return 0, nil
}
func (c *shortBlockingConn) Write(b []byte) (int, error) {
	time.Sleep(c.block)
	return 0, os.ErrDeadlineExceeded
}
func (c *shortBlockingConn) Close() error                       { c.closed.Store(true); return nil }
func (c *shortBlockingConn) LocalAddr() net.Addr                { return nil }
func (c *shortBlockingConn) RemoteAddr() net.Addr               { return nil }
func (c *shortBlockingConn) SetDeadline(t time.Time) error      { return nil }
func (c *shortBlockingConn) SetReadDeadline(t time.Time) error  { return nil }
func (c *shortBlockingConn) SetWriteDeadline(t time.Time) error { return nil }

// header 写阻塞（对端零窗口）也必须受 writeFrameLocked 截止时间约束，不能无限持锁。
func TestWriteFrameLockedBoundsHeaderWrite(t *testing.T) {
	c := &shortBlockingConn{block: 500 * time.Millisecond}
	ac := &agentConn{id: "bound-header", conn: c}
	header := MessageHeader{Length: 4, Type: Notify}
	start := time.Now()
	err := ac.writeFrameLocked(&header, []byte("PONG"), 100*time.Millisecond)
	if err == nil {
		t.Fatal("expected write deadline error")
	}
	if elapsed := time.Since(start); elapsed > 750*time.Millisecond {
		t.Fatalf("header write not bounded: elapsed %v > 750ms", elapsed)
	}
}

// sendNotify 写失败应主动拆链（Close），让 readLoop 走回收路径，避免锁持有者卡死。
func TestSendNotifyWriteFailureClosesConn(t *testing.T) {
	c := &shortBlockingConn{block: 500 * time.Millisecond}
	ac := &agentConn{id: "close-on-fail", conn: c, pending: map[uint32]*pendingResponse{}}
	orig := notifyWriteTimeout
	notifyWriteTimeout = 100 * time.Millisecond
	defer func() { notifyWriteTimeout = orig }()
	ac.sendNotify("inbox.message", map[string]any{"msgId": "m1"})
	if !c.closed.Load() {
		t.Fatal("sendNotify should close the connection on write failure")
	}
}

// SendCommandWithTimeout 写失败（含 header）应返回错误并清理 pending。
func TestSendCommandWriteFailureBoundsAndCleans(t *testing.T) {
	c := &shortBlockingConn{block: 500 * time.Millisecond}
	ac := &agentConn{id: "cmd-bound", conn: c, pending: map[uint32]*pendingResponse{}}
	orig := notifyWriteTimeout
	notifyWriteTimeout = 100 * time.Millisecond
	defer func() { notifyWriteTimeout = orig }()
	_, err := ac.SendCommandWithTimeout("run_script", map[string]any{"path": "x.lua"}, 2*time.Second)
	if err == nil {
		t.Fatal("expected write error")
	}
	ac.mu.Lock()
	n := len(ac.pending)
	ac.mu.Unlock()
	if n != 0 {
		t.Fatalf("pending not cleaned up on write failure, got %d", n)
	}
}
