package vault

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"gorm.io/gorm"
)

// LocalProvider 默认实现（models/vault.go 头注释的加密结构）：用户主口令
// 经 PBKDF2 派生 KEK 包裹 per-user DEK，凭据以 DEK 加密落 server 主库，
// DEK 只在解锁期间驻留本进程内存（空闲 TTL 自动锁回）。自托管零外部依赖。
// 加密原语（PBKDF2 / AES-256-GCM）在 internal/security——本类型是它们唯一
// 的接线处（会话口径与密钥策略不散落到调用方）。
type LocalProvider struct {
	db  *gorm.DB
	ttl time.Duration

	mu       sync.Mutex
	sessions map[uint]*localSession
}

// localSession 一枚解锁态（DEK + 空闲到期时刻）。
type localSession struct {
	dek       []byte
	expiresAt time.Time
}

// LocalName 本地加密实现注册名（WINGMAN_VAULT_PROVIDER 默认值）。
const LocalName = "local"

// MinMasterLen 主口令最小长度（弱口令防御第一层；KDF 轮数扛离线爆破）。
const MinMasterLen = 8

// defaultSessionTTL 解锁驻留时长（空闲计时：每次取用/管理刷新）。到期自动
// 锁回 =「锁定保险箱」的自动化身；手动 lock 随时可用。经配置节
// SESSION_TTL 覆盖。
const defaultSessionTTL = 30 * time.Minute

func init() { Register(LocalName, NewLocal) }

// NewLocal 构造本地实现。Config 节：SESSION_TTL 覆盖空闲锁回时长
// （time.ParseDuration，须 > 0；非法值报错——配置错误启动即炸，不静默用默认）。
func NewLocal(opts Options) (Provider, error) {
	if opts.DB == nil {
		return nil, fmt.Errorf("local vault requires db")
	}
	ttl := defaultSessionTTL
	if raw := strings.TrimSpace(opts.Config["SESSION_TTL"]); raw != "" {
		d, err := time.ParseDuration(raw)
		if err != nil || d <= 0 {
			return nil, fmt.Errorf("local vault: bad SESSION_TTL %q", raw)
		}
		ttl = d
	}
	return &LocalProvider{db: opts.DB, ttl: ttl, sessions: map[uint]*localSession{}}, nil
}

// Name 实现 Provider。
func (p *LocalProvider) Name() string { return LocalName }

// ---------- 解锁会话（实例内，进程重启天然全锁） ----------

// keepAlive 取用并刷新空闲 TTL；过期懒清理。未解锁返回 nil。
func (p *LocalProvider) keepAlive(userID uint) []byte {
	p.mu.Lock()
	defer p.mu.Unlock()
	s, ok := p.sessions[userID]
	if !ok {
		return nil
	}
	if time.Now().After(s.expiresAt) {
		delete(p.sessions, userID) // 过期懒清理
		return nil
	}
	s.expiresAt = time.Now().Add(p.ttl)
	return s.dek
}

// storeSession 登记解锁态。
func (p *LocalProvider) storeSession(userID uint, dek []byte) {
	p.mu.Lock()
	defer p.mu.Unlock()
	p.sessions[userID] = &localSession{dek: dek, expiresAt: time.Now().Add(p.ttl)}
}

// ---------- 状态 / 主口令生命周期 ----------

// Status 实现 Provider。
func (p *LocalProvider) Status(userID uint) (MasterStatus, error) {
	var master models.VaultMaster
	configed := p.db.Where("user_id = ?", userID).First(&master).Error == nil
	st := MasterStatus{Configured: configed, AutoLockAfterS: int(p.ttl.Seconds())}
	if p.keepAlive(userID) != nil {
		st.Unlocked = true
		p.mu.Lock()
		if s, ok := p.sessions[userID]; ok {
			st.UnlockedUntil = s.expiresAt.UTC().Format(time.RFC3339)
		}
		p.mu.Unlock()
	}
	return st, nil
}

// Setup 实现 Provider。
func (p *LocalProvider) Setup(userID uint, masterPassword string) (time.Time, error) {
	if len(masterPassword) < MinMasterLen {
		return time.Time{}, ErrWeakMaster
	}
	var exist models.VaultMaster
	if p.db.Where("user_id = ?", userID).First(&exist).Error == nil {
		return time.Time{}, ErrAlreadyConfigured
	}
	salt, err := security.VaultGenerateSalt()
	if err != nil {
		return time.Time{}, fmt.Errorf("generate salt: %w", err)
	}
	kek, err := security.VaultDeriveKey(masterPassword, salt, security.VaultKDFIterations)
	if err != nil {
		return time.Time{}, fmt.Errorf("derive kek: %w", err)
	}
	dek, err := security.VaultGenerateDEK()
	if err != nil {
		return time.Time{}, fmt.Errorf("generate dek: %w", err)
	}
	wrapped, err := security.VaultWrapKey(kek, dek)
	if err != nil {
		return time.Time{}, fmt.Errorf("wrap dek: %w", err)
	}
	if err := p.db.Create(&models.VaultMaster{
		UserID: userID, KDFSalt: salt,
		KDFIterations: security.VaultKDFIterations, WrappedKey: wrapped,
	}).Error; err != nil {
		return time.Time{}, fmt.Errorf("save master: %w", err)
	}
	// 设置即解锁：用户刚证明知道主口令，不必立刻再输一遍
	p.storeSession(userID, dek)
	return time.Now().Add(p.ttl), nil
}

// Unlock 实现 Provider。
func (p *LocalProvider) Unlock(userID uint, masterPassword string) (time.Time, error) {
	var master models.VaultMaster
	if p.db.Where("user_id = ?", userID).First(&master).Error != nil {
		return time.Time{}, ErrNotConfigured // 未配置前不存在「解锁」，先 setup
	}
	kek, err := security.VaultDeriveKey(masterPassword, master.KDFSalt, master.KDFIterations)
	if err != nil {
		return time.Time{}, fmt.Errorf("derive kek: %w", err)
	}
	dek, err := security.VaultUnwrapKey(kek, master.WrappedKey)
	if err != nil {
		// GCM 认证失败 = 口令错（或密文被篡改，AEAD 下不可区分）
		return time.Time{}, ErrWrongMaster
	}
	p.storeSession(userID, dek)
	return time.Now().Add(p.ttl), nil
}

// ChangePassword 实现 Provider。换口令只重包裹 DEK（DEK 不变 → 已存密文
// 全部继续有效），盐一并换新。
func (p *LocalProvider) ChangePassword(userID uint, currentPassword, newPassword string) error {
	if len(newPassword) < MinMasterLen {
		return ErrWeakMaster
	}
	var master models.VaultMaster
	if p.db.Where("user_id = ?", userID).First(&master).Error != nil {
		return ErrNotConfigured
	}
	kek, err := security.VaultDeriveKey(currentPassword, master.KDFSalt, master.KDFIterations)
	if err != nil {
		return fmt.Errorf("derive kek: %w", err)
	}
	dek, err := security.VaultUnwrapKey(kek, master.WrappedKey)
	if err != nil {
		return ErrWrongCurrent
	}
	salt, err := security.VaultGenerateSalt()
	if err != nil {
		return fmt.Errorf("generate salt: %w", err)
	}
	newKek, err := security.VaultDeriveKey(newPassword, salt, security.VaultKDFIterations)
	if err != nil {
		return fmt.Errorf("derive kek: %w", err)
	}
	wrapped, err := security.VaultWrapKey(newKek, dek)
	if err != nil {
		return fmt.Errorf("wrap dek: %w", err)
	}
	master.KDFSalt = salt
	master.KDFIterations = security.VaultKDFIterations
	master.WrappedKey = wrapped
	if err := p.db.Save(&master).Error; err != nil {
		return fmt.Errorf("save master: %w", err)
	}
	p.storeSession(userID, dek) // 换口令即续解锁（刚证明过新口令）
	return nil
}

// Lock 实现 Provider（幂等）。
func (p *LocalProvider) Lock(userID uint) bool {
	p.mu.Lock()
	defer p.mu.Unlock()
	_, was := p.sessions[userID]
	delete(p.sessions, userID)
	return was
}

// ---------- 凭据条目 ----------

// entryMetaOf 落库行 → 元数据视图。
func entryMetaOf(cr models.RemoteCredential) EntryMeta {
	return EntryMeta{
		ID: cr.ID, AgentID: cr.AgentID, Protocol: cr.Protocol, Port: cr.Port,
		Label: cr.Label, Username: cr.Username, Domain: cr.Domain,
		HasPassword: cr.PasswordEnc != "",
		HasSecret:   cr.PasswordEnc != "" || cr.PrivateKeyEnc != "",
		UpdatedAt:   cr.UpdatedAt,
	}
}

// List 实现 Provider。
func (p *LocalProvider) List(userID uint) ([]EntryMeta, error) {
	var creds []models.RemoteCredential
	if err := p.db.Where("user_id = ?", userID).Order("updated_at DESC").Find(&creds).Error; err != nil {
		return nil, fmt.Errorf("list credentials: %w", err)
	}
	out := make([]EntryMeta, 0, len(creds))
	for _, cr := range creds {
		out = append(out, entryMetaOf(cr))
	}
	return out, nil
}

// Upsert 实现 Provider。秘密字段留空 = 保留旧值（改标签不重输密码）。
func (p *LocalProvider) Upsert(userID uint, entry Entry) (EntryMeta, bool, error) {
	dek := p.keepAlive(userID)
	if dek == nil {
		return EntryMeta{}, false, ErrLocked
	}
	var cred models.RemoteCredential
	err := p.db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
		userID, entry.AgentID, entry.Protocol, entry.Port).First(&cred).Error
	exists := err == nil
	if !exists && entry.Password == "" && entry.PrivateKey == "" {
		return EntryMeta{}, false, ErrNeedSecret
	}

	cred.UserID = userID
	cred.AgentID = entry.AgentID
	cred.Protocol = entry.Protocol
	cred.Port = entry.Port
	cred.Label = entry.Label
	cred.Username = entry.Username
	cred.Domain = entry.Domain
	if entry.Password != "" {
		enc, err := security.VaultEncryptString(dek, entry.Password)
		if err != nil {
			return EntryMeta{}, false, fmt.Errorf("encrypt password: %w", err)
		}
		cred.PasswordEnc = enc
	}
	if entry.PrivateKey != "" {
		enc, err := security.VaultEncryptString(dek, entry.PrivateKey)
		if err != nil {
			return EntryMeta{}, false, fmt.Errorf("encrypt private key: %w", err)
		}
		cred.PrivateKeyEnc = enc
	}
	if err := p.db.Save(&cred).Error; err != nil {
		return EntryMeta{}, false, fmt.Errorf("save credential: %w", err)
	}
	return entryMetaOf(cred), !exists, nil
}

// Delete 实现 Provider（归属校验：非本人条目按不存在处理）。
func (p *LocalProvider) Delete(userID uint, entryID uint) error {
	if p.keepAlive(userID) == nil {
		return ErrLocked
	}
	res := p.db.Where("user_id = ? AND id = ?", userID, entryID).Delete(&models.RemoteCredential{})
	if res.Error != nil {
		return fmt.Errorf("delete credential: %w", res.Error)
	}
	if res.RowsAffected == 0 {
		return ErrNotFound
	}
	return nil
}

// exportEntry 导出束里的条目形态（与 wingman-vault-v1 格式对齐）。
type exportEntry struct {
	AgentID       string `json:"agentId"`
	Protocol      string `json:"protocol"`
	Port          int    `json:"port"`
	Label         string `json:"label"`
	Username      string `json:"username"`
	Domain        string `json:"domain,omitempty"`
	PasswordEnc   string `json:"passwordEnc,omitempty"`
	PrivateKeyEnc string `json:"privateKeyEnc,omitempty"`
}

// Export 实现 Provider。导出的是主口令加密的密文束（KDF 参数 + 包裹的
// DEK + 条目密文），可用主口令在未来恢复——明文永不导出（隐私默认）。
func (p *LocalProvider) Export(userID uint) (map[string]any, error) {
	if p.keepAlive(userID) == nil {
		return nil, ErrLocked
	}
	var master models.VaultMaster
	if p.db.Where("user_id = ?", userID).First(&master).Error != nil {
		return nil, ErrNotConfigured
	}
	var creds []models.RemoteCredential
	if err := p.db.Where("user_id = ?", userID).Find(&creds).Error; err != nil {
		return nil, fmt.Errorf("load credentials: %w", err)
	}
	entries := make([]exportEntry, 0, len(creds))
	for _, cr := range creds {
		entries = append(entries, exportEntry{
			AgentID: cr.AgentID, Protocol: cr.Protocol, Port: cr.Port, Label: cr.Label,
			Username: cr.Username, Domain: cr.Domain,
			PasswordEnc: cr.PasswordEnc, PrivateKeyEnc: cr.PrivateKeyEnc,
		})
	}
	return map[string]any{
		"format": "wingman-vault-v1",
		"kdf": map[string]any{
			"salt":        master.KDFSalt,
			"iterations":  master.KDFIterations,
			"wrapped_key": master.WrappedKey,
		},
		"credentials": entries,
		"exportedAt":  time.Now().UTC().Format(time.RFC3339),
	}, nil
}

// ---------- 连接快捷调用（guacamole.go 票据签发注入） ----------

// DefaultProtocolPort 协议默认端口。本地库存入前归一到默认端口（取用回退
// 也按它找条目），一张表两处共用；覆盖面与 guacd 承接的协议一致。
func DefaultProtocolPort(protocol string) int {
	switch protocol {
	case "rdp":
		return 3389
	case "vnc":
		return 5900
	case "ssh":
		return 22
	}
	return 0
}

// Lookup 实现 Provider。
func (p *LocalProvider) Lookup(userID uint, agentID, protocol string, port int) (string, string, string, error) {
	dek := p.keepAlive(userID)
	if dek == nil {
		return "", "", "", ErrLocked
	}
	if port <= 0 {
		port = DefaultProtocolPort(protocol)
	}
	// 先精确端口；非默认端口查不到再回退默认端口条目（保存侧入库前已归一，
	// 两形态通常同一条——回退兜「存时默认端口、连时显式同值端口」的形态差）
	var cred models.RemoteCredential
	if p.db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
		userID, agentID, protocol, port).First(&cred).Error != nil {
		if port == DefaultProtocolPort(protocol) ||
			p.db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
				userID, agentID, protocol, DefaultProtocolPort(protocol)).First(&cred).Error != nil {
			return "", "", "", ErrNotFound
		}
	}
	password := ""
	if cred.PasswordEnc != "" {
		pw, err := security.VaultDecryptString(dek, cred.PasswordEnc)
		if err != nil {
			return "", "", "", ErrDecrypt
		}
		password = pw
	}
	return cred.Username, password, cred.Domain, nil
}
