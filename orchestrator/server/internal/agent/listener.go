package agent

import (
	"crypto/subtle"
	"encoding/binary"
	"fmt"
	"log"
	"net"
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
