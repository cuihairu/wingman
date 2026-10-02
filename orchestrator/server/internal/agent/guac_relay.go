package agent

import (
	"encoding/base64"
	"fmt"
	"io"
	"log"
	"net"
	"sync"
	"sync/atomic"
)

// Guacamole 目标转发（设计修正 2026-10-02，自 cockpit guac_relay.go 移植）：
// guacd 的目标连接不再直拨 hostname——server 常与目标不同网段（guacd 在云上、
// 目标在 agent 所在内网），云/NAT 拓扑下直拨必败。每条 Guacamole 会话起一个
// 回环中继 listener：guacd 拨中继，字节流经 agent TCP 链路的代理协议
// （proxy.new/proxy.data/proxy.close Notify 帧，data 为 base64），agent 从
// 自己网络位拨 target。hostname 语义随之变为「agent 侧可达的地址」。
//
// 载荷形状与 cockpit 的代理管线保持一致（proxyId/proxyType/connId/newConn、
// data base64），便于两项目的 agent 实现共享协议语义（DG-6 共享基建）。
//
// 网络拓扑约束：中继绑 server 本机回环，guacd 必须与 server 同机（或共享
// 回环可见性）——与录制目录同路径双挂（WINGMAN_GUACD_RECORDING_PATH）是
// 同一前提，见 deployments/desktop/README.md 的部署拓扑。

// GuacRelayPrefix Guacamole 转发会话的 proxyId 前缀。server 侧 proxy.data/
// proxy.close/proxy.error 按前缀路由（同 logs:/inbox. 等类型命名惯例），不进
// 端口转发的配置面。
const GuacRelayPrefix = "guac:"

// GuacRelaySendFunc server → agent 方向的 Notify 帧发送口。
type GuacRelaySendFunc func(agentID, msgType string, data map[string]any) error

// NotifyRegistry 中继发送所需的注册表能力（*Registry 满足）。
type NotifyRegistry interface {
	GetClient(agentID string) (AgentConn, bool)
}

// notifySender 支持推送 Notify 帧的连接（*agentConn 实现；测试假连接可不实现，
// 此时按「agent 不支持代理协议」失败）。
type notifySender interface {
	SendNotify(msgType string, data map[string]any) error
}

// GuacRelaySendToAgent 经注册表查找 agent 连接并发送 Notify 帧。
// agent 不在线 / 连接不支持 Notify 推送时返回错误，中继随即拆链——
// guacd 侧表现为 connect 失败，而非挂到超时。
func GuacRelaySendToAgent(registry NotifyRegistry, agentID, msgType string, data map[string]any) error {
	if registry == nil {
		return fmt.Errorf("registry unavailable")
	}
	conn, ok := registry.GetClient(agentID)
	if !ok || conn == nil {
		return fmt.Errorf("agent %s not connected", agentID)
	}
	sender, ok := conn.(notifySender)
	if !ok {
		return fmt.Errorf("agent %s connection does not support notify push", agentID)
	}
	return sender.SendNotify(msgType, data)
}

// GuacRelay 一条 Guacamole 会话的目标转发器。
type GuacRelay struct {
	agentID string
	proxyID string
	target  string // agent 侧视角的 host:port
	send    GuacRelaySendFunc

	ln net.Listener

	mu    sync.Mutex
	conns map[string]net.Conn // connID → guacd 侧连接
	seq   atomic.Uint64

	closeOnce sync.Once
}

// guacRelayReg 会话级转发器注册表（proxyID → relay）。包级同 terminal 会话
// 惯例；agentConn.handleNotify 的 proxy.* 分支按前缀路由到这里。测试可用
// 真实 handleNotify 分发路径（listener.go）验证回程路由，无需 Server 句柄。
var (
	guacRelayRegMu sync.Mutex
	guacRelayReg   = map[string]*GuacRelay{}
)

func guacRelayLookup(proxyID string) *GuacRelay {
	guacRelayRegMu.Lock()
	defer guacRelayRegMu.Unlock()
	return guacRelayReg[proxyID]
}

// StartGuacRelay 起中继 listener。返回 relay 与「guacd 应拨的地址」
// （127.0.0.1:<ephemeral>，进 connect 指令的 hostname/port 参数）。
func StartGuacRelay(agentID, proxyID, target string, send GuacRelaySendFunc) (*GuacRelay, string, error) {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		return nil, "", fmt.Errorf("listen relay: %w", err)
	}
	r := &GuacRelay{
		agentID: agentID,
		proxyID: proxyID,
		target:  target,
		send:    send,
		ln:      ln,
		conns:   make(map[string]net.Conn),
	}
	guacRelayRegMu.Lock()
	guacRelayReg[proxyID] = r
	guacRelayRegMu.Unlock()
	go r.acceptLoop()
	return r, ln.Addr().String(), nil
}

// Close 幂等关闭：listener + 全部 guacd 侧连接（并逐一通知 agent 拆链）。
func (r *GuacRelay) Close() {
	r.closeOnce.Do(func() {
		guacRelayRegMu.Lock()
		delete(guacRelayReg, r.proxyID)
		guacRelayRegMu.Unlock()
		_ = r.ln.Close()
		r.mu.Lock()
		connIDs := make([]string, 0, len(r.conns))
		for id := range r.conns {
			connIDs = append(connIDs, id)
		}
		r.mu.Unlock()
		for _, id := range connIDs {
			r.removeConn(id, "session closed")
		}
	})
}

func (r *GuacRelay) acceptLoop() {
	for {
		conn, err := r.ln.Accept()
		if err != nil {
			return // listener 已关（会话结束）
		}
		go r.handleConn(conn)
	}
}

// handleConn guacd 的一次拨入：注册连接 → 让 agent 拨目标 → 起 guacd→agent 泵。
// 注册先于 proxy.new：agent 回程数据到达时 connID 必已可路由。
func (r *GuacRelay) handleConn(conn net.Conn) {
	connID := fmt.Sprintf("%s-%d", r.proxyID, r.seq.Add(1))
	r.mu.Lock()
	r.conns[connID] = conn
	r.mu.Unlock()

	// 裸 TCP 分支：agent 不解析协议内容，协议端点是 guacd（SSH/RDP/VNC 都是）
	if err := r.send(r.agentID, "proxy.new", map[string]any{
		"proxyId":   r.proxyID,
		"proxyType": "tcp",
		"target":    r.target,
		"connId":    connID,
		"newConn":   true,
	}); err != nil {
		log.Printf("Guacamole relay: proxy.new to agent %s failed: %v", r.agentID, err)
		r.removeConn(connID, "agent unavailable")
		return
	}
	r.pumpToAgent(conn, connID)
}

// pumpToAgent guacd → agent 方向泵。出口（EOF/错误）即拆链并通知 agent。
// data 在入队/写帧前必须拷贝（buf 是复用读缓冲，异步序列化前会被下次
// Read 覆写——cockpit 0767b8b 的 RDP 间歇崩即此竞态，移植必带）。
func (r *GuacRelay) pumpToAgent(conn net.Conn, connID string) {
	defer r.removeConn(connID, "guacd closed")
	buf := make([]byte, 32*1024)
	for {
		n, err := conn.Read(buf)
		if n > 0 {
			payload := make([]byte, n)
			copy(payload, buf[:n])
			if err := r.send(r.agentID, "proxy.data", map[string]any{
				"proxyId": r.proxyID,
				"connId":  connID,
				"data":    base64.StdEncoding.EncodeToString(payload),
			}); err != nil {
				log.Printf("Guacamole relay: send data to agent %s failed: %v", r.agentID, err)
				return
			}
		}
		if err != nil {
			if err != io.EOF {
				log.Printf("Guacamole relay: read from guacd side %s failed: %v", connID, err)
			}
			return
		}
	}
}

// removeConn 拆一条连接：关 guacd 侧 socket + 通知 agent（幂等——agent 侧
// 先关时会收到对不存在 connID 的 proxy.close，静默忽略）。
func (r *GuacRelay) removeConn(connID, reason string) {
	r.mu.Lock()
	conn, ok := r.conns[connID]
	if ok {
		delete(r.conns, connID)
	}
	r.mu.Unlock()
	if !ok {
		return
	}
	log.Printf("Guacamole relay: tearing down %s (%s)", connID, reason)
	_ = conn.Close()
	_ = r.send(r.agentID, "proxy.close", map[string]any{
		"proxyId": r.proxyID,
		"connId":  connID,
		"reason":  reason,
	})
}

// guacRelayDeliver agent → guacd 方向（handleNotify 的 proxy.data 路由进）。
func guacRelayDeliver(proxyID, connID string, data []byte) error {
	r := guacRelayLookup(proxyID)
	if r == nil {
		return fmt.Errorf("relay %s not found", proxyID)
	}
	r.mu.Lock()
	conn, ok := r.conns[connID]
	r.mu.Unlock()
	if !ok {
		return fmt.Errorf("relay conn %s not found", connID)
	}
	_, err := conn.Write(data)
	if err != nil {
		r.removeConn(connID, "write to guacd failed")
	}
	return err
}

// guacRelayHandleClose agent 侧关闭（目标断开/写错误）→ 关 guacd 侧连接，
// guacd 随之向浏览器走协议层错误路径。
func guacRelayHandleClose(proxyID, connID, reason string) {
	r := guacRelayLookup(proxyID)
	if r == nil {
		return
	}
	r.removeConn(connID, "target closed: "+reason)
}

// guacRelayHandleError agent 拨目标失败 → 关 guacd 侧连接，让 guacd 的
// connect 立即失败而非挂到超时。
func guacRelayHandleError(proxyID, connID, errMsg string) {
	r := guacRelayLookup(proxyID)
	if r == nil {
		return
	}
	if connID == "" {
		// 无 connId 的错误无法定位连接，整条中继拆掉（guacd connect 必败）
		log.Printf("Guacamole relay: agent error (no connId), closing relay %s: %s", proxyID, errMsg)
		r.Close()
		return
	}
	log.Printf("Guacamole relay: agent dial failed for %s: %s", connID, errMsg)
	r.removeConn(connID, "agent dial failed: "+errMsg)
}
