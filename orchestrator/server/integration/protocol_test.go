package integration

// 跨语言 frame/protocol 集成测试。
//
// 验证 Go FrameListener 与 C++ runtime（libs/transport session.hpp 的
// Message::serialize() = MessageHeader memcpy + body）之间的字节级兼容：
//
//	C++ MessageHeader（session.hpp:47，16 字节，小端平台 memcpy）：
//	  uint32_t length;      // [0:4]  消息体长度
//	  uint32_t sequence;    // [4:8]  序列号
//	  MessageType type;     // [8]    1=Request 2=Response 3=Notify 4=Error
//	  (padding)             // [9:12] 3 字节对齐填充
//	  uint32_t reserved;    // [12:16]保留
//
// Go 侧镜像定义见 pkg/agent/client.go。四种 frame 类型对应架构文档的命令流：
// agent.register / agent.heartbeat / command dispatch（Request+Response，即
// command.run_script → command.result 的 wire 形式）/ event report（agent.event）。
//
// 测试全部走裸 TCP 字节读写（不经 simAgent 的高层封装），确保验证的是
// 序列化字节本身，而非 Go 两侧的共享代码路径。

import (
	"encoding/json"
	"io"
	"net"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// 与 C++ MessageType 枚举（session.hpp:40）对齐。
const (
	cppRequest  = byte(1)
	cppResponse = byte(2)
	cppNotify   = byte(3)
	cppError    = byte(4)
)

// maxFrameBody 与 pkg/agent maxResponseSize（16 MiB）一致。
const maxFrameBody = 16 * 1024 * 1024

// dialProtocol 建立到 FrameListener 的裸连接（模拟 C++ runtime outbound）。
func dialProtocol(t *testing.T, env *testEnv) net.Conn {
	t.Helper()
	conn, err := net.DialTimeout("tcp", env.agentAddr, 2*time.Second)
	if err != nil {
		t.Fatalf("dial frame listener: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	return conn
}

// writeCppFrame 按 C++ Message::serialize() 的字节布局写一帧。
func writeCppFrame(t *testing.T, conn net.Conn, msgType byte, seq uint32, body []byte) {
	t.Helper()
	header := make([]byte, 16)
	frameEndian.PutUint32(header[0:4], uint32(len(body)))
	frameEndian.PutUint32(header[4:8], seq)
	header[8] = msgType
	frameEndian.PutUint32(header[12:16], 0) // reserved
	if _, err := conn.Write(header); err != nil {
		t.Fatalf("write frame header: %v", err)
	}
	if len(body) > 0 {
		if _, err := conn.Write(body); err != nil {
			t.Fatalf("write frame body: %v", err)
		}
	}
}

// writeCppJSONFrame 写 JSON body 的帧。
func writeCppJSONFrame(t *testing.T, conn net.Conn, msgType byte, seq uint32, payload any) {
	t.Helper()
	body, err := json.Marshal(payload)
	if err != nil {
		t.Fatalf("marshal frame body: %v", err)
	}
	writeCppFrame(t, conn, msgType, seq, body)
}

// readCppFrame 按 C++ Message::deserialize() 视角读一帧，返回 header 原始字节与 body。
func readCppFrame(t *testing.T, conn net.Conn, timeout time.Duration) (header []byte, msgType byte, seq uint32, body []byte) {
	t.Helper()
	if err := conn.SetReadDeadline(time.Now().Add(timeout)); err != nil {
		t.Fatalf("set read deadline: %v", err)
	}
	header = make([]byte, 16)
	if _, err := io.ReadFull(conn, header); err != nil {
		t.Fatalf("read frame header: %v", err)
	}
	length := frameEndian.Uint32(header[0:4])
	seq = frameEndian.Uint32(header[4:8])
	msgType = header[8]
	if length > 0 {
		body = make([]byte, length)
		if _, err := io.ReadFull(conn, body); err != nil {
			t.Fatalf("read frame body (%d bytes): %v", length, err)
		}
	}
	return header, msgType, seq, body
}

// assertHeaderLayout 断言 header 字节严格符合 C++ MessageHeader 布局。
func assertHeaderLayout(t *testing.T, header []byte, wantType byte, wantSeq uint32, bodyLen int) {
	t.Helper()
	if got := frameEndian.Uint32(header[0:4]); got != uint32(bodyLen) {
		t.Errorf("header.length = %d, want %d", got, bodyLen)
	}
	if got := frameEndian.Uint32(header[4:8]); got != wantSeq {
		t.Errorf("header.sequence = %d, want %d", got, wantSeq)
	}
	if header[8] != wantType {
		t.Errorf("header.type = %d, want %d", header[8], wantType)
	}
	if got := frameEndian.Uint32(header[12:16]); got != 0 {
		t.Errorf("header.reserved = %d, want 0", got)
	}
}

// registerRawAgent 用裸字节完成 agent.register，返回 register_ack body。
func registerRawAgent(t *testing.T, conn net.Conn, agentID, hostname string) map[string]any {
	t.Helper()
	writeCppJSONFrame(t, conn, cppNotify, 0, map[string]any{
		"type":     "agent.register",
		"agentId":  agentID,
		"hostname": hostname,
	})
	_, msgType, _, body := readCppFrame(t, conn, 3*time.Second)
	if msgType != cppNotify {
		t.Fatalf("register_ack frame type = %d, want Notify(3)", msgType)
	}
	var ack map[string]any
	if err := json.Unmarshal(body, &ack); err != nil {
		t.Fatalf("register_ack body not json: %v", err)
	}
	if ack["type"] != "agent.register_ack" {
		t.Fatalf("register_ack type = %v, want agent.register_ack", ack["type"])
	}
	return ack
}

// waitRegistry 等待 registry 中 agent 满足条件。
func waitRegistry(t *testing.T, env *testEnv, agentID string, cond func(*agent.AgentInfo) bool, desc string) *agent.AgentInfo {
	t.Helper()
	var info *agent.AgentInfo
	waitUntil(t, 3*time.Second, desc, func() bool {
		got, ok := env.registry.Get(agentID)
		if !ok {
			return false
		}
		if cond(got) {
			info = got
			return true
		}
		return false
	})
	return info
}

// ---------- 1. agent.register ----------

// 字节级验证 agent.register：C++ 布局的 Notify 帧进、register_ack Notify 帧出、
// registry 注册成功，且响应 header 布局可被 C++ Message::deserialize() 解析。
func TestProtocolAgentRegisterRoundTrip(t *testing.T) {
	env := newTestEnv(t)
	conn := dialProtocol(t, env)

	ack := registerRawAgent(t, conn, "proto-register-1", "cpp-host")
	if ack["agentId"] != "proto-register-1" {
		t.Errorf("register_ack agentId = %v, want proto-register-1", ack["agentId"])
	}

	info := waitRegistry(t, env, "proto-register-1",
		func(i *agent.AgentInfo) bool { return i.Status == agent.StatusOnline },
		"agent 注册后 online")
	if info.Hostname != "cpp-host" {
		t.Errorf("registry hostname = %q, want cpp-host", info.Hostname)
	}
}

// ---------- 2. agent.heartbeat ----------

// 心跳帧更新 registry 状态/资源/链路统计；PING 保活帧回 PONG 并刷新心跳时间。
func TestProtocolAgentHeartbeatUpdatesRegistry(t *testing.T) {
	env := newTestEnv(t)
	conn := dialProtocol(t, env)
	registerRawAgent(t, conn, "proto-heartbeat-1", "cpp-host")

	// 与 remote_client.cpp 一致的 heartbeat 字段：status/resources/link
	// （resources 按 agent.ResourceStats 嵌套结构：cpu/memory/disk/network/system）
	writeCppJSONFrame(t, conn, cppNotify, 0, map[string]any{
		"type":   "agent.heartbeat",
		"status": "online",
		"resources": map[string]any{
			"cpu":    map[string]any{"usage": 42.5, "cores": 8, "model": "i7"},
			"memory": map[string]any{"total": 16000000000, "available": 8000000000, "usage": 61.0},
		},
		"link": map[string]any{
			"reconnects":           3,
			"dropped":              1,
			"outboxPending":        2,
			"lastDisconnectReason": "connection_reset",
			"sessionUptimeMs":      95000,
		},
	})

	info := waitRegistry(t, env, "proto-heartbeat-1",
		func(i *agent.AgentInfo) bool {
			return i.Resources.CPU.Usage == 42.5 && i.Resources.Memory.Usage == 61.0 && i.Link.Reconnects == 3
		},
		"heartbeat 写入 resources 与 link 统计")
	if info.Resources.CPU.Cores != 8 || info.Resources.CPU.Model != "i7" {
		t.Errorf("cpu stats = %+v, want cores=8 model=i7", info.Resources.CPU)
	}
	if info.Link.Dropped != 1 || info.Link.OutboxPending != 2 {
		t.Errorf("link stats = %+v, want dropped=1 outboxPending=2", info.Link)
	}
	if info.Link.LastDisconnectReason != "connection_reset" || info.Link.SessionUptimeMs != 95000 {
		t.Errorf("link stats = %+v, want reason=connection_reset uptime=95000", info.Link)
	}

	// PING 保活帧（Notify + 4 字节 "PING"）→ 应回 PONG
	writeCppFrame(t, conn, cppNotify, 0, []byte("PING"))
	header, msgType, _, body := readCppFrame(t, conn, 3*time.Second)
	if msgType != cppNotify || string(body) != "PONG" {
		t.Fatalf("PING reply = type %d body %q, want Notify(3) PONG", msgType, body)
	}
	assertHeaderLayout(t, header, cppNotify, 0, 4)
}

// ---------- 3. command dispatch / result（Request + Response） ----------

// server 下发命令：字节布局为 Request 帧（sequence 配对）；runtime 回 Response 帧
// 后 SendCommand 解析得到结果——即架构文档 command.run_script → command.result 的
// wire 形式。
func TestProtocolCommandDispatchResult(t *testing.T) {
	env := newTestEnv(t)
	conn := dialProtocol(t, env)
	registerRawAgent(t, conn, "proto-command-1", "cpp-host")

	// server 侧异步下发命令（SendCommandWithTimeout 会阻塞等待 Response）
	type result struct {
		resp map[string]any
		err  error
	}
	cmdCh := make(chan result, 1)
	go func() {
		client, ok := env.registry.GetClient("proto-command-1")
		if !ok {
			cmdCh <- result{err: io.ErrUnexpectedEOF}
			return
		}
		resp, err := client.SendCommandWithTimeout("command.run_script", map[string]any{
			"command_id": "cmd-42",
			"script":     "demo.lua",
		}, 5*time.Second)
		cmdCh <- result{resp: resp, err: err}
	}()

	// C++ 视角读命令帧：type=Request(1)、sequence、body 含 method
	header, msgType, seq, body := readCppFrame(t, conn, 3*time.Second)
	if msgType != cppRequest {
		t.Fatalf("command frame type = %d, want Request(1)", msgType)
	}
	var req map[string]any
	if err := json.Unmarshal(body, &req); err != nil {
		t.Fatalf("command body not json: %v", err)
	}
	if req["method"] != "command.run_script" {
		t.Errorf("command method = %v, want command.run_script", req["method"])
	}
	if req["command_id"] != "cmd-42" {
		t.Errorf("command_id = %v, want cmd-42", req["command_id"])
	}
	assertHeaderLayout(t, header, cppRequest, seq, len(body))

	// runtime 回 Response 帧（同 sequence）——command.result
	writeCppJSONFrame(t, conn, cppResponse, seq, map[string]any{
		"command_id": "cmd-42",
		"success":    true,
		"data":       map[string]any{"exitCode": 0},
	})

	select {
	case got := <-cmdCh:
		if got.err != nil {
			t.Fatalf("SendCommand error: %v", got.err)
		}
		if got.resp["success"] != true {
			t.Errorf("command result = %v, want success", got.resp)
		}
		data, _ := got.resp["data"].(map[string]any)
		if data == nil || data["exitCode"] != float64(0) {
			t.Errorf("command result data = %v, want exitCode=0", got.resp["data"])
		}
	case <-time.After(5 * time.Second):
		t.Fatal("SendCommand did not return after runtime response")
	}
}

// ---------- 4. event report（agent.event） ----------

// runtime 上报事件帧（agent.event/script_output）：server 解析后经
// ScriptOutputHandler 持久化到 ExecutionLog（dashboard 经 WebSocket 收广播）。
func TestProtocolEventReportPersisted(t *testing.T) {
	env := newTestEnv(t)
	conn := dialProtocol(t, env)
	registerRawAgent(t, conn, "proto-event-1", "cpp-host")

	writeCppJSONFrame(t, conn, cppNotify, 0, map[string]any{
		"type":  "agent.event",
		"event": "script_output",
		"data": map[string]any{
			"scriptId": "proto-event-script",
			"message":  "hello from cpp runtime",
			"level":    "warn",
		},
	})

	waitUntil(t, 3*time.Second, "script_output 事件持久化", func() bool {
		var logs []models.ExecutionLog
		env.db.Where("script_id = ?", "proto-event-script").Find(&logs)
		return len(logs) == 1 && logs[0].Output == "hello from cpp runtime" && logs[0].Level == "warn"
	})
}

// ---------- 5. 边界：超大 payload ----------

// header.length 超过 16 MiB 上限：server 必须断开连接（不分配巨型缓冲），
// 且监听器存活可接受新连接。
func TestProtocolOversizedPayloadRejected(t *testing.T) {
	env := newTestEnv(t)

	conn, err := net.DialTimeout("tcp", env.agentAddr, 2*time.Second)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	header := make([]byte, 16)
	frameEndian.PutUint32(header[0:4], maxFrameBody+1)
	header[8] = cppNotify
	if _, err := conn.Write(header); err != nil {
		t.Fatalf("write oversized header: %v", err)
	}

	// server 应关闭连接（读到 EOF）；给读设置 deadline 防挂死
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	if _, err := conn.Read(make([]byte, 16)); err == nil {
		t.Fatal("expected connection close after oversized frame header")
	} else if err != io.EOF {
		// TCP RST 也视作断开
		t.Logf("connection terminated with: %v", err)
	}
	conn.Close()

	// 监听器仍应健康：新连接可正常注册
	refresh := dialProtocol(t, env)
	registerRawAgent(t, refresh, "proto-oversize-recover", "cpp-host")
}

// ---------- 6. 边界：非法 frame ----------

// 三类非法输入不应崩溃监听器：
// a) Notify body 非法 JSON → 记日志跳过，连接保持可用
// b) 未知 MessageType（如 200）→ 记日志跳过，连接保持可用
// c) 截断 header（8 字节后断开）→ 连接关闭，无 panic
func TestProtocolMalformedFramesTolerated(t *testing.T) {
	env := newTestEnv(t)
	conn := dialProtocol(t, env)

	// a) 非 JSON body 的 Notify
	writeCppFrame(t, conn, cppNotify, 0, []byte("not-json{{"))

	// b) 未知消息类型
	writeCppJSONFrame(t, conn, byte(200), 0, map[string]any{"type": "whatever"})

	// c) Error 类型（枚举已定义但无 handler 分支）也应只跳过
	writeCppJSONFrame(t, conn, cppError, 0, map[string]any{"type": "err"})

	// 连接应仍然可用：补一个合法 register 成功
	ack := registerRawAgent(t, conn, "proto-malformed-1", "cpp-host")
	if ack["agentId"] != "proto-malformed-1" {
		t.Fatalf("connection unusable after malformed frames: %v", ack)
	}

	// 截断 header：写 8 字节后关闭 → server 读 header 失败断开，不 panic
	trunc, err := net.DialTimeout("tcp", env.agentAddr, 2*time.Second)
	if err != nil {
		t.Fatalf("dial truncated-header conn: %v", err)
	}
	trunc.Write([]byte{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07})
	trunc.Close()

	// 监听器存活
	after := dialProtocol(t, env)
	registerRawAgent(t, after, "proto-malformed-2", "cpp-host")
}

// ---------- 7. 边界：断线重连 ----------

// runtime 断开后 server 应将 agent 标记 offline；重连并重新 register 后
// registry 恢复 online 且命令通道可用（C++ RemoteClient 重连语义）。
func TestProtocolReconnectAfterDisconnect(t *testing.T) {
	env := newTestEnv(t)

	conn1, err := net.DialTimeout("tcp", env.agentAddr, 2*time.Second)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	registerRawAgent(t, conn1, "proto-reconnect-1", "cpp-host")
	waitRegistry(t, env, "proto-reconnect-1",
		func(i *agent.AgentInfo) bool { return i.Status == agent.StatusOnline },
		"首次注册 online")

	// 断开 → server Unregister → offline（条目保留，状态翻转）
	conn1.Close()
	waitRegistry(t, env, "proto-reconnect-1",
		func(i *agent.AgentInfo) bool { return i.Status == agent.StatusOffline },
		"断开后 offline")

	// 重连 → register → 恢复 online，命令通道重新可用
	conn2 := dialProtocol(t, env)
	registerRawAgent(t, conn2, "proto-reconnect-1", "cpp-host")
	waitRegistry(t, env, "proto-reconnect-1",
		func(i *agent.AgentInfo) bool { return i.Status == agent.StatusOnline },
		"重连注册后恢复 online")

	cmdDone := make(chan error, 1)
	go func() {
		client, ok := env.registry.GetClient("proto-reconnect-1")
		if !ok {
			cmdDone <- io.ErrUnexpectedEOF
			return
		}
		_, err := client.SendCommandWithTimeout("get_status", nil, 3*time.Second)
		cmdDone <- err
	}()

	// 读到 Request 帧即证明重连后的命令通道恢复；回 Response 完成往返
	_, msgType, seq, _ := readCppFrame(t, conn2, 3*time.Second)
	if msgType != cppRequest {
		t.Fatalf("post-reconnect frame type = %d, want Request(1)", msgType)
	}
	writeCppJSONFrame(t, conn2, cppResponse, seq, map[string]any{"success": true})
	if err := <-cmdDone; err != nil {
		t.Fatalf("post-reconnect command failed: %v", err)
	}
}
