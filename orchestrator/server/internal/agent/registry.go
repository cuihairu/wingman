package agent

import (
	"encoding/json"
	"log"
	"strings"
	"sync"
	"time"

	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
)

// AgentConn 对 TCP 连接的抽象，用于向 agent 发送命令
type AgentConn interface {
	SendCommand(method string, data map[string]any) (map[string]any, error)
	// SendCommandWithTimeout sends a command with a timeout. A timeout of 0 means wait indefinitely.
	SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error)
}

// LinkHealth agent↔orchestrator 链路质量统计。
// runtime 在 agent.heartbeat 中上报自身视角的连接真话（重连/丢弃/掉线原因/会话时长），
// server 侧补充实测 heartbeatAgeMs（距上次心跳的毫秒数）供前端判断链路健康度。
type LinkHealth struct {
	// Reconnects 进程启动以来成功重连的累计次数（初始连接不计）
	Reconnects int `json:"reconnects"`
	// Dropped 断线期间 outbox 满导致的丢弃累计
	Dropped int `json:"dropped"`
	// OutboxPending 当前断线缓冲中待冲刷的消息数
	OutboxPending int `json:"outboxPending"`
	// LastDisconnectReason 最近一次断线原因（连接成功后保留，便于追溯）
	LastDisconnectReason string `json:"lastDisconnectReason"`
	// SessionUptimeMs 当前连接持续时长（毫秒）
	SessionUptimeMs int64 `json:"sessionUptimeMs"`
}

// AgentInfo 内存中的 Agent 信息
type AgentInfo struct {
	AgentID   string
	Hostname  string
	IP        string
	Status    AgentStatus
	Resources ResourceStats
	Link      LinkHealth
	LastSeen  time.Time
	Client    AgentConn
	Tags      []string
	// Platform 设备平台（android/desktop/...），agent.register 上报；
	// 空值视为 desktop（旧版桌面 agent 不上报）。见 docs/android-agent-design.md §3.3。
	Platform string
}

// TagStore agent 标签持久化接口（由 DB 层实现，Registry 不直接依赖 gorm）。
type TagStore interface {
	// LoadTags 返回持久化的标签；(nil, false) 表示无记录或解析失败。
	LoadTags(agentID string) ([]string, bool)
	// SaveTags 持久化标签（无记录则建行，顺带补 hostname/ip 元数据）。
	SaveTags(agentID, hostname, ip string, tags []string) error
}

// Registry Agent 内存注册表
// 实现 pkg/agent.AgentRegistrar 接口
type Registry struct {
	agents    map[string]*AgentInfo
	mu        sync.RWMutex
	hub       *ws.Hub
	heartbeat time.Duration
	// tagStore 可选的标签持久化后端，SetTagStore 注入；读写均受 mu 保护。
	tagStore TagStore
	// checkInterval 心跳巡检周期（默认 30s），测试中可缩短以触发 ticker 分支。
	checkInterval time.Duration
	stopCh        chan struct{}
	stopOnce      sync.Once
}

// NewRegistry 创建 Agent 注册表
func NewRegistry(hub *ws.Hub) *Registry {
	return &Registry{
		agents:        make(map[string]*AgentInfo),
		hub:           hub,
		heartbeat:     90 * time.Second,
		checkInterval: 30 * time.Second,
		stopCh:        make(chan struct{}),
	}
}

// SetTagStore 注入标签持久化后端（启动时调用一次，幂等）。
func (r *Registry) SetTagStore(store TagStore) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.tagStore = store
}

// tagStoreRef 在锁外取 tagStore 引用，避免 DB IO 持锁。
func (r *Registry) tagStoreRef() TagStore {
	r.mu.RLock()
	defer r.mu.RUnlock()
	return r.tagStore
}

// Register 注册 Agent（实现 pkg/agent.AgentRegistrar 接口）
// conn 参数可以是任何实现了 SendCommand 的类型。
// 标签恢复：内存中已有条目（重连）保留内存 Tags；否则从 TagStore（DB）载入，
// 使 server 重启后标签不丢失。DB IO 一律在锁外。
func (r *Registry) Register(agentID, hostname, ip string, conn any) {
	// 锁外载入持久化标签（store 为 nil 时跳过）
	var restored []string
	if store := r.tagStoreRef(); store != nil {
		if tags, ok := store.LoadTags(agentID); ok {
			restored = tags
		}
	}

	r.mu.Lock()
	defer r.mu.Unlock()

	info := &AgentInfo{
		AgentID:  agentID,
		Hostname: hostname,
		IP:       ip,
		Status:   StatusOnline,
		LastSeen: time.Now(),
		Tags:     restored,
	}
	if existing, ok := r.agents[agentID]; ok {
		info.Resources = existing.Resources
		info.Tags = existing.Tags // 重连保留内存标签，不用旧 DB 值覆盖
	}
	// 尝试将 conn 转为 AgentConn
	if ac, ok := conn.(AgentConn); ok {
		info.Client = ac
	}

	r.agents[agentID] = info

	log.Printf("[Registry] Agent registered: %s (%s@%s)", agentID, hostname, ip)

	r.hub.BroadcastAgentEvent("connected", map[string]any{
		"agentId":  agentID,
		"hostname": hostname,
		"ip":       ip,
		"status":   string(StatusOnline),
		"tags":     info.Tags,
		"lastSeen": info.LastSeen.UnixMilli(),
	})
}

// Unregister 注销 Agent
func (r *Registry) Unregister(agentID string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if info, ok := r.agents[agentID]; ok {
		info.Status = StatusOffline
		log.Printf("[Registry] Agent unregistered: %s", agentID)

		r.hub.BroadcastAgentEvent("disconnected", map[string]any{
			"agentId":  agentID,
			"hostname": info.Hostname,
			"status":   string(StatusOffline),
		})
	}
}

// UpdateStatus 更新 Agent 状态和资源（实现 pkg/agent.AgentRegistrar 接口）
// status 为字符串 "online"/"offline" 等，resources 为 any（可以是 ResourceStats 或 map）
func (r *Registry) UpdateStatus(agentID string, status string, resources any) {
	r.mu.Lock()
	defer r.mu.Unlock()

	info, ok := r.agents[agentID]
	if !ok {
		return
	}

	if status != "" {
		info.Status = AgentStatus(status)
	}
	if resources != nil {
		// 尝试解析为 ResourceStats
		resBytes, err := json.Marshal(resources)
		if err == nil {
			var rs ResourceStats
			if json.Unmarshal(resBytes, &rs) == nil {
				info.Resources = rs
			}
		}
	}
	info.LastSeen = time.Now()

	r.hub.BroadcastAgentEvent("status_changed", map[string]any{
		"agentId":   agentID,
		"hostname":  info.Hostname,
		"ip":        info.IP,
		"status":    string(info.Status),
		"resources": info.Resources,
		"link":      info.Link,
		"lastSeen":  info.LastSeen.UnixMilli(),
	})
}

// UpdateHeartbeat 更新心跳时间
func (r *Registry) UpdateHeartbeat(agentID string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if info, ok := r.agents[agentID]; ok {
		info.LastSeen = time.Now()
	}
}

// UpdateLinkHealth 解析 agent.heartbeat 携带的 link 统计并写入注册表。
// 宽容解析：字段缺失/类型不符时保持原值，不因此拒绝心跳。
func (r *Registry) UpdateLinkHealth(agentID string, raw map[string]any) {
	if raw == nil {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()

	info, ok := r.agents[agentID]
	if !ok {
		return
	}
	if v, ok := toInt(raw["reconnects"]); ok {
		info.Link.Reconnects = v
	}
	if v, ok := toInt(raw["dropped"]); ok {
		info.Link.Dropped = v
	}
	if v, ok := toInt(raw["outboxPending"]); ok {
		info.Link.OutboxPending = v
	}
	if v, ok := raw["lastDisconnectReason"].(string); ok {
		info.Link.LastDisconnectReason = v
	}
	if v, ok := toInt64(raw["sessionUptimeMs"]); ok {
		info.Link.SessionUptimeMs = v
	}
}

// toInt 宽容整数解析（JSON 数字在 Go 侧为 float64）
func toInt(value any) (int, bool) {
	switch v := value.(type) {
	case float64:
		return int(v), true
	case int:
		return v, true
	case int64:
		return int(v), true
	case json.Number:
		n, err := v.Int64()
		if err != nil {
			return 0, false
		}
		return int(n), true
	}
	return 0, false
}

// toInt64 宽容 64 位整数解析
func toInt64(value any) (int64, bool) {
	switch v := value.(type) {
	case float64:
		return int64(v), true
	case int:
		return int64(v), true
	case int64:
		return v, true
	case json.Number:
		n, err := v.Int64()
		if err != nil {
			return 0, false
		}
		return n, true
	}
	return 0, false
}

// SetTags 设置 Agent 的标签（分组），返回是否找到该 agent。
// 更新内存后经 TagStore 写穿持久化；DB 失败仅记录日志（内存为运行时真值，下次 SetTags 重写）。
func (r *Registry) SetTags(agentID string, tags []string) bool {
	// 去重 + 去空白
	seen := map[string]bool{}
	cleaned := make([]string, 0, len(tags))
	for _, t := range tags {
		t = strings.TrimSpace(t)
		if t == "" || seen[t] {
			continue
		}
		seen[t] = true
		cleaned = append(cleaned, t)
	}

	r.mu.Lock()
	info, ok := r.agents[agentID]
	if !ok {
		r.mu.Unlock()
		return false
	}
	info.Tags = cleaned
	hostname, ip := info.Hostname, info.IP

	r.hub.BroadcastAgentEvent("status_changed", map[string]any{
		"agentId":  agentID,
		"hostname": info.Hostname,
		"ip":       info.IP,
		"status":   string(info.Status),
		"tags":     info.Tags,
		"lastSeen": info.LastSeen.UnixMilli(),
	})
	r.mu.Unlock()

	// 锁外持久化
	if store := r.tagStoreRef(); store != nil {
		if err := store.SaveTags(agentID, hostname, ip, cleaned); err != nil {
			log.Printf("[Registry] Failed to persist tags for %s: %v", agentID, err)
		}
	}
	return true
}

// Get 获取指定 Agent
func (r *Registry) Get(agentID string) (*AgentInfo, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	info, ok := r.agents[agentID]
	if !ok {
		return nil, false
	}
	cp := *info
	return &cp, true
}

// List 列出所有 Agent
func (r *Registry) List() []*AgentInfo {
	r.mu.RLock()
	defer r.mu.RUnlock()

	result := make([]*AgentInfo, 0, len(r.agents))
	for _, info := range r.agents {
		cp := *info
		result = append(result, &cp)
	}
	return result
}

// GetClient 获取 Agent 的 TCP 连接
func (r *Registry) GetClient(agentID string) (AgentConn, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	info, ok := r.agents[agentID]
	if !ok || info.Client == nil {
		return nil, false
	}
	return info.Client, true
}

// SetClient 设置 Agent 的 TCP 连接（实现 pkg/agent.AgentRegistrar 接口）
func (r *Registry) SetClient(agentID string, conn any) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if info, ok := r.agents[agentID]; ok {
		if ac, ok := conn.(AgentConn); ok {
			info.Client = ac
		}
	}
}

// UpdatePlatform 记录 agent.register 上报的平台标识（实现 pkg/agent.AgentRegistrar
// 接口）。每次重连都会随 register 重新上报，这里直接覆盖即可。
func (r *Registry) UpdatePlatform(agentID string, platform string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if info, ok := r.agents[agentID]; ok {
		info.Platform = platform
	}
}

// StartHeartbeatCheck 启动心跳检测
func (r *Registry) StartHeartbeatCheck() {
	r.mu.RLock()
	interval := r.checkInterval
	r.mu.RUnlock()
	if interval <= 0 {
		interval = 30 * time.Second
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			r.checkHeartbeats()
		case <-r.stopCh:
			return
		}
	}
}

// Stop 停止注册表
func (r *Registry) Stop() {
	r.stopOnce.Do(func() {
		close(r.stopCh)
	})
}

// SetHeartbeatTimeout 调整心跳超时阈值（d 必须为正）。主要供集成测试缩短等待窗口。
func (r *Registry) SetHeartbeatTimeout(d time.Duration) {
	if d <= 0 {
		return
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	r.heartbeat = d
}

// CheckHeartbeatsNow 立即执行一次心跳检查，不等待定时器。主要供集成测试触发。
func (r *Registry) CheckHeartbeatsNow() {
	r.checkHeartbeats()
}

// checkHeartbeats 检查心跳超时
func (r *Registry) checkHeartbeats() {
	r.mu.Lock()
	defer r.mu.Unlock()

	now := time.Now()
	for _, info := range r.agents {
		if info.Status != StatusOffline && now.Sub(info.LastSeen) > r.heartbeat {
			log.Printf("[Registry] Agent heartbeat timeout: %s (last seen: %v)", info.AgentID, info.LastSeen)
			info.Status = StatusOffline

			r.hub.BroadcastAgentEvent("disconnected", map[string]any{
				"agentId":  info.AgentID,
				"hostname": info.Hostname,
				"status":   string(StatusOffline),
				"reason":   "heartbeat_timeout",
			})
		}
	}
}

// ToJSON 将 AgentInfo 序列化为前端需要的格式。
// heartbeatAgeMs 为 server 实测的距上次心跳毫秒数，接近心跳超时阈值即链路异常。
func (info *AgentInfo) ToJSON() map[string]any {
	tags := info.Tags
	if tags == nil {
		tags = []string{}
	}
	link := map[string]any{
		"reconnects":           info.Link.Reconnects,
		"dropped":              info.Link.Dropped,
		"outboxPending":        info.Link.OutboxPending,
		"lastDisconnectReason": info.Link.LastDisconnectReason,
		"sessionUptimeMs":      info.Link.SessionUptimeMs,
		"heartbeatAgeMs":       time.Since(info.LastSeen).Milliseconds(),
	}
	// 平台归一：未上报（旧版桌面 agent）按 desktop 展示
	platform := info.Platform
	if platform == "" {
		platform = "desktop"
	}
	return map[string]any{
		"agentId":     info.AgentID,
		"hostname":    info.Hostname,
		"ip":          info.IP,
		"status":      string(info.Status),
		"platform":    platform,
		"resources":   info.Resources,
		"link":        link,
		"lastSeen":    info.LastSeen.UnixMilli(),
		"currentTask": "",
		"tags":        tags,
	}
}
