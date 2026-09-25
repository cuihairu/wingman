// Package remoteticket 提供远程桌面（Guacamole 像素面）的一次性短时效连接票据。
//
// 设计对齐 docs/remote-gateway-guacamole-design.md §8 安全模型：
//   - 票据 5 分钟有效、ValidateTicket 即消费（一次性），泄漏窗口有界；
//   - 连接参数（协议/host/port/凭据）随票据携带，凭据只存在于
//     server → guacd 的 connect 指令里，不下发浏览器；
//   - WS 握手是票据的唯一消费点，断线重连须重新申请（与 terminal/desktop
//     的重连行为一致）。
package remoteticket

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"sync"
	"time"
)

// TTL 票据有效期。一次性语义下这只是泄漏窗口的上界。
const TTL = 5 * time.Minute

// 常见错误（errors.Is 可判）。
var (
	ErrNotFound = errors.New("remoteticket: ticket not found")
	ErrExpired  = errors.New("remoteticket: ticket expired")
	ErrConsumed = errors.New("remoteticket: ticket already consumed")
)

// Ticket 一次远程桌面连接的握手凭证。
type Ticket struct {
	ID        string
	UserID    uint
	Username  string
	Params    map[string]string
	ExpiresAt time.Time
	Consumed  bool
}

// Manager 票据管理器。零依赖（不碰 DB/网络），便于单测；
// 后台清扫 goroutine 防止长期运行下过期票据累积。
type Manager struct {
	mu            sync.Mutex
	tickets       map[string]*Ticket
	sweepInterval time.Duration
	stopCh        chan struct{}
	stopOnce      sync.Once
}

// NewManager 创建管理器并启动过期清扫（Stop 停止；测试可传短周期）。
func NewManager() *Manager {
	return newManagerWithSweep(TTL)
}

func newManagerWithSweep(interval time.Duration) *Manager {
	m := &Manager{
		tickets:       make(map[string]*Ticket),
		sweepInterval: interval,
		stopCh:        make(chan struct{}),
	}
	go m.sweepLoop()
	return m
}

// Stop 停止后台清扫。幂等。
func (m *Manager) Stop() {
	m.stopOnce.Do(func() { close(m.stopCh) })
}

// GenerateTicket 签发一张新票据。params 携带连接参数（protocol/host/port/
// username/password/width/height/readOnly 等），由调用方（票据 REST handler）
// 负责校验；Manager 只做透明携带。
func (m *Manager) GenerateTicket(userID uint, username string, params map[string]string) (*Ticket, error) {
	buf := make([]byte, 16)
	if _, err := rand.Read(buf); err != nil {
		return nil, err
	}
	t := &Ticket{
		ID:        hex.EncodeToString(buf),
		UserID:    userID,
		Username:  username,
		Params:    params,
		ExpiresAt: time.Now().Add(TTL),
	}
	m.mu.Lock()
	m.tickets[t.ID] = t
	m.mu.Unlock()
	return t, nil
}

// ValidateTicket 校验并消费票据（一次性）。不满足条件返回 nil 与具体错误。
func (m *Manager) ValidateTicket(id string) (*Ticket, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	t, ok := m.tickets[id]
	if !ok {
		return nil, ErrNotFound
	}
	// 一次性语义：无论后续是否过期，先消费再判。
	m.deleteLocked(id)
	if time.Now().After(t.ExpiresAt) {
		return nil, ErrExpired
	}
	t.Consumed = true
	return t, nil
}

func (m *Manager) deleteLocked(id string) {
	delete(m.tickets, id)
}

func (m *Manager) sweepLoop() {
	ticker := time.NewTicker(m.sweepInterval)
	defer ticker.Stop()
	for {
		select {
		case <-m.stopCh:
			return
		case <-ticker.C:
			m.sweepOnce()
		}
	}
}

func (m *Manager) sweepOnce() {
	now := time.Now()
	m.mu.Lock()
	defer m.mu.Unlock()
	for id, t := range m.tickets {
		if now.After(t.ExpiresAt) {
			delete(m.tickets, id)
		}
	}
}
