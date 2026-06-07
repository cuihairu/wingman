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
	listener   net.Listener
	registry   AgentRegistrar
	broadcast  Broadcaster
	conns      map[string]*agentConn
	mu         sync.RWMutex
	connCount  uint64
	stopCh     chan struct{}
	stopOnce   sync.Once
	onScript   ScriptOutputHandler
	teamMgr    *TeamManager
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
		teamMgr:   NewTeamManager(),
	}
}

// SetTeamManager sets the team manager (for testing/customization).
func (l *FrameListener) SetTeamManager(tm *TeamManager) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.teamMgr = tm
}

// GetTeamManager returns the team manager.
func (l *FrameListener) GetTeamManager() *TeamManager {
	l.mu.RLock()
	defer l.mu.RUnlock()
	return l.teamMgr
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

	// Register pending response BEFORE writing to prevent race: if runtime responds
	// immediately after write, readLoop must find the pending entry.
	p := &pendingResponse{ch: make(chan *messageOrError, 1)}
	ac.pending[seq] = p

	req := map[string]any{
		"type":     method,
		"method":   method,
		"sequence": float64(seq), // tell the runtime the sequence
	}
	maps.Copy(req, data)

	reqBytes, err := json.Marshal(req)
	if err != nil {
		delete(ac.pending, seq) // Clean up on early return
		ac.mu.Unlock()
		return nil, err
	}

	header := MessageHeader{
		Length:   uint32(len(reqBytes)),
		Type:     Request,
		Sequence: seq,
	}
	if err := ac.writeMsgHeaderLocked(&header); err != nil {
		delete(ac.pending, seq) // Clean up on write failure
		ac.mu.Unlock()
		return nil, err
	}
	if _, err := ac.conn.Write(reqBytes); err != nil {
		delete(ac.pending, seq) // Clean up on write failure
		ac.mu.Unlock()
		return nil, err
	}

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
	case "inbox.register":
		ac.handleInboxRegister(msg)
	case "inbox.heartbeat":
		ac.handleInboxHeartbeat(msg)
	case "inbox.ack":
		ac.handleInboxAck(msg)
	case "inbox.report":
		ac.handleInboxReport(msg)
	case "team.join":
		ac.handleTeamJoin(msg)
	case "team.leave":
		ac.handleTeamLeave(msg)
	case "team.vote_create":
		ac.handleTeamVoteCreate(msg)
	case "team.vote_cast":
		ac.handleTeamVoteCast(msg)
	case "team.status_report":
		ac.handleTeamStatusReport(msg)
	case "team.broadcast":
		ac.handleTeamBroadcast(msg)
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

// ========== Inbox & Team Handlers ==========

// handleInboxRegister handles inbox registration.
func (ac *agentConn) handleInboxRegister(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	if agentID == "" {
		agentID = ac.agentID
	}

	// Send registration acknowledgment
	ac.sendNotify("inbox.register_ack", map[string]any{
		"success": true,
		"agentId":  agentID,
	})

	log.Printf("[Inbox] Agent %s registered", agentID)
}

// handleInboxHeartbeat handles inbox heartbeat.
func (ac *agentConn) handleInboxHeartbeat(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	if agentID != "" {
		ac.listener.registry.UpdateHeartbeat(agentID)
	}

	// Send heartbeat acknowledgment
	ac.sendNotify("inbox.heartbeat_ack", map[string]any{
		"timestamp": time.Now().UnixMilli(),
	})
}

// handleInboxAck handles message acknowledgment.
func (ac *agentConn) handleInboxAck(msg map[string]any) {
	msgID, _ := msg["msgId"].(string)
	agentID, _ := msg["agentId"].(string)
	if agentID == "" {
		agentID = ac.agentID
	}

	if ac.listener.teamMgr != nil {
		if err := ac.listener.teamMgr.AckMessage(agentID, msgID); err != nil {
			log.Printf("[Inbox] Ack failed: %v", err)
		}
	}
}

// handleInboxReport handles task completion report.
func (ac *agentConn) handleInboxReport(msg map[string]any) {
	msgID, _ := msg["msgId"].(string)
	agentID, _ := msg["agentId"].(string)
	result, _ := msg["result"].(map[string]any)

	if agentID == "" {
		agentID = ac.agentID
	}

	if ac.listener.teamMgr != nil {
		if err := ac.listener.teamMgr.ReportMessage(agentID, msgID, result); err != nil {
			log.Printf("[Inbox] Report failed: %v", err)
		}
	}
}

// handleTeamJoin handles team join request.
func (ac *agentConn) handleTeamJoin(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	agentID, _ := msg["agentId"].(string)

	if agentID == "" {
		agentID = ac.agentID
	}

	if ac.listener.teamMgr != nil {
		if err := ac.listener.teamMgr.JoinTeam(teamID, memberID, agentID); err != nil {
			log.Printf("[Team] Join failed: %v", err)
			ac.sendNotify("team.error", map[string]any{
				"error": err.Error(),
			})
		}
	}
}

// handleTeamLeave handles team leave request.
func (ac *agentConn) handleTeamLeave(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	agentID, _ := msg["agentId"].(string)

	if agentID == "" {
		agentID = ac.agentID
	}

	// If memberId is empty, use agentID as memberId
	if memberID == "" {
		memberID = agentID
	}

	if ac.listener.teamMgr != nil {
		if err := ac.listener.teamMgr.LeaveTeam(teamID, memberID); err != nil {
			log.Printf("[Team] Leave failed: %v", err)
		}
	}
}

// handleTeamVoteCreate handles vote creation request.
func (ac *agentConn) handleTeamVoteCreate(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	proposerID, _ := msg["proposerId"].(string)
	subject, _ := msg["subject"].(string)
	timeoutVal, _ := msg["timeout"].(float64)

	if ac.listener.teamMgr != nil {
		timeout := time.Duration(timeoutVal) * time.Millisecond
		if timeout == 0 {
			timeout = 30 * time.Second // Default 30 seconds
		}

		vote, err := ac.listener.teamMgr.CreateVote(teamID, proposerID, subject, timeout)
		if err != nil {
			log.Printf("[Team] Vote create failed: %v", err)
			ac.sendNotify("team.error", map[string]any{
				"error": err.Error(),
			})
		} else {
			log.Printf("[Team] Vote created: %s", vote.VoteID)
		}
	}
}

// handleTeamVoteCast handles vote casting.
func (ac *agentConn) handleTeamVoteCast(msg map[string]any) {
	voteID, _ := msg["voteId"].(string)
	memberID, _ := msg["memberId"].(string)
	response, _ := msg["response"].(string)

	if ac.listener.teamMgr != nil {
		if err := ac.listener.teamMgr.CastVote(voteID, memberID, response); err != nil {
			log.Printf("[Team] Vote cast failed: %v", err)
		}
	}
}

// handleTeamStatusReport handles status report from team member.
func (ac *agentConn) handleTeamStatusReport(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	status, _ := msg["status"].(map[string]any)

	log.Printf("[Team] Status report from %s in team %s: %v", memberID, teamID, status)

	// Broadcast status to other team members via inbox
	if ac.listener.teamMgr != nil {
		// Get team info to find other members
		if teamInfo, err := ac.listener.teamMgr.GetTeamInfo(teamID); err == nil {
			if members, ok := teamInfo["members"].([]string); ok {
				for _, mid := range members {
					if mid != memberID {
						// Find agent ID for this member
						// Note: This is simplified - in production you'd maintain a member->agent mapping
					}
				}
			}
		}
	}
}

// handleTeamBroadcast handles broadcast message to all team members.
func (ac *agentConn) handleTeamBroadcast(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	senderMemberID, _ := msg["memberId"].(string)
	message, _ := msg["message"].(map[string]any)

	log.Printf("[Team] Broadcast from %s in team %s: %v", senderMemberID, teamID, message)

	if ac.listener.teamMgr != nil {
		// Get team info to find all members
		if teamInfo, err := ac.listener.teamMgr.GetTeamInfo(teamID); err == nil {
			if members, ok := teamInfo["members"].([]string); ok {
				// Send message to all members via inbox
				for _, memberID := range members {
					// Skip the sender
					if memberID == senderMemberID {
						continue
					}

					// Create broadcast message
					broadcastMsg := map[string]any{
						"type":     "team.broadcast_received",
						"teamId":   teamID,
						"senderId": senderMemberID,
						"message":  message,
					}

					// Send via inbox (requires mapping memberId to agentId)
					// For now, we use a simple mapping: memberId -> agentId (same value)
					// In production, you'd maintain a proper memberId -> agentId mapping
					ac.listener.teamMgr.SendMessageToAgent(memberID, "team.broadcast_received", broadcastMsg)
				}
			}
		}
	}
}
