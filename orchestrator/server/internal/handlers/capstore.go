package handlers

import (
	"encoding/json"
	"log"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// agentCapabilityStore models.Agent.Capabilities 列（JSON 文本）的读写实现。
// Registry 经 agent.CapabilityStore 接口使用，避免 Registry 直接依赖 gorm
// （与 tagstore.go 的 TagStore 同构，DB IO 一律在锁外）。
type agentCapabilityStore struct {
	db *gorm.DB
}

// NewAgentCapabilityStore 构造基于 models.Agent 表的能力集持久化后端。
// CapabilityStore 接口不含 hostname/ip，建行时留空（注册流程由 TagStore
// 顺带补全元数据）。
func NewAgentCapabilityStore(db *gorm.DB) agent.CapabilityStore {
	return agentCapabilityStore{db: db}
}

// LoadCapabilities 读取持久化能力集；无记录或解析失败返回 (nil, false)。
func (s agentCapabilityStore) LoadCapabilities(agentID string) ([]string, bool) {
	if s.db == nil {
		return nil, false
	}
	var rec models.Agent
	// Find 不产生 ErrRecordNotFound，以主键是否为零判断命中
	s.db.Select("id", "capabilities").Where("agent_id = ?", agentID).Limit(1).Find(&rec)
	if rec.ID == 0 || rec.Capabilities == "" {
		return nil, false
	}
	var caps []string
	if err := json.Unmarshal([]byte(rec.Capabilities), &caps); err != nil {
		log.Printf("[CapabilityStore] Failed to parse capabilities for %s: %v", agentID, err)
		return nil, false
	}
	return caps, true
}

// SaveCapabilities 持久化能力集：无记录则建行，有则更新。
// 空集持久化为 "[]"，保证清除语义可 roundtrip。
func (s agentCapabilityStore) SaveCapabilities(agentID string, caps []string) error {
	if s.db == nil {
		return nil
	}
	if caps == nil {
		caps = []string{}
	}
	// []string 的 MarshalJSON 恒成功
	payload, _ := json.Marshal(caps)

	var rec models.Agent
	s.db.Select("id").Where("agent_id = ?", agentID).Limit(1).Find(&rec)
	if rec.ID == 0 {
		return s.db.Create(&models.Agent{
			AgentID:      agentID,
			Status:       "offline",
			Capabilities: string(payload),
		}).Error
	}
	return s.db.Model(&models.Agent{}).Where("id = ?", rec.ID).Updates(map[string]any{
		"capabilities": string(payload),
	}).Error
}
