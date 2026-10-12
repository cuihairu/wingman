// Package vault 密钥保险箱 provider 抽象（根 todo.md 2026-10-11：VaultProvider
// 统一接口 + 可插拔实现）。解锁/读/写/列/删除/导出/连接取用统一在一个接口
// 面上；加密落库（KDF、主口令派生、DEK 包裹）与解锁会话口径（驻留 TTL、
// 空闲自动锁回）收敛在各 provider 实现内部，不散落到 HTTP 适配层。
//
// 分工边界（适配层在 handlers/vault.go）：
//   - provider 实现负责：主口令策略（最小长度等）、加密与落库、解锁会话、
//     导出格式、目标凭据取用；
//   - HTTP 适配层负责：请求形状校验（非空、协议白名单、端口归一、export
//     显式确认）、审计（WriteAuditLog 统一记，provider 不重复记）、sentinel
//     错误 → HTTP 状态码映射。
//
// 消费方（含 guacamole 票据注入）一律经 vault.Active() 走当前激活实现，
// 只改配置（WINGMAN_VAULT_PROVIDER）即换实现，业务代码零改动。
//
// 红线（根 todo.md）：解锁口令与明文凭据不落日志、不出现在错误消息——
// 本包 sentinel 错误一律静态文案；实现不得把口令/明文带进任何 error。
package vault

import (
	"errors"
	"time"
)

// Provider 密钥保险箱统一接口。实现必须满足：
//   - Setup 后的解锁会话语义：解锁成功即驻留（空闲 TTL 到期自动锁回，
//     每次取用/管理刷新计时）；Lock 幂等；
//   - 密钥策略：主口令过短返回 ErrWeakMaster；未配置时 Unlock 返回
//     ErrNotConfigured、重复 Setup 返回 ErrAlreadyConfigured；
//   - Upsert 以 (agentID, protocol, port) 归一，秘密字段留空 = 保留旧值；
//   - 红线：口令/明文不出现在 error、日志与审计里。
type Provider interface {
	// Name 注册名（WINGMAN_VAULT_PROVIDER 的取值）。
	Name() string

	// Status 配置/解锁状态（前端弹窗探测用，不泄密）。
	Status(userID uint) (MasterStatus, error)

	// Setup 首次设置主口令；返回解锁到期时刻（设置即解锁）。
	Setup(userID uint, masterPassword string) (unlockedUntil time.Time, err error)

	// Unlock 校验主口令并进入解锁态；返回解锁到期时刻。
	Unlock(userID uint, masterPassword string) (unlockedUntil time.Time, err error)

	// ChangePassword 换主口令（须当前口令）；已存密文对新口令继续有效。
	ChangePassword(userID uint, currentPassword, newPassword string) error

	// Lock 立即锁回（丢弃驻留会话）。返回是否原本处于解锁态。
	Lock(userID uint) bool

	// List 凭据元数据列表（无密文无明文）。
	List(userID uint) ([]EntryMeta, error)

	// Upsert 保存/更新一条凭据；返回落库元数据与「是否新建」。
	Upsert(userID uint, entry Entry) (meta EntryMeta, updated bool, err error)

	// Delete 删除一条凭据（归属校验：非本人条目按不存在处理）。
	Delete(userID uint, entryID uint) error

	// Export 导出密文束（实现自定义格式；实现须保证明文永不导出）。
	Export(userID uint) (map[string]any, error)

	// Lookup 连接快捷调用：取目标对应凭据并解密（须已解锁）。明文只在
	// 此刻交给票据签发方，实现不得另落日志。
	Lookup(userID uint, agentID, protocol string, port int) (username, password, domain string, err error)
}

// MasterStatus 状态视图（HandleStatus 与前端探测的契约）。
type MasterStatus struct {
	Configured bool `json:"configured"`
	Unlocked   bool `json:"unlocked"`
	// UnlockedUntil 解锁到期时刻（RFC3339）；锁定态为空。
	UnlockedUntil string `json:"unlockedUntil,omitempty"`
	// AutoLockAfterS 空闲自动锁回时长（秒），前端展示倒计时基准。
	AutoLockAfterS int `json:"autoLockAfterS"`
}

// EntryMeta 凭据元数据（列表/保存返回视图；无密文无明文）。
type EntryMeta struct {
	ID          uint      `json:"id"`
	AgentID     string    `json:"agentId"`
	Protocol    string    `json:"protocol"`
	Port        int       `json:"port"`
	Label       string    `json:"label"`
	Username    string    `json:"username"`
	Domain      string    `json:"domain,omitempty"`
	HasPassword bool      `json:"hasPassword"`
	HasSecret   bool      `json:"hasSecret"`
	UpdatedAt   time.Time `json:"updatedAt"`
}

// Entry 保存面。Password/PrivateKey 留空 = 保留旧值（「改标签」路径）。
type Entry struct {
	AgentID    string
	Protocol   string
	Port       int
	Label      string
	Username   string
	Domain     string
	Password   string
	PrivateKey string
}

// 语义错误（HTTP 适配层据此映射状态码；文案即 API error 字段，保持与
// 既有前端识别兼容——如 "vault locked" / "not configured" 子串匹配）。
var (
	// ErrAlreadyConfigured 重复 Setup（409）。
	ErrAlreadyConfigured = errors.New("vault already configured")
	// ErrNotConfigured 未设置主口令（409）。
	ErrNotConfigured = errors.New("vault not configured")
	// ErrWrongMaster 解锁口令错误（401；AEAD 下与密文被篡改不可区分，实现
	// 统一折叠为这一错误，不给攻击者分辨通道）。
	ErrWrongMaster = errors.New("master password incorrect")
	// ErrWrongCurrent 换口令时当前口令错误（401）。
	ErrWrongCurrent = errors.New("current master password incorrect")
	// ErrWeakMaster 主口令不满足实现强度策略（400）。
	ErrWeakMaster = errors.New("master password must be at least 8 characters")
	// ErrNeedSecret 新建条目未给任何秘密字段（400）。
	ErrNeedSecret = errors.New("password or privateKey required for new entry")
	// ErrLocked 保险箱处于锁定态（423）。
	ErrLocked = errors.New("vault locked")
	// ErrNotFound 目标凭据不存在（404）。
	ErrNotFound = errors.New("credential not found")
	// ErrDecrypt 取用时密文解不开（500；理论上只在库被手改/换 DEK 时发生）。
	ErrDecrypt = errors.New("failed to decrypt saved credential")
)
