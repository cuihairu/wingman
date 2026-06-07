package agent

import (
	"encoding/json"
	"log"
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

// AgentInfo 内存中的 Agent 信息
type AgentInfo struct {
	AgentID   string
	Hostname  string
	IP        string
	Status    AgentStatus
	Resources ResourceStats
	LastSeen  time.Time
	Client    AgentConn
}

// Registry Agent 内存注册表
// 实现 pkg/agent.AgentRegistrar 接口
type Registry struct {
	agents    map[string]*AgentInfo
	mu        sync.RWMutex
	hub       *ws.Hub
	heartbeat time.Duration
	stopCh    chan struct{}
	stopOnce  sync.Once
}

// NewRegistry 创建 Agent 注册表
func NewRegistry(hub *ws.Hub) *Registry {
	return &Registry{
		agents:    make(map[string]*AgentInfo),
		hub:       hub,
		heartbeat: 90 * time.Second,
		stopCh:    make(chan struct{}),
	}
}

// Register 注册 Agent（实现 pkg/agent.AgentRegistrar 接口）
// conn 参数可以是任何实现了 SendCommand 的类型
func (r *Registry) Register(agentID, hostname, ip string, conn any) {
	r.mu.Lock()
	defer r.mu.Unlock()

	info := &AgentInfo{
		AgentID:  agentID,
		Hostname: hostname,
		IP:       ip,
		Status:   StatusOnline,
		LastSeen: time.Now(),
	}
	if existing, ok := r.agents[agentID]; ok {
		info.Resources = existing.Resources
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

// StartHeartbeatCheck 启动心跳检测
func (r *Registry) StartHeartbeatCheck() {
	ticker := time.NewTicker(30 * time.Second)
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

// ToJSON 将 AgentInfo 序列化为前端需要的格式
func (info *AgentInfo) ToJSON() map[string]any {
	return map[string]any{
		"agentId":     info.AgentID,
		"hostname":    info.Hostname,
		"ip":          info.IP,
		"status":      string(info.Status),
		"resources":   info.Resources,
		"lastSeen":    info.LastSeen.UnixMilli(),
		"currentTask": "",
	}
}
