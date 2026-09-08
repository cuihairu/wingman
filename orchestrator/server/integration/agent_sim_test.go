package integration

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"sync"
	"testing"
	"time"
	"unsafe"
)

// 与 pkg/agent 一致的 16 字节消息头布局（本机字节序）。
const (
	frameHeaderSize = 16
	frameRequest    = byte(1)
	frameResponse   = byte(2)
	frameNotify     = byte(3)
)

var frameEndian = func() binary.ByteOrder {
	var x uint16 = 0x0102
	if *(*byte)(unsafe.Pointer(&x)) == 0x02 {
		return binary.LittleEndian
	}
	return binary.BigEndian
}()

// simCommand 记录 orchestrator 下发给 agent 的一条命令。
type simCommand struct {
	Method string
	Seq    uint32
	Data   map[string]any
	At     time.Time
}

// simAgent 模拟 C++ runtime：outbound 连接 FrameListener，讲相同的帧协议。
// readLoop 是唯一读方；收到 Request 帧后回调 handler 并回写 Response 帧。
type simAgent struct {
	t    *testing.T
	id   string
	conn net.Conn

	wmu      sync.Mutex // 保护 conn 写（响应帧与测试侧 notify 并发）
	mu       sync.Mutex // 保护以下字段
	commands []simCommand
	notifies []string
	handler  func(method string, data map[string]any) map[string]any
}

// newSimAgent 连接 orchestrator、发送 agent.register 并等待 register_ack。
func newSimAgent(t *testing.T, addr, agentID, hostname string, handler func(string, map[string]any) map[string]any) *simAgent {
	t.Helper()
	conn, err := net.DialTimeout("tcp", addr, 2*time.Second)
	if err != nil {
		t.Fatalf("sim agent %s dial %s: %v", agentID, addr, err)
	}
	a := &simAgent{t: t, id: agentID, conn: conn, handler: handler}
	t.Cleanup(a.close)
	go a.readLoop()

	a.notify("agent.register", map[string]any{
		"agentId":  agentID,
		"hostname": hostname,
	})
	a.waitForNotify(t, "agent.register_ack", 2*time.Second)
	return a
}

func (a *simAgent) close() {
	a.conn.Close()
}

// ---------- 帧读写 ----------

func (a *simAgent) writeFrame(msgType byte, seq uint32, payload any) {
	body, err := json.Marshal(payload)
	if err != nil {
		a.t.Errorf("sim agent %s marshal frame: %v", a.id, err)
		return
	}
	a.wmu.Lock()
	defer a.wmu.Unlock()
	header := make([]byte, frameHeaderSize)
	frameEndian.PutUint32(header[0:4], uint32(len(body)))
	frameEndian.PutUint32(header[4:8], seq)
	header[8] = msgType
	frameEndian.PutUint32(header[12:16], 0)
	if _, err := a.conn.Write(header); err != nil {
		return
	}
	a.conn.Write(body)
}

func (a *simAgent) notify(msgType string, extra map[string]any) {
	payload := map[string]any{"type": msgType}
	for k, v := range extra {
		payload[k] = v
	}
	a.writeFrame(frameNotify, 0, payload)
}

func (a *simAgent) readLoop() {
	for {
		header := make([]byte, frameHeaderSize)
		if _, err := io.ReadFull(a.conn, header); err != nil {
			return
		}
		length := frameEndian.Uint32(header[0:4])
		seq := frameEndian.Uint32(header[4:8])
		msgType := header[8]

		body := make([]byte, length)
		if length > 0 {
			if _, err := io.ReadFull(a.conn, body); err != nil {
				return
			}
		}

		switch msgType {
		case frameRequest:
			a.handleRequest(seq, body)
		case frameNotify:
			a.recordNotify(body)
		}
	}
}

func (a *simAgent) handleRequest(seq uint32, body []byte) {
	var msg map[string]any
	if err := json.Unmarshal(body, &msg); err != nil {
		a.writeFrame(frameResponse, seq, map[string]any{"success": false, "error": "bad request json"})
		return
	}
	method, _ := msg["method"].(string)
	if method == "" {
		method, _ = msg["type"].(string)
	}

	a.mu.Lock()
	a.commands = append(a.commands, simCommand{Method: method, Seq: seq, Data: msg, At: time.Now()})
	handler := a.handler
	a.mu.Unlock()

	resp := map[string]any{"success": true}
	if handler != nil {
		if custom := handler(method, msg); custom != nil {
			resp = custom
		}
	}
	a.writeFrame(frameResponse, seq, resp)
}

func (a *simAgent) recordNotify(body []byte) {
	if string(body) == "PONG" {
		a.mu.Lock()
		a.notifies = append(a.notifies, "PONG")
		a.mu.Unlock()
		return
	}
	var msg map[string]any
	if err := json.Unmarshal(body, &msg); err != nil {
		return
	}
	typ, _ := msg["type"].(string)
	a.mu.Lock()
	a.notifies = append(a.notifies, typ)
	a.mu.Unlock()
}

// ---------- agent 侧主动行为 ----------

// heartbeat 上报心跳（agent.heartbeat Notify），status/resources 语义与 runtime 一致。
func (a *simAgent) heartbeat(status string, resources map[string]any) {
	extra := map[string]any{"status": status}
	if resources != nil {
		extra["resources"] = resources
	}
	a.notify("agent.heartbeat", extra)
}

// pushEvent 上报 agent 事件（agent.event Notify），如 script_output。
func (a *simAgent) pushEvent(event string, data map[string]any) {
	a.notify("agent.event", map[string]any{"event": event, "data": data})
}

// ping 发送 4 字节 PING 保活帧，服务端应回 PONG 并刷新心跳。
func (a *simAgent) ping() {
	a.wmu.Lock()
	defer a.wmu.Unlock()
	header := make([]byte, frameHeaderSize)
	frameEndian.PutUint32(header[0:4], 4)
	header[8] = frameNotify
	if _, err := a.conn.Write(header); err != nil {
		return
	}
	a.conn.Write([]byte("PING"))
}

// ---------- 断言辅助 ----------

func (a *simAgent) setHandler(h func(method string, data map[string]any) map[string]any) {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.handler = h
}

func (a *simAgent) commandsOf(method string) []simCommand {
	a.mu.Lock()
	defer a.mu.Unlock()
	var out []simCommand
	for _, c := range a.commands {
		if c.Method == method {
			out = append(out, c)
		}
	}
	return out
}

func (a *simAgent) waitForCommands(t *testing.T, method string, n int, timeout time.Duration) []simCommand {
	t.Helper()
	var out []simCommand
	waitUntil(t, timeout, fmt.Sprintf("agent %s 收到 %d 条 %s 命令", a.id, n, method), func() bool {
		out = a.commandsOf(method)
		return len(out) >= n
	})
	return out
}

func (a *simAgent) waitForNotify(t *testing.T, typ string, timeout time.Duration) {
	t.Helper()
	waitUntil(t, timeout, fmt.Sprintf("agent %s 收到 %s 通知", a.id, typ), func() bool {
		a.mu.Lock()
		defer a.mu.Unlock()
		for _, got := range a.notifies {
			if got == typ {
				return true
			}
		}
		return false
	})
}
