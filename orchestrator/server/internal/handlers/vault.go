package handlers

// 密钥保险箱 API（models/vault.go 头注释的加密结构、cockpit 密码箱的
// wingman 落地）。与 cockpit 的差异：落库密钥不是 server 级 env 密钥，
// 而是用户主口令经 PBKDF2 派生的 KEK 所包裹的 per-user DEK——DEK 只在
// 解锁期间驻留内存（空闲 30 分钟自动锁回），忘掉主口令即不可恢复。
//
// 端点语义（全部挂 desktop 权限组，登录态是底线）：
//   GET  status                 配置/解锁状态（弹窗探测用，不泄密）
//   POST setup                  首次设置主口令（已配置则 409）
//   POST change-password        换主口令（需当前口令，DEK 不变、密文不动）
//   POST unlock / lock          解锁 / 锁回（lock 幂等）
//   GET  credentials            元数据列表（无密文，登录态即可——弹窗要用
//                                它探测「已存凭据」，这正是仅凭登录态的
//                                连接便利所在，与 cockpit 同款取舍）
//   PUT  credentials            保存/更新（须解锁；秘密字段留空 = 保留旧值，
//                                即「改标签」路径）
//   DELETE credentials/:id      删除（须解锁）
//   GET  export?confirm=true    导出密文束（须解锁 + 显式确认；内容全为
//                                主口令加密的密文，永不导出明文——隐私默认）
//
// 连接快捷调用（useSaved）在 guacamole.go 的票据签发里注入，明文只在
// 那一刻解密进 connect 参数，不回传浏览器、不进日志与审计。

import (
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// vaultUnlockTTL 解锁驻留时长（空闲计时：每次取用/管理刷新）。到期自动
// 锁回 = 「锁定保险箱」的自动化身；手动 lock 随时可用。
const vaultUnlockTTL = 30 * time.Minute

// vaultMinMasterLen 主口令最小长度（弱口令防御第一层；KDF 轮数扛离线爆破）
const vaultMinMasterLen = 8

// vaultUnlock 一枚解锁态（DEK + 空闲到期时刻）
type vaultUnlock struct {
	dek       []byte
	expiresAt time.Time
}

// 包级解锁表（同 listener.go guacRelayReg 惯例：不依赖 handler 字段，
// 测试可用零值依赖走真实路径）。进程重启天然全锁（内存即锁）。
var (
	vaultUnlocksMu sync.Mutex
	vaultUnlocks   = map[uint]*vaultUnlock{}
)

// vaultKeepAlive 取用并刷新空闲 TTL。未解锁返回 nil。
func vaultKeepAlive(userID uint) []byte {
	vaultUnlocksMu.Lock()
	defer vaultUnlocksMu.Unlock()
	u, ok := vaultUnlocks[userID]
	if !ok {
		return nil
	}
	if time.Now().After(u.expiresAt) {
		delete(vaultUnlocks, userID) // 过期懒清理
		return nil
	}
	u.expiresAt = time.Now().Add(vaultUnlockTTL)
	return u.dek
}

// vaultLockUser 锁回（幂等）。返回是否原本处于解锁态。
func vaultLockUser(userID uint) bool {
	vaultUnlocksMu.Lock()
	defer vaultUnlocksMu.Unlock()
	_, was := vaultUnlocks[userID]
	delete(vaultUnlocks, userID)
	return was
}

// vaultUnlockStore 登记解锁态。
func vaultUnlockStore(userID uint, dek []byte) {
	vaultUnlocksMu.Lock()
	defer vaultUnlocksMu.Unlock()
	vaultUnlocks[userID] = &vaultUnlock{dek: dek, expiresAt: time.Now().Add(vaultUnlockTTL)}
}

// VaultHandler 保险箱端点。
type VaultHandler struct {
	db *gorm.DB
}

func NewVaultHandler(db *gorm.DB) *VaultHandler {
	return &VaultHandler{db: db}
}

// ---------- 状态 / 主口令生命周期 ----------

// vaultStatus 视图（HandleStatus 与前端探测的契约）
type vaultStatus struct {
	Configured     bool   `json:"configured"`
	Unlocked       bool   `json:"unlocked"`
	UnlockedUntil  string `json:"unlockedUntil,omitempty"`
	AutoLockAfterS int    `json:"autoLockAfterS"`
}

// HandleStatus GET /api/remote/vault/status
// @Summary      保险箱状态
// @Description  是否已设置主口令、当前是否解锁（含空闲自动锁定倒计时基准）
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/status [get]
func (h *VaultHandler) HandleStatus(c *gin.Context) {
	userID, _, _ := middleware.GetCurrentUser(c)
	var master models.VaultMaster
	configed := h.db.Where("user_id = ?", userID).First(&master).Error == nil

	until := ""
	dek := vaultKeepAlive(userID)
	if dek != nil {
		vaultUnlocksMu.Lock()
		if u, ok := vaultUnlocks[userID]; ok {
			until = u.expiresAt.UTC().Format(time.RFC3339)
		}
		vaultUnlocksMu.Unlock()
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": vaultStatus{
		Configured:     configed,
		Unlocked:       dek != nil,
		UnlockedUntil:  until,
		AutoLockAfterS: int(vaultUnlockTTL.Seconds()),
	}})
}

// HandleSetup POST /api/remote/vault/setup {masterPassword}
// @Summary      首次设置保险箱主口令
// @Description  生成 KDF 盐与随机 DEK，DEK 以主口令派生 KEK 包裹落库；重复设置返回 409
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object{masterPassword=string}  true  "主口令（≥8 字符）"
// @Success      200   {object}  map[string]interface{}
// @Failure      400   {object}  ErrorResponse
// @Failure      409   {object}  ErrorResponse
// @Router       /remote/vault/setup [post]
func (h *VaultHandler) HandleSetup(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	var req struct {
		MasterPassword string `json:"masterPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || strings.TrimSpace(req.MasterPassword) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "masterPassword required"})
		return
	}
	if len(req.MasterPassword) < vaultMinMasterLen {
		c.JSON(http.StatusBadRequest, gin.H{"success": false,
			"error": "master password must be at least 8 characters"})
		return
	}
	var exist models.VaultMaster
	if h.db.Where("user_id = ?", userID).First(&exist).Error == nil {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "vault already configured"})
		return
	}

	salt, err := security.VaultGenerateSalt()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}
	kek, err := security.VaultDeriveKey(req.MasterPassword, salt, security.VaultKDFIterations)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}
	dek, err := security.VaultGenerateDEK()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}
	wrapped, err := security.VaultWrapKey(kek, dek)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}
	if err := h.db.Create(&models.VaultMaster{
		UserID: userID, KDFSalt: salt,
		KDFIterations: security.VaultKDFIterations, WrappedKey: wrapped,
	}).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}

	// 设置即解锁：用户刚证明知道主口令，不必立刻再输一遍
	vaultUnlockStore(userID, dek)
	WriteAuditLog(h.db, username, "vault.setup", strconv.FormatUint(uint64(userID), 10), nil)
	c.JSON(http.StatusOK, gin.H{"success": true, "data": gin.H{
		"unlockedUntil": time.Now().Add(vaultUnlockTTL).UTC().Format(time.RFC3339),
	}})
}

// HandleChangePassword POST /api/remote/vault/change-password {currentPassword, newPassword}
// 换口令只重包裹 DEK（DEK 不变 → 已存密文全部继续有效），盐一并换新。
// @Summary      更换保险箱主口令
// @Description  校验当前口令后以新口令重包裹 DEK；已存凭据密文不受影响
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object{currentPassword=string,newPassword=string}  true  "当前与新主口令"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Router       /remote/vault/change-password [post]
func (h *VaultHandler) HandleChangePassword(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	var req struct {
		CurrentPassword string `json:"currentPassword"`
		NewPassword     string `json:"newPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || req.CurrentPassword == "" || req.NewPassword == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "currentPassword and newPassword required"})
		return
	}
	if len(req.NewPassword) < vaultMinMasterLen {
		c.JSON(http.StatusBadRequest, gin.H{"success": false,
			"error": "master password must be at least 8 characters"})
		return
	}
	var master models.VaultMaster
	if h.db.Where("user_id = ?", userID).First(&master).Error != nil {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "vault not configured"})
		return
	}
	kek, err := security.VaultDeriveKey(req.CurrentPassword, master.KDFSalt, master.KDFIterations)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	dek, err := security.VaultUnwrapKey(kek, master.WrappedKey)
	if err != nil {
		WriteAuditLog(h.db, username, "vault.change_password", "vault", gin.H{"result": "wrong current password"})
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "current master password incorrect"})
		return
	}
	salt, err := security.VaultGenerateSalt()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	newKek, err := security.VaultDeriveKey(req.NewPassword, salt, security.VaultKDFIterations)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	wrapped, err := security.VaultWrapKey(newKek, dek)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	master.KDFSalt = salt
	master.KDFIterations = security.VaultKDFIterations
	master.WrappedKey = wrapped
	if err := h.db.Save(&master).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	vaultUnlockStore(userID, dek) // 换口令即续解锁（刚证明过新口令）
	WriteAuditLog(h.db, username, "vault.change_password", "vault", nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleUnlock POST /api/remote/vault/unlock {masterPassword}
// @Summary      解锁保险箱
// @Description  校验主口令并解开 DEK 驻留内存（30 分钟空闲自动锁回）；连续失败记审计
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object{masterPassword=string}  true  "主口令"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      409  {object}  ErrorResponse
// @Router       /remote/vault/unlock [post]
func (h *VaultHandler) HandleUnlock(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	var req struct {
		MasterPassword string `json:"masterPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || req.MasterPassword == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "masterPassword required"})
		return
	}
	var master models.VaultMaster
	if h.db.Where("user_id = ?", userID).First(&master).Error != nil {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "vault not configured"})
		return
	}
	// 未配置前不存在「解锁」，先 setup
	kek, err := security.VaultDeriveKey(req.MasterPassword, master.KDFSalt, master.KDFIterations)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to unlock"})
		return
	}
	dek, err := security.VaultUnwrapKey(kek, master.WrappedKey)
	if err != nil {
		WriteAuditLog(h.db, username, "vault.unlock", "vault", gin.H{"result": "failed"})
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "master password incorrect"})
		return
	}
	vaultUnlockStore(userID, dek)
	WriteAuditLog(h.db, username, "vault.unlock", "vault", gin.H{"result": "ok"})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": gin.H{
		"unlockedUntil": time.Now().Add(vaultUnlockTTL).UTC().Format(time.RFC3339),
	}})
}

// HandleLock POST /api/remote/vault/lock
// @Summary      锁定保险箱
// @Description  立即丢弃内存中的 DEK（幂等）；此后取用/管理均要求重新解锁
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/lock [post]
func (h *VaultHandler) HandleLock(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	vaultLockUser(userID)
	WriteAuditLog(h.db, username, "vault.lock", "vault", nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// ---------- 凭据条目 ----------

// vaultEntryMeta 列表视图（元数据，无密文）
type vaultEntryMeta struct {
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

// HandleList GET /api/remote/vault/credentials
// @Summary      保险箱凭据列表（元数据）
// @Description  只含标签/目标/用户名等元数据与「有无密码」标志，密文与明文一律不出
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/credentials [get]
func (h *VaultHandler) HandleList(c *gin.Context) {
	userID, _, _ := middleware.GetCurrentUser(c)
	var creds []models.RemoteCredential
	if err := h.db.Where("user_id = ?", userID).Order("updated_at DESC").Find(&creds).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to list credentials"})
		return
	}
	out := make([]vaultEntryMeta, 0, len(creds))
	for _, cr := range creds {
		out = append(out, vaultEntryMeta{
			ID: cr.ID, AgentID: cr.AgentID, Protocol: cr.Protocol, Port: cr.Port,
			Label: cr.Label, Username: cr.Username, Domain: cr.Domain,
			HasPassword: cr.PasswordEnc != "", HasSecret: cr.PasswordEnc != "" || cr.PrivateKeyEnc != "",
			UpdatedAt: cr.UpdatedAt,
		})
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": out})
}

// HandleUpsert PUT /api/remote/vault/credentials（须解锁）
// 秘密字段留空 + 条目已存在 = 只改标签/用户名等元数据（「改标签」路径）；
// 新条目必须至少给一个秘密字段。
// @Summary      保存/更新一条凭据
// @Description  同 (agent, 协议, 端口) 至多一组，重复保存视为更新；须先解锁保险箱
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "凭据（password/privateKey 留空表示保留旧值）"
// @Success      200  {object}  map[string]interface{}
// @Failure      423  {object}  ErrorResponse
// @Router       /remote/vault/credentials [put]
func (h *VaultHandler) HandleUpsert(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	dek := vaultKeepAlive(userID)
	if dek == nil {
		c.JSON(http.StatusLocked, gin.H{"success": false, "error": "vault locked"})
		return
	}
	var req struct {
		AgentID    string `json:"agentId"`
		Protocol   string `json:"protocol"`
		Port       int    `json:"port"`
		Label      string `json:"label"`
		Username   string `json:"username"`
		Domain     string `json:"domain"`
		Password   string `json:"password"`
		PrivateKey string `json:"privateKey"`
	}
	if err := c.ShouldBindJSON(&req); err != nil ||
		strings.TrimSpace(req.AgentID) == "" || !guacSupportedProtocol(strings.ToLower(strings.TrimSpace(req.Protocol))) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "agentId and protocol (rdp/vnc/ssh) required"})
		return
	}
	req.Protocol = strings.ToLower(strings.TrimSpace(req.Protocol))
	if req.Port <= 0 {
		req.Port = guacProtocolPort(req.Protocol)
	}
	req.AgentID = strings.TrimSpace(req.AgentID)

	var cred models.RemoteCredential
	err := h.db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
		userID, req.AgentID, req.Protocol, req.Port).First(&cred).Error
	exists := err == nil

	if !exists && req.Password == "" && req.PrivateKey == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "password or privateKey required for new entry"})
		return
	}

	cred.UserID = userID
	cred.AgentID = req.AgentID
	cred.Protocol = req.Protocol
	cred.Port = req.Port
	cred.Label = strings.TrimSpace(req.Label)
	cred.Username = req.Username
	cred.Domain = req.Domain
	// 秘密字段留空 = 保留旧值（改标签不重输密码）
	if req.Password != "" {
		enc, err := security.VaultEncryptString(dek, req.Password)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to encrypt credential"})
			return
		}
		cred.PasswordEnc = enc
	}
	if req.PrivateKey != "" {
		enc, err := security.VaultEncryptString(dek, req.PrivateKey)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to encrypt credential"})
			return
		}
		cred.PrivateKeyEnc = enc
	}
	if err := h.db.Save(&cred).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to save credential"})
		return
	}
	WriteAuditLog(h.db, username, "vault.save", req.AgentID, gin.H{
		"protocol": req.Protocol, "port": req.Port, "label": cred.Label, "updated": exists,
	})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": vaultEntryMeta{
		ID: cred.ID, AgentID: cred.AgentID, Protocol: cred.Protocol, Port: cred.Port,
		Label: cred.Label, Username: cred.Username, Domain: cred.Domain,
		HasPassword: cred.PasswordEnc != "", HasSecret: cred.PasswordEnc != "" || cred.PrivateKeyEnc != "",
		UpdatedAt: cred.UpdatedAt,
	}})
}

// HandleDelete DELETE /api/remote/vault/credentials/:id（须解锁）
// @Summary      删除一条凭据
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Failure      423  {object}  ErrorResponse
// @Router       /remote/vault/credentials/{id} [delete]
func (h *VaultHandler) HandleDelete(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	if vaultKeepAlive(userID) == nil {
		c.JSON(http.StatusLocked, gin.H{"success": false, "error": "vault locked"})
		return
	}
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid credential id"})
		return
	}
	res := h.db.Where("user_id = ? AND id = ?", userID, id).Delete(&models.RemoteCredential{})
	if res.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete credential"})
		return
	}
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "credential not found"})
		return
	}
	WriteAuditLog(h.db, username, "vault.delete", c.Param("id"), nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleExport GET /api/remote/vault/export?confirm=true（须解锁 + 显式确认）
// 导出的是主口令加密的密文束（KDF 参数 + 包裹的 DEK + 条目密文），可用
// 主口令在未来恢复——明文永不导出（隐私默认：凭据不出本机，导出须明示）。
// @Summary      导出保险箱（密文束）
// @Description  须解锁且显式 confirm=true；内容为主口令加密的密文，不含任何明文凭据
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Param        confirm  query  bool  true  "显式确认导出"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      423  {object}  ErrorResponse
// @Router       /remote/vault/export [get]
func (h *VaultHandler) HandleExport(c *gin.Context) {
	userID, username, _ := middleware.GetCurrentUser(c)
	if c.Query("confirm") != "true" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false,
			"error": "export requires explicit confirm=true (credentials never leave in plaintext)"})
		return
	}
	dek := vaultKeepAlive(userID)
	if dek == nil {
		c.JSON(http.StatusLocked, gin.H{"success": false, "error": "vault locked"})
		return
	}
	var master models.VaultMaster
	if h.db.Where("user_id = ?", userID).First(&master).Error != nil {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "vault not configured"})
		return
	}
	var creds []models.RemoteCredential
	if err := h.db.Where("user_id = ?", userID).Find(&creds).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to export"})
		return
	}
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
	entries := make([]exportEntry, 0, len(creds))
	for _, cr := range creds {
		entries = append(entries, exportEntry{
			AgentID: cr.AgentID, Protocol: cr.Protocol, Port: cr.Port, Label: cr.Label,
			Username: cr.Username, Domain: cr.Domain,
			PasswordEnc: cr.PasswordEnc, PrivateKeyEnc: cr.PrivateKeyEnc,
		})
	}
	WriteAuditLog(h.db, username, "vault.export", "vault", gin.H{"entries": len(entries)})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": gin.H{
		"format": "wingman-vault-v1",
		"kdf": gin.H{
			"salt":        master.KDFSalt,
			"iterations":  master.KDFIterations,
			"wrapped_key": master.WrappedKey,
		},
		"credentials": entries,
		"exportedAt":  time.Now().UTC().Format(time.RFC3339),
	}})
}

// ---------- 连接快捷调用（guacamole.go 票据签发注入） ----------

// VaultLookupSaved 取目标对应的已存凭据并解密（须已解锁）。给票据签发的
// useSaved 注入用；错误可经 VaultErrStatus/VaultErrMsg 映射为 API 响应。
func VaultLookupSaved(db *gorm.DB, userID uint, agentID, protocol string, port int) (username, password, domain string, err error) {
	dek := vaultKeepAlive(userID)
	if dek == nil {
		return "", "", "", errVaultLocked
	}
	if port <= 0 {
		port = guacProtocolPort(protocol)
	}
	// 先精确端口；非默认端口查不到再回退默认端口条目（保存侧入库前已归一，
	// 两形态通常同一条——回退兜「存时默认端口、连时显式同值端口」的形态差）
	var cred models.RemoteCredential
	if db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
		userID, agentID, protocol, port).First(&cred).Error != nil {
		if port == guacProtocolPort(protocol) ||
			db.Where("user_id = ? AND agent_id = ? AND protocol = ? AND port = ?",
				userID, agentID, protocol, guacProtocolPort(protocol)).First(&cred).Error != nil {
			return "", "", "", errVaultNoSaved
		}
	}
	if cred.PasswordEnc != "" {
		password, err = security.VaultDecryptString(dek, cred.PasswordEnc)
		if err != nil {
			return "", "", "", errVaultDecrypt
		}
	}
	return cred.Username, password, cred.Domain, nil
}

// 保险箱取用错误（票据签发处映射为 API 响应）
var (
	errVaultLocked  = &vaultError{status: http.StatusLocked, msg: "vault locked: unlock before using saved credentials"}
	errVaultNoSaved = &vaultError{status: http.StatusNotFound, msg: "no saved credential for this target"}
	errVaultDecrypt = &vaultError{status: http.StatusInternalServerError, msg: "failed to decrypt saved credential"}
)

type vaultError struct {
	status int
	msg    string
}

func (e *vaultError) Error() string { return e.msg }

// VaultErrStatus 保险箱错误 → HTTP 状态码（非保险箱错误 200 语义占位）
func VaultErrStatus(err error) int {
	if ve, ok := err.(*vaultError); ok {
		return ve.status
	}
	return http.StatusInternalServerError
}

// VaultErrMsg 保险箱错误 → API 文案
func VaultErrMsg(err error) string {
	if err != nil {
		return err.Error()
	}
	return ""
}
