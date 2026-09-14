package handlers

import (
	"encoding/json"
	"log"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// agentTagStore agent.Tags 列（JSON 文本）的读写实现。
// Registry 经 agent.TagStore 接口使用，避免 Registry 直接依赖 gorm。
type agentTagStore struct {
	db *gorm.DB
}

// NewAgentTagStore 构造基于 models.Agent 表的标签持久化后端。
func NewAgentTagStore(db *gorm.DB) agent.TagStore {
	return agentTagStore{db: db}
}

// LoadTags 读取持久化标签；无记录或解析失败返回 (nil, false)。
func (s agentTagStore) LoadTags(agentID string) ([]string, bool) {
	if s.db == nil {
		return nil, false
	}
	var rec models.Agent
	// Find 不产生 ErrRecordNotFound，以主键是否为零判断命中
	s.db.Select("id", "tags").Where("agent_id = ?", agentID).Limit(1).Find(&rec)
	if rec.ID == 0 || rec.Tags == "" {
		return nil, false
	}
	var tags []string
	if err := json.Unmarshal([]byte(rec.Tags), &tags); err != nil {
		log.Printf("[TagStore] Failed to parse tags for %s: %v", agentID, err)
		return nil, false
	}
	return tags, true
}

// SaveTags 持久化标签：无记录则建行（顺带补 hostname/ip 元数据），有则更新。
// 空标签持久化为 "[]"，保证清除语义可 roundtrip。
func (s agentTagStore) SaveTags(agentID, hostname, ip string, tags []string) error {
	if s.db == nil {
		return nil
	}
	if tags == nil {
		tags = []string{}
	}
	payload, err := json.Marshal(tags)
	if err != nil {
		return err
	}

	var rec models.Agent
	s.db.Select("id").Where("agent_id = ?", agentID).Limit(1).Find(&rec)
	if rec.ID == 0 {
		return s.db.Create(&models.Agent{
			AgentID:  agentID,
			Hostname: hostname,
			IP:       ip,
			Status:   "offline",
			Tags:     string(payload),
		}).Error
	}
	return s.db.Model(&models.Agent{}).Where("id = ?", rec.ID).Updates(map[string]any{
		"tags":     string(payload),
		"hostname": hostname,
		"ip":       ip,
	}).Error
}
