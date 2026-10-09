package agent

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"maps"
	"net"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

// Broadcaster 由 pkg/websocket 的 Hub 实现（依赖倒置：本包不导入 websocket 包）。
type Broadcaster interface {
	BroadcastAgentEvent(eventType string, data any)
	BroadcastEvent(eventType string, data any)
}

// AgentRegistrar 由同包的 Registry 实现（listener 与 registry 的解耦点）。
type AgentRegistrar interface {
	Register(agentID, hostname, ip string, conn any)
	Unregister(agentID string)
	UpdateStatus(agentID string, status string, resources any)
	UpdateHeartbeat(agentID string)
	UpdateLinkHealth(agentID string, raw map[string]any)
	UpdatePlatform(agentID string, platform string)
	UpdateCapabilities(agentID string, caps []string)
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
	teamMgr   *TeamManager
	// agentTokens agent 注册 token 白名单；空 = 关闭注册鉴权（向后兼容）。
	// 支持 多 token 并存以平滑轮换。见 docs/agent-token-auth-design.md §3。
	agentTokens []string
	// tokenStore per-agent token DB 源（A3-P2，§6.2）；nil = 未启用。
	// 与 env 白名单双源并存：任一命中即放行（演进只加不改），迁移期 env
	// 兜底、清空 env 后即纯 DB 管理面模式。
	tokenStore *TokenStore
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

	// challengeStarted CAS 门：challenge 型注册的挑战在独立 goroutine 完成，
	// 期间同连接再来的 register 一律拒绝（防并发双重注册）。
	challengeStarted atomic.Bool

	// settled CAS 门：注册终态（成功入册 / 拒绝断连）谁先到谁生效——
	// 异步挑战期间连接被并发路径断开时，挑战 goroutine 的后到结果作废，
	// 不会向 Registry 登记一条已关闭的连接。
	settled atomic.Bool
}

// NewFrameListener creates a TCP listener for runtime connections.
func NewFrameListener(registry AgentRegistrar, broadcast Broadcaster) *FrameListener {
	l := &FrameListener{
		registry:  registry,
		broadcast: broadcast,
		conns:     make(map[string]*agentConn),
		stopCh:    make(chan struct{}),
		teamMgr:   NewTeamManager(),
	}
	// 装配收件箱实时下发：TeamManager 消息入队后经此回调推送给在线 agent
	l.teamMgr.SetMessageNotifier(l.deliverInboxMessage)
	return l
}

// SetAgentTokens sets the agent register token whitelist (empty disables auth).
// 配合 SetTeamManager 风格：构造后注入，既有调用方与测试不受影响。
func (l *FrameListener) SetAgentTokens(tokens []string) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.agentTokens = tokens
}

// authEnabled reports whether register token auth is on.
func (l *FrameListener) authEnabled() bool {
	l.mu.RLock()
	defer l.mu.RUnlock()
	return len(l.agentTokens) > 0
}

// tokenValid 用 constant-time 比对校验 token（防时序侧信道逐字节猜测），
// 任一白名单项命中即通过。仅应在 authEnabled() 为真（白名单非空）时调用——
// 调用点 handleRegister 已守卫，此处不再处理空白名单。
func (l *FrameListener) tokenValid(token string) bool {
	l.mu.RLock()
	tokens := l.agentTokens
	l.mu.RUnlock()
	if token == "" {
		return false
	}
	for _, t := range tokens {
		if subtle.ConstantTimeCompare([]byte(token), []byte(t)) == 1 {
			return true
		}
	}
	return false
}

// SetTokenStore sets the per-agent token DB source (nil disables DB auth).
// 与 SetTeamManager 同风格：构造后注入，既有调用方与测试不受影响。
func (l *FrameListener) SetTokenStore(store *TokenStore) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.tokenStore = store
}

// getTokenStore returns the DB token source (nil = disabled).
func (l *FrameListener) getTokenStore() *TokenStore {
	l.mu.RLock()
	defer l.mu.RUnlock()
	return l.tokenStore
}

// storeEnabled reports whether the DB token source is configured with at
// least one issued record (revoked included — revoking the last token must
// keep auth ON, fail-closed; an empty DB means the source is not in use).
func (l *FrameListener) storeEnabled() bool {
	s := l.getTokenStore()
	if s == nil {
		return false
	}
	ok, err := s.AnyExists()
	return err == nil && ok
}

// authRequired reports whether register auth is on: env whitelist or DB
// token source, either enabled (union; empty env + empty DB = closed).
func (l *FrameListener) authRequired() bool {
	return l.authEnabled() || l.storeEnabled()
}

// SetTeamManager sets the team manager (for testing/customization).
func (l *FrameListener) SetTeamManager(tm *TeamManager) {
	// 先在锁外装配通知回调：deliverInboxMessage 在 TeamManager 锁内会获取 l.mu，
	// 此处若持 l.mu 再取 TeamManager 锁会形成反序加锁（死锁风险）。
	if tm != nil {
		tm.SetMessageNotifier(l.deliverInboxMessage)
	}
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

// determineByteOrder 依宿主机字节序返回帧编解码使用的 ByteOrder（纯函数，便于测试）。
func determineByteOrder(hostLittle bool) binary.ByteOrder {
	if hostLittle {
		return binary.LittleEndian
	}
	return binary.BigEndian
}

// hostIsLittleEndian 探测宿主机是否为小端字节序。
func hostIsLittleEndian() bool {
	var x uint16 = 0x0102
	return *(*byte)(unsafe.Pointer(&x)) == 0x02
}

var listenerEndian = determineByteOrder(hostIsLittleEndian())

// SetScriptOutputHandler sets the callback for script output events.
func (l *FrameListener) SetScriptOutputHandler(handler ScriptOutputHandler) {
	l.onScript = handler
}

func (l *FrameListener) getListener() net.Listener {
	l.mu.RLock()
	defer l.mu.RUnlock()
	return l.listener
}

// Start begins listening for connections.
func (l *FrameListener) Start(addr string) error {
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	l.mu.Lock()
	l.listener = ln
	l.mu.Unlock()
	log.Printf("[FrameListener] Listening on %s", addr)

	go l.acceptLoop()
	return nil
}

// Stop shuts down the listener and all connections.
func (l *FrameListener) Stop() {
	l.stopOnce.Do(func() {
		close(l.stopCh)
		if ln := l.getListener(); ln != nil {
			ln.Close()
		}
		l.mu.Lock()
		for id, ac := range l.conns {
			ac.conn.Close()
			delete(l.conns, id)
		}
		l.listener = nil
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

		ln := l.getListener()
		if ln == nil {
			return
		}

		conn, err := ln.Accept()
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
			pending:  make(map[uint32]*pendingResponse),
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

func (ac *agentConn) getAgentID() string {
	ac.mu.Lock()
	defer ac.mu.Unlock()
	return ac.agentID
}

func (ac *agentConn) setAgentID(agentID string) {
	ac.mu.Lock()
	ac.agentID = agentID
	ac.mu.Unlock()
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

		if agentID := ac.getAgentID(); agentID != "" {
			ac.listener.registry.Unregister(agentID)
			// 清理该 agent 的收件箱缓冲与团队成员关系（消息未送达即断连，不应残留）。
			// 仅当没有其他活跃连接仍以同一 agentID 注册时才清理，
			// 避免重连竞态下旧连接的 defer 清掉新会话的状态。
			if !ac.listener.hasOtherConnForAgent(agentID, ac.id) {
				if tm := ac.listener.GetTeamManager(); tm != nil {
					tm.RemoveAgent(agentID)
				}
			}
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
		if agentID := ac.getAgentID(); agentID != "" {
			ac.listener.registry.UpdateHeartbeat(agentID)
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
	case "proxy.data":
		ac.handleProxyData(msg)
	case "proxy.close":
		ac.handleProxyClose(msg)
	case "proxy.error":
		ac.handleProxyError(msg)
	default:
		log.Printf("[FrameListener] Unknown notify type: %s", msgType)
	}
}

// handleProxyData agent → server 方向的代理数据帧。按 proxyId 前缀路由到
// 对应中继（guac: → Guacamole 目标转发）；data 为 base64。未知前缀记日志
// 丢弃——后续端口转发等消费方在此扩路由，与 cockpit proxyMgr 同位。
func (ac *agentConn) handleProxyData(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	dataB64, _ := msg["data"].(string)
	if proxyID == "" || connID == "" {
		log.Printf("[FrameListener] proxy.data missing proxyId/connId")
		return
	}
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		log.Printf("[FrameListener] proxy.data for unknown prefix: %s", proxyID)
		return
	}
	data, err := base64.StdEncoding.DecodeString(dataB64)
	if err != nil {
		log.Printf("[FrameListener] proxy.data bad base64 for %s: %v", connID, err)
		return
	}
	if err := guacRelayDeliver(proxyID, connID, data); err != nil {
		log.Printf("[FrameListener] proxy.data deliver %s: %v", connID, err)
	}
}

// handleProxyClose agent 侧关闭（目标断开/写错误）。
func (ac *agentConn) handleProxyClose(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	reason, _ := msg["reason"].(string)
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		return // 非中继会话的关闭通知无需日志噪声
	}
	guacRelayHandleClose(proxyID, connID, reason)
}

// handleProxyError agent 拨目标失败等错误上报 → 拆 guacd 侧连接，让 connect
// 立即失败而非挂到超时。
func (ac *agentConn) handleProxyError(msg map[string]any) {
	proxyID, _ := msg["proxyId"].(string)
	connID, _ := msg["connId"].(string)
	errMsg, _ := msg["error"].(string)
	if !strings.HasPrefix(proxyID, GuacRelayPrefix) {
		log.Printf("[FrameListener] proxy.error for unknown prefix: %s", proxyID)
		return
	}
	guacRelayHandleError(proxyID, connID, errMsg)
}

// handleRequest 应答 runtime 发来的 Request 帧（runtime 主动向服务器取数）。
// 当前 runtime 的 agent 链路（inbox/team 等模块）只发送 Notify 帧，并不发起 Request；
// 此处理为协议防御性兜底：对未知 Request 回空 success:true，保持协议向后兼容
// （runtime 侧对该响应不解析额外字段）。后续若 runtime 依赖具体 Request 方法取数，
// 应在此按 method 分发实现，而不是移除该兜底。
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
	// 平台标识（android/desktop/...）：桌面 agent 旧版本不上报，缺省由
	// Registry 归一为 desktop。见 docs/android-agent-design.md §3.3。
	platform, _ := msg["platform"].(string)

	// 能力集（agent.register 上报，ADR: Capability System）：宽容解析，
	// 字段缺失/类型不符按空集处理，不因此拒绝注册；词汇表外的未知项
	// 照常存储（见 registry.ToJSON 的 unknownCapabilities）。
	capList := []string{}
	if rawCaps, ok := msg["capabilities"].([]any); ok {
		for _, rc := range rawCaps {
			if s, ok := rc.(string); ok && s != "" {
				capList = append(capList, s)
			}
		}
	}

	// 注册鉴权（docs/agent-token-auth-design.md §2/§6）三条路径：
	// ① challenge-response（register 带 challenge:true、不携明文 token，
	//    §6.1）：下发 auth.challenge{nonce}，agent 回 HMAC-SHA256
	//    （key = sha256hex(token)，与 DB 哈希列同值，服务端无需持有明文）。
	//    挑战在独立 goroutine 完成——handleRegister 由 readLoop 驱动，
	//    同步等 Response 会自锁。
	// ② 顶层 token 明文（env 白名单 ∪ DB 哈希，P1/P2.2，双源任一命中放行）。
	// ③ 关闭（env 空 + 无 DB 记录，向后兼容）。
	// 失败路径统一：ack success:false 后立即断连——不 set agentID、不入
	// Registry，未授权连接不允许停留在链路上（readLoop 退出时 agentID 为空
	// 自然跳过 Unregister）。
	if ac.listener.authRequired() {
		if challenge, _ := msg["challenge"].(bool); challenge {
			if !ac.challengeStarted.CompareAndSwap(false, true) {
				ac.rejectRegister(agentID)
				return
			}
			go func() {
				if ac.verifyChallengeHMAC(agentID) {
					ac.completeRegistration(agentID, hostname, platform, capList)
				} else {
					ac.rejectRegister(agentID)
				}
			}()
			return
		}
		token, _ := msg["token"].(string)
		envOK := ac.listener.tokenValid(token)
		storeOK := false
		if store := ac.listener.getTokenStore(); store != nil {
			_, storeOK = store.Verify(token, agentID)
		}
		if !envOK && !storeOK {
			ac.rejectRegister(agentID)
			return
		}
	}

	ac.completeRegistration(agentID, hostname, platform, capList)
}

// completeRegistration 落 Registry 并回注册成功 ack（challenge 异步路径与
// 同步路径共用收尾）。settled CAS 保证注册终态只生效一次。
func (ac *agentConn) completeRegistration(agentID, hostname, platform string, capList []string) {
	if !ac.settled.CompareAndSwap(false, true) {
		return
	}
	if agentID == "" {
		agentID = fmt.Sprintf("agent_%s", ac.conn.RemoteAddr().String())
	}

	ac.setAgentID(agentID)
	ac.listener.registry.Register(agentID, hostname, ac.conn.RemoteAddr().String(), ac)
	ac.listener.registry.UpdatePlatform(agentID, platform)
	ac.listener.registry.UpdateCapabilities(agentID, capList)
	ac.listener.registry.SetClient(agentID, ac)

	ac.sendNotify("agent.register_ack", map[string]any{
		"success": true,
		"agentId": agentID,
	})

	log.Printf("[FrameListener] Agent registered: %s (%s)", agentID, hostname)
}

// rejectRegister 注册拒绝统一路径：ack success:false + 断连 + 日志。
func (ac *agentConn) rejectRegister(agentID string) {
	if !ac.settled.CompareAndSwap(false, true) {
		return
	}
	ac.sendNotify("agent.register_ack", map[string]any{
		"success": false,
		"agentId": agentID,
		"error":   "invalid or missing token",
	})
	log.Printf("[Auth] register rejected from %s (agentId=%q)",
		ac.conn.RemoteAddr(), agentID)
	ac.conn.Close()
}

// challengeTimeout auth.challenge 等待应答的超时；var 供测试缩短。
var challengeTimeout = 5 * time.Second

// verifyChallengeHMAC challenge-response 校验（§6.1）：下发 32 字节 nonce，
// 等 agent 回 HMAC-SHA256(key, nonce)（key = sha256hex(token)，agent 侧
// 自行推导、明文不过网；服务端 env 走明文派生、DB 直接用哈希列）。
func (ac *agentConn) verifyChallengeHMAC(agentID string) bool {
	raw := make([]byte, 32)
	if _, err := rand.Read(raw); err != nil {
		log.Printf("[Auth] challenge nonce generation failed: %v", err)
		return false
	}
	nonceHex := hex.EncodeToString(raw)

	resp, err := ac.SendCommandWithTimeout("auth.challenge",
		map[string]any{"nonce": nonceHex}, challengeTimeout)
	if err != nil {
		log.Printf("[Auth] challenge failed from %s (agentId=%q): %v",
			ac.conn.RemoteAddr(), agentID, err)
		return false
	}
	macHex, _ := resp["hmac"].(string)
	if ac.listener.hmacMatchesEnv(macHex, nonceHex) {
		return true
	}
	if store := ac.listener.getTokenStore(); store != nil {
		if _, ok := store.VerifyHMAC(macHex, nonceHex, agentID); ok {
			return true
		}
	}
	return false
}

// tokenMAC 以 keyHex（token 的 sha256 hex 或 DB 哈希列）为 HMAC-SHA256
// 密钥、nonceHex 为消息，返回 hex 小写 MAC。
func tokenMAC(keyHex, nonceHex string) string {
	h := hmac.New(sha256.New, []byte(keyHex))
	h.Write([]byte(nonceHex))
	return hex.EncodeToString(h.Sum(nil))
}

// sha256Hex returns the lowercase hex sha256 of s.
func sha256Hex(s string) string {
	sum := sha256.Sum256([]byte(s))
	return hex.EncodeToString(sum[:])
}

// hmacMatchesEnv challenge 应答对 env 白名单的校验（明文派生密钥）。
func (l *FrameListener) hmacMatchesEnv(macHex, nonceHex string) bool {
	if macHex == "" {
		return false
	}
	l.mu.RLock()
	tokens := l.agentTokens
	l.mu.RUnlock()
	for _, t := range tokens {
		if subtle.ConstantTimeCompare([]byte(macHex), []byte(tokenMAC(sha256Hex(t), nonceHex))) == 1 {
			return true
		}
	}
	return false
}

// handleHeartbeat processes heartbeat reports.
func (ac *agentConn) handleHeartbeat(msg map[string]any) {
	agentID := ac.getAgentID()
	if agentID == "" {
		return
	}

	status := "online"
	if s, ok := msg["status"].(string); ok && s != "" {
		status = s
	}

	resources := msg["resources"]
	// 链路质量统计（reconnects/dropped/outboxPending/lastDisconnectReason/sessionUptimeMs）：
	// 先于 UpdateStatus 写入，随 status_changed 广播携带给 Dashboard。
	if link, ok := msg["link"].(map[string]any); ok {
		ac.listener.registry.UpdateLinkHealth(agentID, link)
	}
	ac.listener.registry.UpdateStatus(agentID, status, resources)
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
			ac.listener.onScript(ac.getAgentID(), dataMap)
		}
		ac.listener.broadcast.BroadcastEvent("script", map[string]any{
			"event": "output",
			"data":  data,
		})
	case "trigger_fired":
		ac.listener.broadcast.BroadcastAgentEvent("trigger_fired", map[string]any{
			"agentId": ac.getAgentID(),
			"data":    data,
		})
	case "script_state":
		// runtime 推送的脚本状态变更（running/paused/stopped/error），转发给 Dashboard。
		ac.listener.broadcast.BroadcastEvent("script", map[string]any{
			"event":   "state_changed",
			"agentId": ac.getAgentID(),
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

// getConnByAgent 返回该 agentID 当前活跃的连接；agent 离线时返回 nil。
// 锁序：l.mu → ac.mu（getAgentID）。仓库中不存在 ac.mu → l.mu 的路径，安全。
func (l *FrameListener) getConnByAgent(agentID string) *agentConn {
	l.mu.RLock()
	defer l.mu.RUnlock()
	for _, ac := range l.conns {
		if ac.getAgentID() == agentID {
			return ac
		}
	}
	return nil
}

// hasOtherConnForAgent 判断除 excludeConnID 外是否还有活跃连接以该 agentID 注册。
func (l *FrameListener) hasOtherConnForAgent(agentID, excludeConnID string) bool {
	l.mu.RLock()
	defer l.mu.RUnlock()
	for id, ac := range l.conns {
		if id != excludeConnID && ac.getAgentID() == agentID {
			return true
		}
	}
	return false
}

// deliverInboxMessage 把新入队的收件箱消息实时推送给在线 agent（TeamManager 通知回调）。
// agent 离线时不推送，消息保留在内存缓冲中；断连时由 readLoop 清理缓冲。
// 契约与 runtime inbox 模块一致（lib/wingman/src/script/modules/inbox_module.cpp
// handleMessage/handleInboxMessage）：notify type = "inbox.message"，
// 字段 msgId / messageType / payload / timestamp。
// 注意：调用方持有 TeamManager 锁，此处只允许获取 l.mu / ac.mu，
// 不得重入 TeamManager 的加锁方法。
func (l *FrameListener) deliverInboxMessage(agentID string, msg *InboxMessage) {
	ac := l.getConnByAgent(agentID)
	if ac == nil {
		return // agent 离线：保持内存缓冲
	}
	ac.sendNotify("inbox.message", map[string]any{
		"msgId":       msg.MsgID,
		"messageType": msg.Type,
		"payload":     msg.Payload,
		"timestamp":   msg.Timestamp.UnixMilli(),
	})
}

// SendNotify 导出的 Notify 帧发送口（代理协议面：中继经 AgentConn 抽象向
// agent 推 proxy.* 帧）。写入失败如实返回——中继以此判定拆链，静默会让
// guacd 侧挂到超时。
func (ac *agentConn) SendNotify(msgType string, data map[string]any) error {
	msg := map[string]any{
		"type": msgType,
	}
	maps.Copy(msg, data)

	body, err := json.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal notify %s: %w", msgType, err)
	}

	ac.mu.Lock()
	defer ac.mu.Unlock()

	header := MessageHeader{
		Length: uint32(len(body)),
		Type:   Notify,
	}
	if err := ac.writeMsgHeaderLocked(&header); err != nil {
		return err
	}
	_, err = ac.conn.Write(body)
	return err
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
		agentID = ac.getAgentID()
	}

	// Send registration acknowledgment
	ac.sendNotify("inbox.register_ack", map[string]any{
		"success": true,
		"agentId": agentID,
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
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.AckMessage(agentID, msgID); err != nil {
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
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.ReportMessage(agentID, msgID, result); err != nil {
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
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.JoinTeam(teamID, memberID, agentID); err != nil {
			log.Printf("[Team] Join failed: %v", err)
			ac.sendNotify("team.error", map[string]any{
				"error": err.Error(),
			})
		}
	}
}

// handleTeamLeave handles team leave request.
func (ac *agentConn) handleTeamLeave(msg map[string]any) {
	// 快照一次并先做 nil 检查：SetTeamManager(nil)（测试/降级场景）下
	// 下方的 FindMemberByAgent 会空指针；且 readLoop 与 SetTeamManager
	// 并发，裸读 teamMgr 字段构成数据竞争（race detector 报警）。
	tm := ac.listener.GetTeamManager()
	if tm == nil {
		return
	}

	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	agentID, _ := msg["agentId"].(string)

	if agentID == "" {
		agentID = ac.getAgentID()
	}

	// If memberId is empty, resolve it from the member -> agent mapping,
	// falling back to using agentID directly (same-value mapping).
	if memberID == "" {
		if mid, found := tm.FindMemberByAgent(teamID, agentID); found {
			memberID = mid
		} else {
			memberID = agentID
		}
	}

	if err := tm.LeaveTeam(teamID, memberID); err != nil {
		log.Printf("[Team] Leave failed: %v", err)
	}
}

// handleTeamVoteCreate handles vote creation request.
func (ac *agentConn) handleTeamVoteCreate(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	proposerID, _ := msg["proposerId"].(string)
	subject, _ := msg["subject"].(string)
	timeoutVal, _ := msg["timeout"].(float64)

	if tm := ac.listener.GetTeamManager(); tm != nil {
		timeout := time.Duration(timeoutVal) * time.Millisecond
		if timeout == 0 {
			timeout = 30 * time.Second // Default 30 seconds
		}

		vote, err := tm.CreateVote(teamID, proposerID, subject, timeout)
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

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.CastVote(voteID, memberID, response); err != nil {
			log.Printf("[Team] Vote cast failed: %v", err)
		}
	}
}

// handleTeamStatusReport 处理成员状态上报，并把状态转发给团队其他成员。
// 转发以收件箱消息承载（type=team.status_report，payload 保留 teamId/memberId/status
// 原始字段）：runtime team 模块（team_module.cpp handleServerMessage）未定义独立的
// status 推送类型，收件箱通道按 messageType 分发给脚本。
func (ac *agentConn) handleTeamStatusReport(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	status, _ := msg["status"].(map[string]any)

	log.Printf("[Team] Status report from %s in team %s: %v", memberID, teamID, status)

	tm := ac.listener.GetTeamManager()
	if tm == nil {
		return
	}

	// 经 memberID -> agentID 映射转发给除上报者外的全部在线/离线成员
	memberAgents, err := tm.GetMemberAgents(teamID)
	if err != nil {
		return
	}
	for mid, agentID := range memberAgents {
		if mid == memberID {
			continue // 跳过上报者自己
		}
		tm.SendMessageToAgent(agentID, "team.status_report", map[string]any{
			"type":     "team.status_report",
			"teamId":   teamID,
			"memberId": memberID,
			"status":   status,
		})
	}
}

// handleTeamBroadcast handles broadcast message to all team members.
func (ac *agentConn) handleTeamBroadcast(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	senderMemberID, _ := msg["memberId"].(string)
	message, _ := msg["message"].(map[string]any)

	log.Printf("[Team] Broadcast from %s in team %s: %v", senderMemberID, teamID, message)

	if tm := ac.listener.GetTeamManager(); tm != nil {
		// Get member -> agent mapping to resolve inbox targets
		if memberAgents, err := tm.GetMemberAgents(teamID); err == nil {
			// Send message to all members via inbox
			for memberID, agentID := range memberAgents {
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

				tm.SendMessageToAgent(agentID, "team.broadcast_received", broadcastMsg)
			}
		}
	}
}
