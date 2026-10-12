package handlers

// 密钥保险箱 HTTP 适配层。加密落库与解锁会话在 internal/vault 的 provider
// 实现（默认 local：主口令派生 KEK 包裹 per-user DEK，解锁期 DEK 只驻留
// 内存，空闲 30 分钟自动锁回；可经 WINGMAN_VAULT_PROVIDER 切换实现）。
// 本层统一负责三件事，provider 实现不重复做：
//   - 请求形状校验（非空、协议白名单、端口归一、export 显式确认）；
//   - 审计（WriteAuditLog，敏感值不进审计——见各 handler）；
//   - provider sentinel 错误 → HTTP 状态码映射（vaultErrStatus）。
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
// 连接快捷调用（useSaved）在 guacamole.go 的票据签发里注入（VaultLookupSaved），
// 明文只在那一刻解密进 connect 参数，不回传浏览器、不进日志与审计。

import (
	"errors"
	"net/http"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/vault"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// VaultHandler 保险箱端点。provider 每请求现取（默认 vault.Active()）——
// 配置切换即时生效，端点与业务代码不感知实现。
type VaultHandler struct {
	db *gorm.DB // 审计落库（审计统一在适配层记，provider 不重复记）
	// pick provider 来源；默认 vault.Active，测试可替换（未装配分支）。
	pick func() vault.Provider
}

func NewVaultHandler(db *gorm.DB) *VaultHandler {
	return &VaultHandler{db: db, pick: vault.Active}
}

// vaultDeps 请求公共前置：激活 provider + 登录态。未装配 provider（启动
// 缺位）报 500，不悄悄回退到任何默认实现。
func (h *VaultHandler) vaultDeps(c *gin.Context) (vault.Provider, uint, string, bool) {
	p := h.pick()
	if p == nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "vault provider not configured"})
		return nil, 0, "", false
	}
	userID, username, _ := middleware.GetCurrentUser(c)
	return p, userID, username, true
}

// ---------- 状态 / 主口令生命周期 ----------

// HandleStatus GET /api/remote/vault/status
// @Summary      保险箱状态
// @Description  是否已设置主口令、当前是否解锁（含空闲自动锁定倒计时基准）
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/status [get]
func (h *VaultHandler) HandleStatus(c *gin.Context) {
	p, userID, _, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	st, err := p.Status(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to query vault status"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": st})
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	var req struct {
		MasterPassword string `json:"masterPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || strings.TrimSpace(req.MasterPassword) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "masterPassword required"})
		return
	}
	until, err := p.Setup(userID, req.MasterPassword)
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrWeakMaster), errors.Is(err, vault.ErrAlreadyConfigured):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to init vault"})
		return
	}
	WriteAuditLog(h.db, username, "vault.setup", strconv.FormatUint(uint64(userID), 10), nil)
	c.JSON(http.StatusOK, gin.H{"success": true, "data": gin.H{
		"unlockedUntil": until.UTC().Format(time.RFC3339),
	}})
}

// HandleChangePassword POST /api/remote/vault/change-password {currentPassword, newPassword}
// 换口令由 provider 只重包裹 DEK（DEK 不变 → 已存密文全部继续有效）。
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	var req struct {
		CurrentPassword string `json:"currentPassword"`
		NewPassword     string `json:"newPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || req.CurrentPassword == "" || req.NewPassword == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "currentPassword and newPassword required"})
		return
	}
	err := p.ChangePassword(userID, req.CurrentPassword, req.NewPassword)
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrWrongCurrent):
		WriteAuditLog(h.db, username, "vault.change_password", "vault", gin.H{"result": "wrong current password"})
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	case errors.Is(err, vault.ErrWeakMaster), errors.Is(err, vault.ErrNotConfigured):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to rekey vault"})
		return
	}
	WriteAuditLog(h.db, username, "vault.change_password", "vault", nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleUnlock POST /api/remote/vault/unlock {masterPassword}
// @Summary      解锁保险箱
// @Description  校验主口令并进入解锁态（空闲自动锁回）；连续失败记审计
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	var req struct {
		MasterPassword string `json:"masterPassword"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || req.MasterPassword == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "masterPassword required"})
		return
	}
	until, err := p.Unlock(userID, req.MasterPassword)
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrNotConfigured):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	case errors.Is(err, vault.ErrWrongMaster):
		WriteAuditLog(h.db, username, "vault.unlock", "vault", gin.H{"result": "failed"})
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to unlock"})
		return
	}
	WriteAuditLog(h.db, username, "vault.unlock", "vault", gin.H{"result": "ok"})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": gin.H{
		"unlockedUntil": until.UTC().Format(time.RFC3339),
	}})
}

// HandleLock POST /api/remote/vault/lock
// @Summary      锁定保险箱
// @Description  立即丢弃内存中的解锁会话（幂等）；此后取用/管理均要求重新解锁
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/lock [post]
func (h *VaultHandler) HandleLock(c *gin.Context) {
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	p.Lock(userID)
	WriteAuditLog(h.db, username, "vault.lock", "vault", nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// ---------- 凭据条目 ----------

// HandleList GET /api/remote/vault/credentials
// @Summary      保险箱凭据列表（元数据）
// @Description  只含标签/目标/用户名等元数据与「有无密码」标志，密文与明文一律不出
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /remote/vault/credentials [get]
func (h *VaultHandler) HandleList(c *gin.Context) {
	p, userID, _, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	metas, err := p.List(userID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to list credentials"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": metas})
}

// HandleUpsert PUT /api/remote/vault/credentials（须解锁）
// 秘密字段留空 + 条目已存在 = 只改标签/用户名等元数据（「改标签」路径）；
// 新条目必须至少给一个秘密字段。协议白名单/端口归一在本层（guacd 承接面），
// 归一后的 (agent, 协议, 端口) 交给 provider。
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
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

	meta, updated, err := p.Upsert(userID, vault.Entry{
		AgentID: req.AgentID, Protocol: req.Protocol, Port: req.Port,
		Label: strings.TrimSpace(req.Label), Username: req.Username, Domain: req.Domain,
		Password: req.Password, PrivateKey: req.PrivateKey,
	})
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrLocked), errors.Is(err, vault.ErrNeedSecret):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to save credential"})
		return
	}
	WriteAuditLog(h.db, username, "vault.save", req.AgentID, gin.H{
		"protocol": req.Protocol, "port": req.Port, "label": meta.Label, "updated": updated,
	})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": meta})
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid credential id"})
		return
	}
	err = p.Delete(userID, uint(id))
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrLocked), errors.Is(err, vault.ErrNotFound):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete credential"})
		return
	}
	WriteAuditLog(h.db, username, "vault.delete", c.Param("id"), nil)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleExport GET /api/remote/vault/export?confirm=true（须解锁 + 显式确认）
// 导出的是主口令加密的密文束，可用主口令在未来恢复——明文永不导出
// （隐私默认：凭据不出本机，导出须明示）。束格式由 provider 自定义
// （local 为 wingman-vault-v1）。
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
	p, userID, username, ok := h.vaultDeps(c)
	if !ok {
		return
	}
	if c.Query("confirm") != "true" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false,
			"error": "export requires explicit confirm=true (credentials never leave in plaintext)"})
		return
	}
	bundle, err := p.Export(userID)
	switch {
	case err == nil:
	case errors.Is(err, vault.ErrLocked), errors.Is(err, vault.ErrNotConfigured):
		c.JSON(vaultErrStatus(err), gin.H{"success": false, "error": err.Error()})
		return
	default:
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to export"})
		return
	}
	WriteAuditLog(h.db, username, "vault.export", "vault", gin.H{"entries": vaultExportEntryCount(bundle)})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": bundle})
}

// vaultExportEntryCount 审计用条目计数（束格式 provider 自定义，按切片
// 长度数，形状不符计 0）。
func vaultExportEntryCount(bundle map[string]any) int {
	if rv := reflect.ValueOf(bundle["credentials"]); rv.Kind() == reflect.Slice || rv.Kind() == reflect.Array {
		return rv.Len()
	}
	return 0
}

// ---------- 连接快捷调用（guacamole.go 票据签发注入） ----------

// vaultLookupError 取用语义错误：文案保持票据面口径，链上保留 provider
// sentinel（Unwrap）供 VaultErrStatus/VaultErrMsg 的 errors.Is 映射。
type vaultLookupError struct {
	msg   string
	cause error
}

func (e *vaultLookupError) Error() string { return e.msg }
func (e *vaultLookupError) Unwrap() error { return e.cause }

var (
	errVaultLookupLocked  = &vaultLookupError{"vault locked: unlock before using saved credentials", vault.ErrLocked}
	errVaultLookupNoSaved = &vaultLookupError{"no saved credential for this target", vault.ErrNotFound}
	errVaultLookupDecrypt = &vaultLookupError{"failed to decrypt saved credential", vault.ErrDecrypt}
)

// VaultLookupSaved 取目标对应的已存凭据并解密（须已解锁）。给票据签发的
// useSaved 注入用；经当前激活 provider，错误归一为取用语义文案，可经
// VaultErrStatus/VaultErrMsg 映射为 API 响应。
func VaultLookupSaved(userID uint, agentID, protocol string, port int) (username, password, domain string, err error) {
	p := vault.Active()
	if p == nil {
		return "", "", "", errors.New("vault provider not configured")
	}
	username, password, domain, err = p.Lookup(userID, agentID, protocol, port)
	if err == nil {
		return username, password, domain, nil
	}
	switch {
	case errors.Is(err, vault.ErrLocked):
		return "", "", "", errVaultLookupLocked
	case errors.Is(err, vault.ErrNotFound):
		return "", "", "", errVaultLookupNoSaved
	case errors.Is(err, vault.ErrDecrypt):
		return "", "", "", errVaultLookupDecrypt
	default:
		return "", "", "", err
	}
}

// vaultErrStatus provider sentinel 错误 → HTTP 状态码（非保险箱语义错误
// 500，具体文案由各 handler 按操作给）。
func vaultErrStatus(err error) int {
	switch {
	case errors.Is(err, vault.ErrLocked):
		return http.StatusLocked
	case errors.Is(err, vault.ErrNotFound):
		return http.StatusNotFound
	case errors.Is(err, vault.ErrWrongMaster), errors.Is(err, vault.ErrWrongCurrent):
		return http.StatusUnauthorized
	case errors.Is(err, vault.ErrAlreadyConfigured), errors.Is(err, vault.ErrNotConfigured):
		return http.StatusConflict
	case errors.Is(err, vault.ErrWeakMaster), errors.Is(err, vault.ErrNeedSecret):
		return http.StatusBadRequest
	default:
		return http.StatusInternalServerError
	}
}

// VaultErrStatus 保险箱错误 → HTTP 状态码（guacamole 票据路径用）
func VaultErrStatus(err error) int {
	return vaultErrStatus(err)
}

// VaultErrMsg 保险箱错误 → API 文案
func VaultErrMsg(err error) string {
	if err != nil {
		return err.Error()
	}
	return ""
}
