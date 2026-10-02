package models

import (
	"time"

	"gorm.io/gorm"
)

// 密钥保险箱（cockpit 密码箱模式的 wingman 落地，2026-10-03）：
// 远程桌面凭据按用户加密落库，连接时自动取用——解决「每次手输」的痛点。
//
// 加密结构（主口令派生密钥，不落明文）：
//
//	主口令 ──PBKDF2-HMAC-SHA256(salt, 600k)──▶ KEK
//	随机 32B DEK ──AES-256-GCM(KEK)──▶ WrappedKey（落库）
//	凭据 Password/PrivateKey ──AES-256-GCM(DEK)──▶ 密文（落库）
//
// DEK 只在解锁期间驻留服务端内存（空闲超时自动锁回），主口令与 DEK
// 永不落库；忘掉主口令 = 保险箱不可恢复（这是设计属性而非缺陷）。
// 与 cockpit 的差异：cockpit 落库密文用 server 级 env 密钥，本实现按
// 用户主口令派生——服务端存储被拖走也解不开凭据。

// VaultMaster 用户保险箱主记录（每用户至多一行）：主口令派生参数 + 包裹
// 的 DEK。不存任何可校验主口令的明文摘要——解锁 = 解开 WrappedKey，
// GCM 认证标签失败即口令错误（AEAD 天然防错口令/防篡改）。
type VaultMaster struct {
	gorm.Model
	// UserID 所属用户（一箱一用户，唯一）
	UserID uint `gorm:"uniqueIndex;not null" json:"userId"`
	// KDFSalt PBKDF2 盐（hex）
	KDFSalt string `gorm:"size:64;not null" json:"-"`
	// KDFIterations PBKDF2 轮数（落库以便未来升级轮数时旧记录自洽）
	KDFIterations int `gorm:"not null" json:"-"`
	// WrappedKey KEK 包裹的 DEK（hex：nonce‖ciphertext，AES-256-GCM）
	WrappedKey string `gorm:"size:256;not null" json:"-"`
}

func (VaultMaster) TableName() string { return "vault_masters" }

// RemoteCredential 一条已存凭据。同一 (用户, agent, 协议, 端口) 至多一组
// （连接时按目标元组自动取用——这正是「不用每次输入」的实现面）；标签
// 供管理界面辨认。密文字段 json 永不序列化，列表 API 只出元数据。
type RemoteCredential struct {
	gorm.Model
	// 四列联合唯一：同用户同目标只有一组，重复保存视为更新
	UserID   uint   `gorm:"uniqueIndex:idx_remote_cred_target" json:"-"`
	AgentID  string `gorm:"size:128;uniqueIndex:idx_remote_cred_target" json:"agentId"`
	Protocol string `gorm:"size:16;uniqueIndex:idx_remote_cred_target" json:"protocol"` // rdp/vnc/ssh
	// Port agent 侧端口（入库前已归一到协议默认端口，0 不会出现）
	Port int `gorm:"uniqueIndex:idx_remote_cred_target" json:"port"`
	// Label 标签（管理界面显示名，如「工作机 RDP」）
	Label string `gorm:"size:128" json:"label"`
	// Username / Domain 明文元数据（非机密；密码才是机密）
	Username string `gorm:"size:255" json:"username"`
	Domain   string `gorm:"size:255" json:"domain,omitempty"`
	// PasswordEnc / PrivateKeyEnc AES-GCM(DEK) 密文（hex：nonce‖ciphertext）
	PasswordEnc   string `gorm:"type:text" json:"-"`
	PrivateKeyEnc string `gorm:"type:text" json:"-"`
	// HasPassword / HasSecret 列表视图位（有密文而不暴露内容）
	HasPassword bool `json:"hasPassword"`
	HasSecret   bool `json:"hasSecret"` // password 或 private key 任一存在

	CreatedAt time.Time `json:"createdAt"`
	UpdatedAt time.Time `json:"updatedAt"`
}

func (RemoteCredential) TableName() string { return "remote_credentials" }
