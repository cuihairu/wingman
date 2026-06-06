package agent

// AgentStatus Agent 状态
type AgentStatus string

const (
	StatusOnline  AgentStatus = "online"
	StatusIdle    AgentStatus = "idle"
	StatusBusy    AgentStatus = "busy"
	StatusOffline AgentStatus = "offline"
	StatusError   AgentStatus = "error"
)

// ResourceStats 资源统计
type ResourceStats struct {
	CPU     CPUStats     `json:"cpu"`
	Memory  MemoryStats  `json:"memory"`
	Disk    DiskStats    `json:"disk"`
	Network NetworkStats `json:"network"`
	System  SystemStats  `json:"system"`
}

// CPUStats CPU 统计
type CPUStats struct {
	Usage float64 `json:"usage"`
	Cores int     `json:"cores"`
	Model string  `json:"model"`
}

// MemoryStats 内存统计
type MemoryStats struct {
	Total     uint64  `json:"total"`
	Available uint64  `json:"available"`
	Usage     float64 `json:"usage"`
}

// DiskStats 磁盘统计
type DiskStats struct {
	Total     uint64  `json:"total"`
	Available uint64  `json:"available"`
	Usage     float64 `json:"usage"`
}

// NetworkStats 网络统计
type NetworkStats struct {
	Up      uint64 `json:"up"`
	Down    uint64 `json:"down"`
	LocalIP string `json:"localIp"`
	PublicIP string `json:"publicIp"`
}

// SystemStats 系统信息
type SystemStats struct {
	Temperature float64 `json:"temperature"`
	OS          string  `json:"os"`
	Arch        string  `json:"arch"`
}
