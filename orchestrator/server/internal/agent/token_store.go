package agent

import (
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// TokenStore per-agent 注册 token 的 DB 存取（A3-P2 安全演进，
// docs/agent-token-auth-design.md §6.2）。明文只在 Create 返回一次，
// 库内仅存 sha256 哈希；校验走 constant-time 比对（与 P1 env 白名单同口径）。
type TokenStore struct {
	db *gorm.DB
}

// NewTokenStore creates a TokenStore backed by the given gorm DB.
func NewTokenStore(db *gorm.DB) *TokenStore {
	return &TokenStore{db: db}
}

// TokenPrefix 明文 token 前缀，沿用 P1 的 wt_ 形态。
const TokenPrefix = "wt_"

// Create 签发新 token：明文只在返回值中出现一次（调用方负责交给创建者），
// 库内落 sha256 哈希。agentID 非空时 token 与该 agent 绑定。
func (s *TokenStore) Create(label, agentID, actor string) (string, *models.AgentToken, error) {
	raw := make([]byte, 32)
	if _, err := rand.Read(raw); err != nil {
		return "", nil, fmt.Errorf("generate token: %w", err)
	}
	plain := TokenPrefix + base64.RawURLEncoding.EncodeToString(raw)
	sum := sha256.Sum256([]byte(plain))

	rec := &models.AgentToken{
		Label:     label,
		TokenHash: hex.EncodeToString(sum[:]),
		Prefix:    plain[:len(TokenPrefix)+6],
		AgentID:   agentID,
		CreatedBy: actor,
	}
	if err := s.db.Create(rec).Error; err != nil {
		return "", nil, fmt.Errorf("save token: %w", err)
	}
	return plain, rec, nil
}

// Verify 校验明文 token：未吊销记录中 constant-time 哈希比对，命中后再查
// agentId 绑定（绑定非空且不匹配 → 拒绝）。命中即刷新 LastSeenAt。
// 库内无任何有效 token 时返回 false——是否启用 DB 源由调用方（listener）判断。
func (s *TokenStore) Verify(token, agentID string) (*models.AgentToken, bool) {
	if token == "" {
		return nil, false
	}
	sum := sha256.Sum256([]byte(token))
	want := hex.EncodeToString(sum[:])

	var rec models.AgentToken
	err := s.db.Where("token_hash = ? AND revoked_at IS NULL", want).
		First(&rec).Error
	if err != nil {
		return nil, false
	}
	if rec.AgentID != "" && rec.AgentID != agentID {
		return nil, false
	}

	now := time.Now()
	_ = s.db.Model(&rec).Update("last_seen_at", &now).Error
	return &rec, true
}

// VerifyHMAC challenge-response 校验（§6.1）：agent 以 sha256hex(token) 为
// HMAC-SHA256 密钥对 nonce 求签，密钥与库内 TokenHash 列同值——服务端无需
// 持有明文即可验签。逐条有效记录 constant-time 比对，命中后查 agentId 绑定
// 并刷新 LastSeenAt（与 Verify 同收尾口径）。
func (s *TokenStore) VerifyHMAC(macHex, nonceHex, agentID string) (*models.AgentToken, bool) {
	if macHex == "" || nonceHex == "" {
		return nil, false
	}
	var recs []models.AgentToken
	if err := s.db.Where("revoked_at IS NULL").Find(&recs).Error; err != nil {
		return nil, false
	}
	for i := range recs {
		want := tokenMAC(recs[i].TokenHash, nonceHex)
		if subtle.ConstantTimeCompare([]byte(macHex), []byte(want)) != 1 {
			continue
		}
		if recs[i].AgentID != "" && recs[i].AgentID != agentID {
			return nil, false
		}
		now := time.Now()
		_ = s.db.Model(&recs[i]).Update("last_seen_at", &now).Error
		return &recs[i], true
	}
	return nil, false
}

// Revoke 吊销 token（幂等：已吊销返回 false 不报错）。
func (s *TokenStore) Revoke(id uint) (bool, error) {
	res := s.db.Model(&models.AgentToken{}).
		Where("id = ? AND revoked_at IS NULL", id).
		Update("revoked_at", time.Now())
	return res.RowsAffected > 0, res.Error
}

// List 返回全部 token（不含哈希；AgentToken.TokenHash json:"-" 序列化即隐去）。
func (s *TokenStore) List() ([]models.AgentToken, error) {
	var out []models.AgentToken
	err := s.db.Order("created_at desc").Find(&out).Error
	return out, err
}

// AnyExists 是否存在任何 token 记录（含已吊销）；listener 以此判断 DB 源
// 是否启用。注意口径是「签发记录」而非「有效 token」——吊销最后一个 token
// 后鉴权必须保持开启（fail-closed），否则会出现「吊销即开门」的安全退化。
func (s *TokenStore) AnyExists() (bool, error) {
	var n int64
	err := s.db.Model(&models.AgentToken{}).Count(&n).Error
	return n > 0, err
}
