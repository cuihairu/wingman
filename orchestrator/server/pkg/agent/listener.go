package agent

import (
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"maps"
	"net"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

// Broadcaster avoids circular dependency between internal/agent and pkg/websocket.
type Broadcaster interface {
	BroadcastAgentEvent(eventType string, data any)
	BroadcastEvent(eventType string, data any)
}

// AgentRegistrar avoids circular dependency.
type AgentRegistrar interface {
	Register(agentID, hostname, ip string, conn any)
	Unregister(agentID string)
	UpdateStatus(agentID string, status string, resources any)
	UpdateHeartbeat(agentID string)
	SetClient(agentID string, conn any)
}

// AgentConnection is the interface for sending commands to a runtime agent.
type AgentConnection interface {
	SendCommand(method string, data map[string]any) (map[string]any, error)
	SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error)
	Close()
}

// ScriptOutputHandler is called when script output is received.
type ScriptOutputHandler func(agentID string, data map[string]any)

// pendingResponse tracks an in-flight command awaiting a response.
type pendingResponse struct {
	ch chan *messageOrError
}

type messageOrError struct {
	msg map[string]any
	err error
}

// FrameListener accepts outbound TCP connections from runtimes.
type FrameListener struct {
	listener  net.Listener
	registry  AgentRegistrar
	broadcast Broadcaster
	conns     map[string]*agentConn
	mu        sync.RWMutex
	connCount uint64
	stopCh    chan struct{}
	stopOnce  sync.Once
	onScript  ScriptOutputHandler
}

// agentConn represents a single agent TCP connection.
type agentConn struct {
	id       string
	conn     net.Conn
	agentID  string
	listener *FrameListener

	// Only readLoop reads from conn. SendCommand registers a pending
	// response keyed by sequence number; readLoop dispatches to it.
	mu      sync.Mutex // protects writes + pending map
	nextSeq uint32
	pending map[uint32]*pendingResponse
}

// NewFrameListener creates a TCP listener for runtime connections.
func NewFrameListener(registry AgentRegistrar, broadcast Broadcaster) *FrameListener {
	return &FrameListener{
		registry:  registry,
		broadcast: broadcast,
		conns:     make(map[string]*agentConn),
		stopCh:    make(chan struct{}),
	}
}

var listenerEndian = func() binary.ByteOrder {
	var x uint16 = 0x0102
	if *(*byte)(unsafe.Pointer(&x)) == 0x02 {
		return binary.LittleEndian
	}
	return binary.BigEndian
}()

// SetScriptOutputHandler sets the callback for script output events.
func (l *FrameListener) SetScriptOutputHandler(handler ScriptOutputHandler) {
	l.onScript = handler
}

// Start begins listening for connections.
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

// Stop shuts down the listener and all connections.
func (l *FrameListener) Stop() {
	l.stopOnce.Do(func() {
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
	})
}

// acceptLoop accepts incoming connections.
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
			id:      connID,
			conn:    conn,
			listener: l,
			pending: make(map[uint32]*pendingResponse),
		}

		l.mu.Lock()
		l.conns[connID] = ac
		l.mu.Unlock()

		log.Printf("[FrameListener] New connection: %s from %s", connID, conn.RemoteAddr())
		go ac.readLoop()
	}
}

// SendCommand sends a command and waits for the response via readLoop dispatch.
func (ac *agentConn) SendCommand(method string, data map[string]any) (map[string]any, error) {
	return ac.SendCommandWithTimeout(method, data, 0)
}

// SendCommandWithTimeout sends a command with a timeout. A timeout of 0 means wait indefinitely.
func (ac *agentConn) SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error) {
	ac.mu.Lock()

	seq := ac.nextSeq
	ac.nextSeq++

	req := map[string]any{
		"type":     method,
		"method":   method,
		"sequence": float64(seq), // tell the runtime the sequence
	}
	maps.Copy(req, data)

	reqBytes, err := json.Marshal(req)
	if err != nil {
		ac.mu.Unlock()
		return nil, err
	}

	header := MessageHeader{
		Length:   uint32(len(reqBytes)),
		Type:     Request,
		Sequence: seq,
	}
	if err := ac.writeMsgHeaderLocked(&header); err != nil {
		ac.mu.Unlock()
		return nil, err
	}
	if _, err := ac.conn.Write(reqBytes); err != nil {
		ac.mu.Unlock()
		return nil, err
	}

	// Register pending response before unlocking so readLoop can't miss it.
	p := &pendingResponse{ch: make(chan *messageOrError, 1)}
	ac.pending[seq] = p
	ac.mu.Unlock()

	// Wait for readLoop to deliver the response, with optional timeout.
	var result *messageOrError
	if timeout > 0 {
		select {
		case result = <-p.ch:
			// Received response
		case <-time.After(timeout):
			result = &messageOrError{err: fmt.Errorf("command timeout after %v", timeout)}
		}
	} else {
		result = <-p.ch
	}

	// Clean up the pending entry.
	ac.mu.Lock()
	delete(ac.pending, seq)
	ac.mu.Unlock()

	return result.msg, result.err
}

// Close closes the connection.
func (ac *agentConn) Close() {
	ac.conn.Close()
}

// readLoop is the sole reader from the TCP connection.
// It dispatches Response frames to the matching SendCommand caller;
// all other frames are handled inline.
func (ac *agentConn) readLoop() {
	defer func() {
		ac.conn.Close()

		// Fail any pending commands so their goroutines don't hang.
		ac.mu.Lock()
		for seq, p := range ac.pending {
			p.ch <- &messageOrError{err: fmt.Errorf("connection closed")}
			delete(ac.pending, seq)
		}
		ac.mu.Unlock()

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

		// If this is a Response with a sequence number, dispatch to the caller.
		// Note: Sequence can be 0 for the first command, so check for Response type only.
		if header.Type == Response {
			ac.dispatchResponse(header.Sequence, body)
			continue
		}

		ac.handleMessage(header, body)
	}
}

// dispatchResponse delivers a response frame to the waiting SendCommand.
func (ac *agentConn) dispatchResponse(seq uint32, body []byte) {
	var msg map[string]any
	if err := json.Unmarshal(body, &msg); err != nil {
		// Still deliver the error so the caller doesn't hang.
		ac.mu.Lock()
		if p, ok := ac.pending[seq]; ok {
			p.ch <- &messageOrError{err: fmt.Errorf("json parse error: %w", err)}
		}
		ac.mu.Unlock()
		return
	}

	ac.mu.Lock()
	p, ok := ac.pending[seq]
	ac.mu.Unlock()

	if ok {
		p.ch <- &messageOrError{msg: msg}
	} else {
		log.Printf("[FrameListener] Orphan response for seq %d", seq)
	}
}

// handleMessage processes inbound messages that are not command responses.
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

// handleNotify processes Notify messages.
func (ac *agentConn) handleNotify(body []byte) {
	if len(body) == 4 && string(body) == "PING" {
		ac.sendPong()
		if ac.agentID != "" {
			ac.listener.registry.UpdateHeartbeat(ac.agentID)
		}
		return
	}

	var msg map[string]any
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

// handleRequest processes Request messages (runtime asking the server).
func (ac *agentConn) handleRequest(header *MessageHeader, _ []byte) {
	resp := map[string]any{
		"success": true,
	}
	respBytes, _ := json.Marshal(resp)

	respHeader := MessageHeader{
		Length:   uint32(len(respBytes)),
		Sequence: header.Sequence,
		Type:     Response,
	}

	ac.mu.Lock()
	ac.writeMsgHeaderLocked(&respHeader)
	ac.conn.Write(respBytes)
	ac.mu.Unlock()
}

// handleRegister processes agent registration.
func (ac *agentConn) handleRegister(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	hostname, _ := msg["hostname"].(string)

	if agentID == "" {
		agentID = fmt.Sprintf("agent_%s", ac.conn.RemoteAddr().String())
	}

	ac.agentID = agentID
	ac.listener.registry.Register(agentID, hostname, ac.conn.RemoteAddr().String(), ac)
	ac.listener.registry.SetClient(agentID, ac)

	ac.sendNotify("agent.register_ack", map[string]any{
		"success": true,
		"agentId": agentID,
	})

	log.Printf("[FrameListener] Agent registered: %s (%s)", agentID, hostname)
}

// handleHeartbeat processes heartbeat reports.
func (ac *agentConn) handleHeartbeat(msg map[string]any) {
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

// handleEvent processes agent events.
func (ac *agentConn) handleEvent(msg map[string]any) {
	event, _ := msg["event"].(string)
	data := msg["data"]

	switch event {
	case "script_output":
		if ac.listener.onScript != nil {
			dataMap, _ := data.(map[string]any)
			if dataMap == nil {
				dataMap = map[string]any{}
			}
			ac.listener.onScript(ac.agentID, dataMap)
		}
		ac.listener.broadcast.BroadcastEvent("script", map[string]any{
			"event": "output",
			"data":  data,
		})
	case "trigger_fired":
		ac.listener.broadcast.BroadcastAgentEvent("trigger_fired", map[string]any{
			"agentId": ac.agentID,
			"data":    data,
		})
	default:
		log.Printf("[FrameListener] Unknown event: %s", event)
	}
}

// sendPong sends a PONG reply.
func (ac *agentConn) sendPong() {
	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: 4,
		Type:   Notify,
	}
	ac.writeMsgHeaderLocked(&header)
	ac.conn.Write([]byte("PONG"))
}

// sendNotify sends a Notify message.
func (ac *agentConn) sendNotify(msgType string, data map[string]any) {
	msg := map[string]any{
		"type": msgType,
	}
	maps.Copy(msg, data)

	body, _ := json.Marshal(msg)

	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: uint32(len(body)),
		Type:   Notify,
	}
	ac.writeMsgHeaderLocked(&header)
	ac.conn.Write(body)
}

// writeMsgHeaderLocked writes a message header. Caller must hold ac.mu.
func (ac *agentConn) writeMsgHeaderLocked(h *MessageHeader) error {
	buf := make([]byte, messageHeaderSize)
	listenerEndian.PutUint32(buf[0:4], h.Length)
	listenerEndian.PutUint32(buf[4:8], h.Sequence)
	buf[8] = byte(h.Type)
	listenerEndian.PutUint32(buf[12:16], h.Reserved)

	_, err := ac.conn.Write(buf)
	return err
}

// readMsgHeader reads a message header.
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
