package handlers

// 密钥保险箱 handler 测试：生命周期（setup/unlock/lock/status）、条目管理
// （元数据列表不含密文、留空保旧值、删除幂等）、导出须显式确认、票据
// useSaved 注入。解锁表是包级状态，各用例用独立 userID + defer 锁回，
// 互不污染。

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/gin-gonic/gin"
)

// vaultJSONCtx 构造带登录态与 JSON body 的测试上下文。
func vaultJSONCtx(t *testing.T, userID uint, method, body string) (*gin.Context, *httptest.ResponseRecorder) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	w := httptest.NewRecorder()
	c, _ := gin.CreateTestContext(w)
	c.Set("user_id", userID)
	c.Set("username", "tester")
	c.Set("role", "admin")
	var req *http.Request
	if body != "" {
		req = httptest.NewRequest(method, "/api/remote/vault", strings.NewReader(body))
		req.Header.Set("Content-Type", "application/json")
	} else {
		req = httptest.NewRequest(method, "/api/remote/vault", nil)
	}
	c.Request = req
	return c, w
}

// vaultDo 便捷链：handler 调用 + 状态码断言 + JSON 解码。
func vaultDo(t *testing.T, h *VaultHandler, userID uint, method, path, body string) (int, map[string]any) {
	t.Helper()
	dispatch := map[string]func(*gin.Context){
		"GET /status":           h.HandleStatus,
		"POST /setup":           h.HandleSetup,
		"POST /change-password": h.HandleChangePassword,
		"POST /unlock":          h.HandleUnlock,
		"POST /lock":            h.HandleLock,
		"GET /credentials":      h.HandleList,
		"PUT /credentials":      h.HandleUpsert,
		"GET /export":           h.HandleExport,
	}[method+" "+path]
	if dispatch == nil {
		t.Fatalf("no handler for %s %s", method, path)
	}
	if method == http.MethodGet && path == "/export" {
		// export 读 query，走带 query 的构造
		c, w := vaultJSONCtx(t, userID, http.MethodGet, "")
		c.Request.URL.RawQuery = body // body 复用为 query 串
		dispatch(c)
		return vaultDecode(t, w)
	}
	c, w := vaultJSONCtx(t, userID, method, body)
	dispatch(c)
	return vaultDecode(t, w)
}

func vaultDecode(t *testing.T, w *httptest.ResponseRecorder) (int, map[string]any) {
	t.Helper()
	var m map[string]any
	if err := json.Unmarshal(w.Body.Bytes(), &m); err != nil {
		t.Fatalf("decode response %q: %v", w.Body.String(), err)
	}
	return w.Code, m
}

func vaultDeleteDo(t *testing.T, h *VaultHandler, userID uint, id string) (int, map[string]any) {
	t.Helper()
	c, w := vaultJSONCtx(t, userID, http.MethodDelete, "")
	c.Params = gin.Params{{Key: "id", Value: id}}
	h.HandleDelete(c)
	return vaultDecode(t, w)
}

// vaultSetup 已配置并解锁的保险箱（setup 即解锁）。
func vaultSetup(t *testing.T, h *VaultHandler, userID uint, master string) {
	t.Helper()
	code, resp := vaultDo(t, h, userID, "POST", "/setup",
		fmt.Sprintf(`{"masterPassword":%q}`, master))
	if code != http.StatusOK {
		t.Fatalf("setup: %d %v", code, resp)
	}
}

// ---------- 主口令生命周期 ----------

func TestVaultSetupUnlockLockLifecycle(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(101)
	defer vaultLockUser(uid)

	// 未配置：status 报未配置；unlock 409
	code, resp := vaultDo(t, h, uid, "GET", "/status", "")
	if code != 200 || resp["data"].(map[string]any)["configured"] != false {
		t.Fatalf("initial status: %d %v", code, resp)
	}
	if code, resp = vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"whatever1"}`); code != http.StatusConflict {
		t.Fatalf("unlock before setup: %d %v", code, resp)
	}

	// 弱口令拒绝
	if code, _ = vaultDo(t, h, uid, "POST", "/setup", `{"masterPassword":"short"}`); code != http.StatusBadRequest {
		t.Fatalf("short master should 400, got %d", code)
	}

	// setup 成功即解锁
	vaultSetup(t, h, uid, "master-pass-1")
	code, resp = vaultDo(t, h, uid, "GET", "/status", "")
	data := resp["data"].(map[string]any)
	if code != 200 || data["configured"] != true || data["unlocked"] != true {
		t.Fatalf("post-setup status: %d %v", code, resp)
	}
	if data["unlockedUntil"] == "" || data["unlockedUntil"] == nil {
		t.Fatalf("unlockedUntil should be present: %v", data)
	}

	// 重复 setup 409
	if code, _ = vaultDo(t, h, uid, "POST", "/setup", `{"masterPassword":"master-pass-2"}`); code != http.StatusConflict {
		t.Fatalf("second setup should 409, got %d", code)
	}

	// lock → 锁定；错口令 401；对口令解锁
	vaultDo(t, h, uid, "POST", "/lock", "")
	if code, resp = vaultDo(t, h, uid, "GET", "/status", ""); resp["data"].(map[string]any)["unlocked"] != false {
		t.Fatalf("locked status: %d %v", code, resp)
	}
	if code, _ = vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"master-pass-2"}`); code != http.StatusUnauthorized {
		t.Fatalf("wrong password should 401, got %d", code)
	}
	if code, _ = vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"master-pass-1"}`); code != 200 {
		t.Fatalf("correct unlock: %d", code)
	}
	if code, resp = vaultDo(t, h, uid, "GET", "/status", ""); resp["data"].(map[string]any)["unlocked"] != true {
		t.Fatalf("re-unlocked status: %d %v", code, resp)
	}
}

func TestVaultUnlockTTLExpiryAutoLocks(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(102)
	defer vaultLockUser(uid)
	vaultSetup(t, h, uid, "master-pass-1")

	// 直接把驻留态拨到过期（懒清理路径）
	vaultUnlocksMu.Lock()
	vaultUnlocks[uid].expiresAt = time.Now().Add(-time.Second)
	vaultUnlocksMu.Unlock()
	if dek := vaultKeepAlive(uid); dek != nil {
		t.Fatalf("expired unlock must lazily drop")
	}
	code, resp := vaultDo(t, h, uid, "GET", "/status", "")
	if code != 200 || resp["data"].(map[string]any)["unlocked"] != false {
		t.Fatalf("expired status should be locked: %d %v", code, resp)
	}
}

// ---------- 条目管理 ----------

func TestVaultUpsertListDeleteFlow(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(103)
	defer vaultLockUser(uid)

	// 未解锁：PUT/DELETE 一律 423
	if code, _ := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","password":"pw"}`); code != http.StatusLocked {
		t.Fatalf("upsert while locked should 423, got %d", code)
	}
	vaultSetup(t, h, uid, "master-pass-1")

	// 新条目必须有秘密字段
	if code, _ := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","username":"bob"}`); code != http.StatusBadRequest {
		t.Fatalf("new entry without secret should 400, got %d", code)
	}
	// 协议白名单
	if code, _ := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ftp","password":"pw"}`); code != http.StatusBadRequest {
		t.Fatalf("bad protocol should 400, got %d", code)
	}

	code, resp := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","label":"工作机","username":"bob","password":"secret-pw"}`)
	if code != 200 {
		t.Fatalf("save: %d %v", code, resp)
	}
	entry := resp["data"].(map[string]any)
	if entry["label"] != "工作机" || entry["hasPassword"] != true || entry["port"] != float64(22) {
		t.Fatalf("saved entry meta: %v", entry)
	}

	// 列表：元数据可见，密文与明文密码都不出现
	c, w := vaultJSONCtx(t, uid, http.MethodGet, "")
	h.HandleList(c)
	body := w.Body.String()
	if strings.Contains(body, "secret-pw") || strings.Contains(body, "passwordEnc") {
		t.Fatalf("list leaked secret: %s", body)
	}
	code, resp = vaultDecode(t, w)
	list := resp["data"].([]any)
	if code != 200 || len(list) != 1 {
		t.Fatalf("list: %d %v", code, resp)
	}
	got := list[0].(map[string]any)
	if got["agentId"] != "ag1" || got["username"] != "bob" || got["hasPassword"] != true {
		t.Fatalf("list entry: %v", got)
	}
	credID := fmt.Sprintf("%.0f", got["id"])

	// 删除须解锁 + 归属校验（他人 ID 404）
	if code, _ := vaultDeleteDo(t, h, 999, credID); code != http.StatusLocked {
		t.Fatalf("delete while locked should 423, got %d", code)
	}
	if code, _ := vaultDeleteDo(t, h, uid, "999999"); code != http.StatusNotFound {
		t.Fatalf("unknown id should 404, got %d", code)
	}
	if code, _ := vaultDeleteDo(t, h, uid, credID); code != 200 {
		t.Fatalf("delete: %d", code)
	}
	_, resp = vaultDo(t, h, uid, "GET", "/credentials", "")
	if n := len(resp["data"].([]any)); n != 0 {
		t.Fatalf("list after delete should be empty, got %d", n)
	}
}

func TestVaultUpsertKeepsSecretsWhenOmitted(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(104)
	defer vaultLockUser(uid)
	vaultSetup(t, h, uid, "master-pass-1")

	vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"rdp","port":3389,"label":"旧标签","username":"bob","password":"secret-pw"}`)
	// 改标签（秘密字段留空）：元数据更新、密文保留
	code, resp := vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"rdp","port":3389,"label":"新标签","username":"bob"}`)
	if code != 200 {
		t.Fatalf("relabel: %d %v", code, resp)
	}
	if entry := resp["data"].(map[string]any); entry["label"] != "新标签" || entry["hasPassword"] != true {
		t.Fatalf("relabel entry: %v", entry)
	}
	// 密文确实还在（经取用通道验证，而非仅看 hasPassword 位）
	user, pass, _, err := VaultLookupSaved(db, uid, "ag1", "rdp", 3389)
	if err != nil || user != "bob" || pass != "secret-pw" {
		t.Fatalf("secret must survive relabel: %q %q %v", user, pass, err)
	}
}

// ---------- 取用（票据注入的数据源） ----------

func TestVaultLookupSavedPortFallbackAndErrors(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(105)
	defer vaultLockUser(uid)

	// 锁定态：errVaultLocked
	if _, _, _, err := VaultLookupSaved(db, uid, "ag1", "ssh", 22); VaultErrStatus(err) != http.StatusLocked {
		t.Fatalf("locked lookup: %v", err)
	}
	vaultSetup(t, h, uid, "master-pass-1")
	vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","username":"bob","password":"secret-pw"}`)

	// 显式 22 与归一后的默认端口是同一条；非默认端口查不到回退默认条目
	for _, port := range []int{22, 2222} {
		user, pass, _, err := VaultLookupSaved(db, uid, "ag1", "ssh", port)
		if err != nil || user != "bob" || pass != "secret-pw" {
			t.Fatalf("lookup port %d: %q %q %v", port, user, pass, err)
		}
	}
	// 不同协议无条目
	if _, _, _, err := VaultLookupSaved(db, uid, "ag1", "vnc", 5900); VaultErrStatus(err) != http.StatusNotFound {
		t.Fatalf("missing entry should 404, got %v", err)
	}
}

// ---------- 导出 ----------

func TestVaultExportRequiresConfirmAndNeverPlaintext(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(106)
	defer vaultLockUser(uid)
	vaultSetup(t, h, uid, "master-pass-1")
	vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","username":"bob","password":"secret-pw"}`)
	vaultDo(t, h, uid, "POST", "/lock", "")

	// 缺 confirm：400（隐私默认——导出必须明示）
	if code, _ := vaultDo(t, h, uid, "GET", "/export", ""); code != http.StatusBadRequest {
		t.Fatalf("export without confirm should 400, got %d", code)
	}
	// confirm 但未解锁：423
	if code, _ := vaultDo(t, h, uid, "GET", "/export", "confirm=true"); code != http.StatusLocked {
		t.Fatalf("export while locked should 423, got %d", code)
	}
	// 解锁 + confirm：密文束，永不含明文
	vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"master-pass-1"}`)
	c, w := vaultJSONCtx(t, uid, http.MethodGet, "")
	c.Request.URL.RawQuery = "confirm=true"
	h.HandleExport(c)
	if w.Code != 200 {
		t.Fatalf("export: %d %s", w.Code, w.Body.String())
	}
	if strings.Contains(w.Body.String(), "secret-pw") {
		t.Fatalf("export leaked plaintext: %s", w.Body.String())
	}
	var bundle struct {
		Data struct {
			Format string `json:"format"`
			KDF    struct {
				Salt       string `json:"salt"`
				WrappedKey string `json:"wrapped_key"`
			} `json:"kdf"`
			Credentials []struct {
				PasswordEnc string `json:"passwordEnc"`
			} `json:"credentials"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &bundle); err != nil {
		t.Fatalf("decode bundle: %v", err)
	}
	if bundle.Data.Format != "wingman-vault-v1" || bundle.Data.KDF.WrappedKey == "" ||
		len(bundle.Data.Credentials) != 1 || bundle.Data.Credentials[0].PasswordEnc == "" {
		t.Fatalf("bundle shape: %+v", bundle.Data)
	}
}

// ---------- 换主口令 ----------

func TestVaultChangePasswordKeepsEntries(t *testing.T) {
	db := newDB(t)
	h := NewVaultHandler(db)
	const uid = uint(107)
	defer vaultLockUser(uid)
	vaultSetup(t, h, uid, "master-pass-1")
	vaultDo(t, h, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","username":"bob","password":"secret-pw"}`)

	// 当前口令错误 → 401
	if code, _ := vaultDo(t, h, uid, "POST", "/change-password",
		`{"currentPassword":"wrong-pass-x","newPassword":"master-pass-2"}`); code != http.StatusUnauthorized {
		t.Fatalf("wrong current should 401, got %d", code)
	}
	if code, _ := vaultDo(t, h, uid, "POST", "/change-password",
		`{"currentPassword":"master-pass-1","newPassword":"master-pass-2"}`); code != 200 {
		t.Fatalf("change password: %d", code)
	}
	// 换口令后：旧口令解锁失败、新口令成功、已存条目照常解密
	vaultDo(t, h, uid, "POST", "/lock", "")
	if code, _ := vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"master-pass-1"}`); code != http.StatusUnauthorized {
		t.Fatalf("old password should 401 after change, got %d", code)
	}
	if code, _ := vaultDo(t, h, uid, "POST", "/unlock", `{"masterPassword":"master-pass-2"}`); code != 200 {
		t.Fatalf("new password should unlock, got %d", code)
	}
	_, pass, _, err := VaultLookupSaved(db, uid, "ag1", "ssh", 22)
	if err != nil || pass != "secret-pw" {
		t.Fatalf("entries must survive rekey: %q %v", pass, err)
	}
}

// ---------- 票据 useSaved 注入（guacamole.go ↔ vault.go 接线） ----------

func TestGuacTicketUseSavedInjectsVaultCredential(t *testing.T) {
	db := newDB(t)
	registry, _ := newRegistry(t)
	registry.Register("ag1", "host1", "127.0.0.1", nil)

	tickets := remoteticket.NewManager()
	defer tickets.Stop()
	guac := NewGuacamoleHandler(db, registry, tickets, "127.0.0.1:4822", "", "", "")
	vh := NewVaultHandler(db)
	const uid = uint(108)
	defer vaultLockUser(uid)

	// 未存凭据 + useSaved：404
	vaultSetup(t, vh, uid, "master-pass-1")
	body := `{"agentId":"ag1","protocol":"ssh","readOnly":true,"useSaved":true}`
	c, w := vaultJSONCtx(t, uid, http.MethodPost, body)
	guac.HandleTicketCreate(c)
	if w.Code != http.StatusNotFound {
		t.Fatalf("useSaved without entry should 404, got %d %s", w.Code, w.Body.String())
	}

	// 存一条（agent/协议对应，端口走默认归一）
	vaultDo(t, vh, uid, "PUT", "/credentials",
		`{"agentId":"ag1","protocol":"ssh","username":"bob","password":"secret-pw"}`)

	c, w = vaultJSONCtx(t, uid, http.MethodPost, body)
	guac.HandleTicketCreate(c)
	if w.Code != http.StatusOK {
		t.Fatalf("ticket with useSaved: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data struct {
			Ticket string `json:"ticket"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil || resp.Data.Ticket == "" {
		t.Fatalf("ticket response: %s", w.Body.String())
	}
	tk, err := tickets.ValidateTicket(resp.Data.Ticket)
	if err != nil {
		t.Fatalf("validate ticket: %v", err)
	}
	if tk.Params["username"] != "bob" || tk.Params["password"] != "secret-pw" {
		t.Fatalf("vault injection missing: %v", tk.Params)
	}

	// 现场显式值优先于已存值（fill-missing 语义）
	c, w = vaultJSONCtx(t, uid, http.MethodPost,
		`{"agentId":"ag1","protocol":"ssh","readOnly":true,"useSaved":true,"username":"alice"}`)
	guac.HandleTicketCreate(c)
	if w.Code != http.StatusOK {
		t.Fatalf("explicit+saved ticket: %d %s", w.Code, w.Body.String())
	}
	var resp2 struct {
		Data struct {
			Ticket string `json:"ticket"`
		} `json:"data"`
	}
	_ = json.Unmarshal(w.Body.Bytes(), &resp2)
	tk2, _ := tickets.ValidateTicket(resp2.Data.Ticket)
	if tk2.Params["username"] != "alice" || tk2.Params["password"] != "secret-pw" {
		t.Fatalf("explicit must win username, saved fills password: %v", tk2.Params)
	}

	// 审计只记来源枚举，不记凭据
	var logs []models.AuditLog
	db.Where("kind = ?", "desktop.ticket").Find(&logs)
	if len(logs) == 0 {
		t.Fatalf("ticket audit rows missing")
	}
	raw, _ := json.Marshal(logs)
	if strings.Contains(string(raw), "secret-pw") {
		t.Fatalf("audit leaked credential: %s", raw)
	}
}
