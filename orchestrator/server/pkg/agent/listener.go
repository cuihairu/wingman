package agent

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"sync"
	"sync/atomic"
	"unsafe"
)

// Broadcaster WebSocket 广播接口（避免循环依赖 internal/agent ↔ pkg/websocket）
type Broadcaster interface {
	BroadcastAgentEvent(eventType string, data interface{})
	BroadcastEvent(eventType string, data interface{})
}

// AgentRegistrar Agent 注册接口（避免循环依赖）
type AgentRegistrar interface {
	Register(agentID, hostname, ip string, conn any)
	Unregister(agentID string)
	UpdateStatus(agentID string, status string, resources any)
	UpdateHeartbeat(agentID string)
	SetClient(agentID string, conn any)
}

// AgentConnection Agent TCP 连接接口
type AgentConnection interface {
	SendCommand(method string, data map[string]interface{}) (map[string]interface{}, error)
	Close()
}

// ScriptOutputHandler 脚本输出回调
type ScriptOutputHandler func(agentID string, data map[string]any)

// FrameListener TCP 监听器，接受 runtime 的 outbound 连接
type FrameListener struct {
	listener   net.Listener
	registry   AgentRegistrar
	broadcast  Broadcaster
	conns      map[string]*agentConn
	mu         sync.RWMutex
	connCount  uint64
	stopCh     chan struct{}
	onScript   ScriptOutputHandler // 脚本输出回调（用于持久化日志）
}

// agentConn 一个 agent TCP 连接
type agentConn struct {
	id       string
	conn     net.Conn
	agentID  string
	listener *FrameListener
	mu       sync.Mutex
}

// NewFrameListener 创建 TCP 监听器
func NewFrameListener(registry AgentRegistrar, broadcast Broadcaster) *FrameListener {
	return &FrameListener{
		registry:  registry,
		broadcast: broadcast,
		conns:     make(map[string]*agentConn),
		stopCh:    make(chan struct{}),
	}
}

// listenerEndian 包级字节序
var listenerEndian = func() binary.ByteOrder {
	var x uint16 = 0x0102
	if *(*byte)(unsafe.Pointer(&x)) == 0x02 {
		return binary.LittleEndian
	}
	return binary.BigEndian
}()

// SetScriptOutputHandler 设置脚本输出回调
func (l *FrameListener) SetScriptOutputHandler(handler ScriptOutputHandler) {
	l.onScript = handler
}

// Start 启动监听
func (l *FrameListener) Start(addr string) error {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	l.listener = ln
	log.Printf("[FrameListener] Listening on %s", addr)

	go l.acceptLoop()
	return nil
}

// Stop 停止监听
func (l *FrameListener) Stop() {
	close(l.stopCh)
	if l.listener != nil {
		l.listener.Close()
	}
	l.mu.Lock()
	for id, ac := range l.conns {
		ac.conn.Close()
		delete(l.conns, id)
	}
	l.mu.Unlock()
}

// acceptLoop 接受连接循环
func (l *FrameListener) acceptLoop() {
	for {
		select {
		case <-l.stopCh:
			return
		default:
		}

		conn, err := l.listener.Accept()
		if err != nil {
			select {
			case <-l.stopCh:
				return
			default:
				log.Printf("[FrameListener] Accept error: %v", err)
				continue
			}
		}

		n := atomic.AddUint64(&l.connCount, 1)
		connID := fmt.Sprintf("agent_conn_%d", n)

		ac := &agentConn{
			id:       connID,
			conn:     conn,
			listener: l,
		}

		l.mu.Lock()
		l.conns[connID] = ac
		l.mu.Unlock()

		log.Printf("[FrameListener] New connection: %s from %s", connID, conn.RemoteAddr())
		go ac.readLoop()
	}
}

// SendCommand 实现 AgentConnection 接口
func (ac *agentConn) SendCommand(method string, data map[string]interface{}) (map[string]interface{}, error) {
	ac.mu.Lock()
	defer ac.mu.Unlock()

	req := map[string]interface{}{
		"type":   method,
		"method": method,
	}
	for k, v := range data {
		req[k] = v
	}

	reqBytes, err := json.Marshal(req)
	if err != nil {
		return nil, err
	}

	header := MessageHeader{
		Length: uint32(len(reqBytes)),
		Type:   Request,
	}
	if err := ac.writeMsgHeader(&header); err != nil {
		return nil, err
	}
	if _, err := ac.conn.Write(reqBytes); err != nil {
		return nil, err
	}

	// 读取响应
	respHeader, err := ac.readMsgHeader()
	if err != nil {
		return nil, err
	}

	if respHeader.Length > maxResponseSize {
		return nil, fmt.Errorf("response too large: %d", respHeader.Length)
	}

	respBody := make([]byte, respHeader.Length)
	if _, err := io.ReadFull(ac.conn, respBody); err != nil {
		return nil, err
	}

	var resp map[string]interface{}
	if err := json.Unmarshal(respBody, &resp); err != nil {
		return nil, err
	}
	return resp, nil
}

// Close 关闭连接
func (ac *agentConn) Close() {
	ac.conn.Close()
}

// readLoop 读取消息循环
func (ac *agentConn) readLoop() {
	defer func() {
		ac.conn.Close()
		ac.listener.mu.Lock()
		delete(ac.listener.conns, ac.id)
		ac.listener.mu.Unlock()

		if ac.agentID != "" {
			ac.listener.registry.Unregister(ac.agentID)
		}
		log.Printf("[FrameListener] Connection closed: %s", ac.id)
	}()

	for {
		select {
		case <-ac.listener.stopCh:
			return
		default:
		}

		header, err := ac.readMsgHeader()
		if err != nil {
			if err != io.EOF {
				log.Printf("[FrameListener] Read header error: %v", err)
			}
			return
		}

		if header.Length > maxResponseSize {
			log.Printf("[FrameListener] Message too large: %d", header.Length)
			return
		}

		body := make([]byte, header.Length)
		if header.Length > 0 {
			if _, err := io.ReadFull(ac.conn, body); err != nil {
				log.Printf("[FrameListener] Read body error: %v", err)
				return
			}
		}

		ac.handleMessage(header, body)
	}
}

// handleMessage 处理收到的消息
func (ac *agentConn) handleMessage(header *MessageHeader, body []byte) {
	switch header.Type {
	case Notify:
		ac.handleNotify(body)
	case Request:
		ac.handleRequest(header, body)
	default:
		log.Printf("[FrameListener] Unknown message type: %d", header.Type)
	}
}

// handleNotify 处理 Notify 消息
func (ac *agentConn) handleNotify(body []byte) {
	// 处理原始心跳（PING 文本）
	if len(body) == 4 && string(body) == "PING" {
		ac.sendPong()
		if ac.agentID != "" {
			ac.listener.registry.UpdateHeartbeat(ac.agentID)
		}
		return
	}

	var msg map[string]interface{}
	if err := json.Unmarshal(body, &msg); err != nil {
		log.Printf("[FrameListener] JSON parse error: %v", err)
		return
	}

	msgType, _ := msg["type"].(string)

	switch msgType {
	case "agent.register":
		ac.handleRegister(msg)
	case "agent.heartbeat":
		ac.handleHeartbeat(msg)
	case "agent.event":
		ac.handleEvent(msg)
	default:
		log.Printf("[FrameListener] Unknown notify type: %s", msgType)
	}
}

// handleRequest 处理 Request 消息
func (ac *agentConn) handleRequest(header *MessageHeader, body []byte) {
	resp := map[string]interface{}{
		"success": true,
	}
	respBytes, _ := json.Marshal(resp)

	respHeader := MessageHeader{
		Length:   uint32(len(respBytes)),
		Sequence: header.Sequence,
		Type:     Response,
	}

	ac.mu.Lock()
	ac.writeMsgHeader(&respHeader)
	ac.conn.Write(respBytes)
	ac.mu.Unlock()
}

// handleRegister 处理 agent 注册
func (ac *agentConn) handleRegister(msg map[string]interface{}) {
	agentID, _ := msg["agentId"].(string)
	hostname, _ := msg["hostname"].(string)

	if agentID == "" {
		agentID = fmt.Sprintf("agent_%s", ac.conn.RemoteAddr().String())
	}

	ac.agentID = agentID
	ac.listener.registry.Register(agentID, hostname, ac.conn.RemoteAddr().String(), ac)
	ac.listener.registry.SetClient(agentID, ac)

	ac.sendNotify("agent.register_ack", map[string]interface{}{
		"success": true,
		"agentId": agentID,
	})

	log.Printf("[FrameListener] Agent registered: %s (%s)", agentID, hostname)
}

// handleHeartbeat 处理心跳上报
func (ac *agentConn) handleHeartbeat(msg map[string]interface{}) {
	if ac.agentID == "" {
		return
	}

	status := "online"
	if s, ok := msg["status"].(string); ok && s != "" {
		status = s
	}

	resources := msg["resources"]
	ac.listener.registry.UpdateStatus(ac.agentID, status, resources)
}

// handleEvent 处理事件上报
func (ac *agentConn) handleEvent(msg map[string]interface{}) {
	event, _ := msg["event"].(string)
	data := msg["data"]

	switch event {
	case "script_output":
		// 持久化日志
		if ac.listener.onScript != nil {
			dataMap, _ := data.(map[string]any)
			if dataMap == nil {
				dataMap = map[string]any{}
			}
			ac.listener.onScript(ac.agentID, dataMap)
		}
		ac.listener.broadcast.BroadcastEvent("script", map[string]interface{}{
			"event": "output",
			"data":  data,
		})
	case "trigger_fired":
		ac.listener.broadcast.BroadcastAgentEvent("trigger_fired", map[string]interface{}{
			"agentId": ac.agentID,
			"data":    data,
		})
	default:
		log.Printf("[FrameListener] Unknown event: %s", event)
	}
}

// sendPong 发送 PONG
func (ac *agentConn) sendPong() {
	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: 4,
		Type:   Notify,
	}
	ac.writeMsgHeader(&header)
	ac.conn.Write([]byte("PONG"))
}

// sendNotify 发送 Notify 消息
func (ac *agentConn) sendNotify(msgType string, data map[string]interface{}) {
	msg := map[string]interface{}{
		"type": msgType,
	}
	for k, v := range data {
		msg[k] = v
	}

	body, _ := json.Marshal(msg)

	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: uint32(len(body)),
		Type:   Notify,
	}
	ac.writeMsgHeader(&header)
	ac.conn.Write(body)
}

// writeMsgHeader 写入消息头
func (ac *agentConn) writeMsgHeader(h *MessageHeader) error {
	buf := make([]byte, messageHeaderSize)
	listenerEndian.PutUint32(buf[0:4], h.Length)
	listenerEndian.PutUint32(buf[4:8], h.Sequence)
	buf[8] = byte(h.Type)
	listenerEndian.PutUint32(buf[12:16], h.Reserved)

	_, err := ac.conn.Write(buf)
	return err
}

// readMsgHeader 读取消息头
func (ac *agentConn) readMsgHeader() (*MessageHeader, error) {
	buf := make([]byte, messageHeaderSize)
	if _, err := io.ReadFull(ac.conn, buf); err != nil {
		return nil, err
	}

	h := &MessageHeader{
		Length:   listenerEndian.Uint32(buf[0:4]),
		Sequence: listenerEndian.Uint32(buf[4:8]),
		Type:     MessageType(buf[8]),
		Reserved: listenerEndian.Uint32(buf[12:16]),
	}
	return h, nil
}
