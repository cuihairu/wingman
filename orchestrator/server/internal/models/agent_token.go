package models

import (
	"time"

	"gorm.io/gorm"
)

// AgentToken per-agent 注册 token（A3-P2 安全演进：token 入 DB，Dashboard
// 创建/吊销，register 校验 + 审计落库；docs/agent-token-auth-design.md §6.2）。
//
// 与 P1 环境变量白名单（WINGMAN_AGENT_TOKENS）双源并存：任一命中即放行，
// 迁移期 env 兜底、清空 env 后即纯 DB 管理面模式（演进只加不改）。
// 明文 token 只在创建时返回一次，库内仅存 sha256 哈希。
type AgentToken struct {
	gorm.Model
	// Label 管理面展示名（如 "pixel-8 真机"），仅人可读，不参与校验。
	Label string `gorm:"size:128;not null" json:"label"`
	// TokenHash sha256 hex（64 字符），uniqueIndex 防重复签发。
	TokenHash string `gorm:"size:64;uniqueIndex;not null" json:"-"`
	// Prefix 明文前缀（wt_ + 前 6 字符），列表展示用，不泄露完整 token。
	Prefix string `gorm:"size:12;not null" json:"prefix"`
	// AgentID 绑定 agentId；空串 = 不限 agent（共享 token）。
	AgentID string `gorm:"size:128;index;default:''" json:"agentId"`
	// CreatedBy 创建者（Dashboard 用户名），审计用。
	CreatedBy string `gorm:"size:128;not null;default:''" json:"createdBy"`
	// RevokedAt 吊销时间；nil = 有效。
	RevokedAt *time.Time `json:"revokedAt,omitempty"`
	// LastSeenAt 最近一次校验通过时间（register 成功时更新）。
	LastSeenAt *time.Time `json:"lastSeenAt,omitempty"`
}
