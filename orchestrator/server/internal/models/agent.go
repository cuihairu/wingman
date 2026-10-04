package models

import "gorm.io/gorm"

// Agent 持久化的 Agent 记录
type Agent struct {
	gorm.Model
	AgentID   string `gorm:"uniqueIndex;not null" json:"agentId"`
	Hostname  string `json:"hostname"`
	IP        string `json:"ip"`
	Status    string `gorm:"default:offline" json:"status"` // online/idle/busy/offline/error
	LastSeen  int64  `json:"lastSeen"`
	Resources string `gorm:"type:text" json:"-"` // JSON 序列化的 ResourceStats
	// Tags JSON 数组文本（如 ["prod","win"]），由 Registry 经 TagStore 写穿/恢复
	Tags string `gorm:"type:text" json:"-"`
	// Capabilities JSON 数组文本（如 ["screen.capture","input.mouse"]），
	// agent.register 上报，由 Registry 经 CapabilityStore 写穿/恢复；
	// 词汇表见 internal/agent/capabilities.go（ADR: Capability System）
	Capabilities string `gorm:"type:text" json:"-"`
}
