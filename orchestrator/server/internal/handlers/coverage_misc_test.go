package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
)

// ---------- Auth 剩余分支 ----------

func TestLoginValidationBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatalf("hash: %v", err)
	}
	user := models.User{Username: "loginuser", Password: hash, Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}

	ah := NewAuthHandler(db)
	r := gin.New()
	r.POST("/login", ah.HandleLogin)
	r.POST("/logout", ah.HandleLogout)

	// 缺字段 → 400
	w := doJSON(r, "POST", "/login", map[string]any{"username": "loginuser"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing password: expected 400, got %d", w.Code)
	}

	// 用户不存在 → 401
	w = doJSON(r, "POST", "/login", map[string]any{"username": "ghost-user-xyz", "password": "whatever"})
	if w.Code != http.StatusUnauthorized {
		t.Errorf("unknown user: expected 401, got %d", w.Code)
	}

	// 密码错误 → 401
	w = doJSON(r, "POST", "/login", map[string]any{"username": "loginuser", "password": "WrongPass1!"})
	if w.Code != http.StatusUnauthorized {
		t.Errorf("bad password: expected 401, got %d", w.Code)
	}

	// logout → 200
	w = doJSON(r, "POST", "/logout", nil)
	if w.Code != http.StatusOK {
		t.Errorf("logout: expected 200, got %d", w.Code)
	}
}

func TestInitAdminBootstrap(t *testing.T) {
	gin.SetMode(gin.TestMode)

	t.Setenv("WINGMAN_ADMIN_PASSWORD", "")
	dbEmpty := newDB(t)
	NewAuthHandler(dbEmpty).InitAdmin()
	var count int64
	dbEmpty.Model(&models.User{}).Count(&count)
	if count != 0 {
		t.Errorf("no env password: expected 0 users, got %d", count)
	}

	t.Setenv("WINGMAN_ADMIN_PASSWORD", "Str0ng!pw")
	NewAuthHandler(dbEmpty).InitAdmin()
	dbEmpty.Model(&models.User{}).Count(&count)
	if count != 1 {
		t.Fatalf("bootstrap: expected 1 user, got %d", count)
	}
	var admin models.User
	if err := dbEmpty.First(&admin, "username = ?", "admin").Error; err != nil {
		t.Fatalf("admin should exist: %v", err)
	}
	if admin.Role != "admin" || !admin.Active {
		t.Errorf("unexpected admin record: %+v", admin)
	}
	if !security.VerifyPassword(admin.Password, "Str0ng!pw") {
		t.Error("bootstrap password should verify")
	}

	// 已有用户 → 跳过
	dbWithUser := newDB(t)
	existing := models.User{Username: "someone", Password: "x", Role: "viewer", Active: true}
	if err := dbWithUser.Create(&existing).Error; err != nil {
		t.Fatal(err)
	}
	NewAuthHandler(dbWithUser).InitAdmin()
	dbWithUser.Model(&models.User{}).Count(&count)
	if count != 1 {
		t.Errorf("existing users: expected 1 user, got %d", count)
	}
}

// ---------- Settings ----------

func TestSettingsGetUpdateAndLegacy(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	sh := NewSettingsHandler(db)

	r := gin.New()
	r.GET("/settings", sh.HandleGetSettings)
	r.PUT("/settings", sh.HandleUpdateSettings)
	// 兼容路由（同名包级函数）
	r.GET("/v1/settings", HandleGetSettings)
	r.PUT("/v1/settings", HandleUpdateSettings)

	// 默认值
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/settings", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("get defaults: %d", w.Code)
	}
	var resp struct {
		Data map[string]string `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp.Data["serverPort"] != "9527" {
		t.Errorf("expected default serverPort 9527, got %v", resp.Data)
	}

	// 预置一条配置
	db.Create(&models.Settings{Key: "serverPort", Value: "9000"})

	// 更新已有键 + 新键
	w = doJSON(r, "PUT", "/settings", map[string]string{"serverPort": "9001", "logLevel": "debug"})
	if w.Code != http.StatusOK {
		t.Fatalf("update: %d %s", w.Code, w.Body.String())
	}

	// 读回
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/settings", nil))
	json.Unmarshal(w.Body.Bytes(), &resp)
	if resp.Data["serverPort"] != "9001" || resp.Data["logLevel"] != "debug" {
		t.Errorf("unexpected settings after update: %v", resp.Data)
	}

	// 非法 body → 400
	req := httptest.NewRequest("PUT", "/settings", strings.NewReader("not-json"))
	req.Header.Set("Content-Type", "application/json")
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body: expected 400, got %d", w.Code)
	}

	// legacy 路由
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/v1/settings", nil))
	if w.Code != http.StatusOK {
		t.Errorf("legacy get: %d", w.Code)
	}
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("PUT", "/v1/settings", nil))
	if w.Code != http.StatusOK {
		t.Errorf("legacy update: %d", w.Code)
	}
}

// ---------- Status ----------

func TestStatusHandlerBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, _ := newRegistry(t)
	st := NewStatusHandler(db, reg)

	r := gin.New()
	r.GET("/status", st.HandleStatus)
	r.GET("/health", st.HandleHealth)

	getData := func() map[string]any {
		w := httptest.NewRecorder()
		r.ServeHTTP(w, httptest.NewRequest("GET", "/status", nil))
		if w.Code != http.StatusOK {
			t.Fatalf("status: %d", w.Code)
		}
		var resp struct {
			Data map[string]any `json:"data"`
		}
		if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
			t.Fatal(err)
		}
		return resp.Data
	}

	// 无 agent → 全零
	data := getData()
	if data["totalScripts"].(float64) != 0 || data["totalWindows"].(float64) != 0 {
		t.Errorf("expected zeros without agent, got %v", data)
	}

	// get_status 成功返回完整数据
	reg.Register("s1", "h1", "10.0.0.1", &handlerMockConn{responses: []map[string]any{{
		"success": true,
		"data": map[string]any{
			"totalScripts":   float64(5),
			"runningScripts": float64(2),
			"totalWindows":   float64(3),
			"cpuUsage":       float64(12.5),
			"memoryUsage":    float64(40.2),
		},
	}}})
	data = getData()
	if data["totalScripts"].(float64) != 5 || data["runningScripts"].(float64) != 2 {
		t.Errorf("unexpected stats from get_status: %v", data)
	}
	if data["totalWindows"].(float64) != 3 || data["cpuUsage"].(float64) != 12.5 {
		t.Errorf("unexpected metrics: %v", data)
	}

	// get_status 失败 → 回退 list_windows
	reg2, _ := newRegistry(t)
	st2 := NewStatusHandler(db, reg2)
	r2 := gin.New()
	r2.GET("/status", st2.HandleStatus)
	reg2.Register("s2", "h2", "10.0.0.2", &handlerMockConn{
		errs: []error{errBoom},
		responses: []map[string]any{
			nil, // get_status 失败
			{"data": map[string]any{"windows": []any{map[string]any{"h": 1}, map[string]any{"h": 2}}}},
		},
	})
	w := httptest.NewRecorder()
	r2.ServeHTTP(w, httptest.NewRequest("GET", "/status", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("fallback status: %d", w.Code)
	}
	var resp2 struct {
		Data map[string]any `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp2)
	if resp2.Data["totalWindows"].(float64) != 2 {
		t.Errorf("expected 2 windows from fallback, got %v", resp2.Data)
	}

	// health
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/health", nil))
	if w.Code != http.StatusOK {
		t.Errorf("health: %d", w.Code)
	}
}

var errBoom = &boomError{}

type boomError struct{}

func (e *boomError) Error() string { return "boom" }

// ---------- Window ----------

func TestWindowHandlerBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	wh := NewWindowHandler(reg)

	r := gin.New()
	r.GET("/windows", wh.HandleList)

	// 无 agent（未指定）
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/windows", nil))
	if w.Code != http.StatusServiceUnavailable {
		t.Errorf("no agent: expected 503, got %d", w.Code)
	}

	// 无 agent（指定）
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/windows?agentId=a9", nil))
	if w.Code != http.StatusServiceUnavailable {
		t.Errorf("missing specified agent: expected 503, got %d", w.Code)
	}

	// 命令错误 → 502
	regErr, _ := newRegistry(t)
	whErr := NewWindowHandler(regErr)
	rErr := gin.New()
	rErr.GET("/windows", whErr.HandleList)
	regErr.Register("a1", "h", "10.0.0.1", &handlerMockConn{errs: []error{errBoom}})
	w = httptest.NewRecorder()
	rErr.ServeHTTP(w, httptest.NewRequest("GET", "/windows", nil))
	if w.Code != http.StatusBadGateway {
		t.Errorf("command error: expected 502, got %d", w.Code)
	}

	// success=false → 200 带错误信息
	regFail, _ := newRegistry(t)
	whFail := NewWindowHandler(regFail)
	rFail := gin.New()
	rFail.GET("/windows", whFail.HandleList)
	regFail.Register("a1", "h", "10.0.0.1", &handlerMockConn{responses: []map[string]any{{"success": false, "message": "boom"}}})
	w = httptest.NewRecorder()
	rFail.ServeHTTP(w, httptest.NewRequest("GET", "/windows", nil))
	if w.Code != http.StatusOK || !strings.Contains(w.Body.String(), "boom") {
		t.Errorf("agent-side failure: %d %s", w.Code, w.Body.String())
	}

	// success=true + data → 200
	regOK, _ := newRegistry(t)
	whOK := NewWindowHandler(regOK)
	rOK := gin.New()
	rOK.GET("/windows", whOK.HandleList)
	regOK.Register("a1", "h", "10.0.0.1", &handlerMockConn{responses: []map[string]any{
		{"success": true, "data": map[string]any{"windows": []any{}}},
	}})
	w = httptest.NewRecorder()
	rOK.ServeHTTP(w, httptest.NewRequest("GET", "/windows?agentId=a1", nil))
	if w.Code != http.StatusOK {
		t.Errorf("happy path: expected 200, got %d", w.Code)
	}
}

// ---------- Debugger 直连端点 ----------

func TestDebuggerDirectAttachEndpoints(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	dh := NewDebugHandler(reg)

	r := gin.New()
	r.POST("/debugger/command", dh.HandleDebuggerCommand)
	r.GET("/debugger/breakpoints", dh.HandleDebuggerGetBreakpoints)
	r.POST("/debugger/breakpoints", dh.HandleDebuggerSetBreakpoints)

	for path, method := range map[string]string{
		"/debugger/command":     "POST",
		"/debugger/breakpoints": "GET",
	} {
		w := httptest.NewRecorder()
		var req = httptest.NewRequest(method, path, nil)
		r.ServeHTTP(w, req)
		if w.Code != http.StatusNotImplemented {
			t.Errorf("%s: expected 501, got %d", path, w.Code)
		}
		if !strings.Contains(w.Body.String(), "direct_attach") {
			t.Errorf("%s: expected direct_attach mode in body: %s", path, w.Body.String())
		}
	}

	// POST 版 set_breakpoints
	w := doJSON(r, "POST", "/debugger/breakpoints", map[string]any{"breakpoints": []any{}})
	if w.Code != http.StatusNotImplemented {
		t.Errorf("set breakpoints: expected 501, got %d", w.Code)
	}

	if firstNonEmpty("", "", "") != "" {
		t.Error("all-empty firstNonEmpty should return empty")
	}
	if firstNonEmpty("", "x") != "x" {
		t.Error("firstNonEmpty should skip empties")
	}
}

// ---------- selectAgent ----------

func TestSelectAgentBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	conn := &handlerMockConn{}
	reg.Register("a1", "h", "10.0.0.1", conn)
	reg.Register("noclient", "h", "10.0.0.2", nil)

	if got := selectAgent(reg, "a1"); got == nil || got.AgentID != "a1" {
		t.Errorf("explicit match: expected a1, got %+v", got)
	}
	if got := selectAgent(reg, "ghost"); got != nil {
		t.Errorf("explicit miss should be nil, got %+v", got)
	}
	if got := selectAgent(reg, ""); got == nil || got.AgentID != "a1" {
		t.Errorf("auto pick should return first online with client, got %+v", got)
	}

	reg2, _ := newRegistry(t)
	reg2.Register("noclient", "h", "10.0.0.3", nil)
	if got := selectAgent(reg2, ""); got != nil {
		t.Errorf("client-less agents must not be selected, got %+v", got)
	}
}

// ---------- 纯函数 ----------

func TestAuditPureHelpers(t *testing.T) {
	// parseRFC3339
	if !parseRFC3339("").IsZero() {
		t.Error("empty string should parse to zero time")
	}
	if !parseRFC3339("not-a-time").IsZero() {
		t.Error("invalid string should parse to zero time")
	}
	if got := parseRFC3339("2026-01-02T15:04:05Z"); got.IsZero() {
		t.Error("valid RFC3339 should parse")
	} else if got.Year() != 2026 {
		t.Errorf("unexpected year: %d", got.Year())
	}

	// parsePositiveInt
	if parsePositiveInt("", 7) != 7 {
		t.Error("empty should fall back")
	}
	if parsePositiveInt("abc", 7) != 7 {
		t.Error("non-numeric should fall back")
	}
	if parsePositiveInt("0", 7) != 7 {
		t.Error("zero should fall back")
	}
	if parsePositiveInt("-3", 7) != 7 {
		t.Error("negative should fall back")
	}
	if parsePositiveInt("42", 7) != 42 {
		t.Error("valid value should parse")
	}

	// splitCSV
	if splitCSV("  ") != nil {
		t.Error("blank CSV should be nil")
	}
	if got := splitCSV("a, b ,c,"); len(got) != 3 || got[0] != "a" || got[1] != "b" || got[2] != "c" {
		t.Errorf("unexpected splitCSV result: %v", got)
	}

	// deriveIPRegion
	if _, ok := deriveIPRegion(map[string]any{"ip_region": "LAN"}); ok {
		t.Error("existing ip_region should not derive")
	}
	if _, ok := deriveIPRegion(map[string]any{}); ok {
		t.Error("missing ip should not derive")
	}
	if _, ok := deriveIPRegion(map[string]any{"ip": 123}); ok {
		t.Error("non-string ip should not derive")
	}
	if _, ok := deriveIPRegion(map[string]any{"ip": "  "}); ok {
		t.Error("blank ip should not derive")
	}
	if _, ok := deriveIPRegion(map[string]any{"ip": "not-an-ip"}); ok {
		t.Error("unparseable ip should not derive")
	}
	if _, ok := deriveIPRegion(map[string]any{"ip": "8.8.8.8"}); ok {
		t.Error("public ip should not derive")
	}
	for _, ip := range []string{"127.0.0.1", "192.168.1.5", "169.254.0.1", "ff01::1", "10.1.2.3"} {
		if region, ok := deriveIPRegion(map[string]any{"ip": ip}); !ok || region != "LAN" {
			t.Errorf("ip %s should derive LAN, got %q ok=%v", ip, region, ok)
		}
	}

	// WriteAuditLog nil db / 空 kind 不应 panic
	WriteAuditLog(nil, "a", "", "t", nil)
}

func TestUsersPureHelpers(t *testing.T) {
	if isUniqueConstraint(nil) {
		t.Error("nil error is not unique constraint")
	}
	if !isUniqueConstraint(errFromString("UNIQUE constraint failed: users.username")) {
		t.Error("UNIQUE constraint should be detected")
	}
	if !isUniqueConstraint(errFromString("Error 1062: Duplicate entry 'x'")) {
		t.Error("Duplicate entry should be detected")
	}
	if isUniqueConstraint(errFromString("some other error")) {
		t.Error("other errors should not be detected")
	}

	if validUsername("ab") {
		t.Error("too-short username should fail")
	}
	if validUsername(strings.Repeat("a", 33)) {
		t.Error("too-long username should fail")
	}
	if validUsername("bad name!") {
		t.Error("invalid chars should fail")
	}
	if !validUsername("Good-Name_1") {
		t.Error("letters/digits/_/- should pass")
	}
}

type stringError string

func (e stringError) Error() string { return string(e) }

func errFromString(s string) error { return stringError(s) }

func TestRolesPureHelpers(t *testing.T) {
	// normalizeCodes
	if got := normalizeCodes([]string{"  ", "", "a", "a", " b "}); len(got) != 2 || got[0] != "a" || got[1] != "b" {
		t.Errorf("unexpected normalizeCodes: %v", got)
	}
	if got := normalizeCodes(nil); len(got) != 0 {
		t.Errorf("nil input should be empty, got %v", got)
	}

	// validRoleCode
	if validRoleCode("a") {
		t.Error("1-char code should fail")
	}
	if validRoleCode(strings.Repeat("a", 33)) {
		t.Error("33-char code should fail")
	}
	if validRoleCode("bad code") {
		t.Error("space in code should fail")
	}
	if !validRoleCode("op_01-A") {
		t.Error("valid code should pass")
	}

	// toRoleDTO 空权限 → []
	dto := toRoleDTO(models.Role{})
	if dto.Permissions == nil || len(dto.Permissions) != 0 {
		t.Errorf("nil permissions should become empty slice, got %v", dto.Permissions)
	}
	if dto.CreatedAt != "" {
		t.Errorf("zero CreatedAt should format empty, got %q", dto.CreatedAt)
	}
}

func TestMessagesPureHelpers(t *testing.T) {
	if firstNonEmptyMessageParam("", "  ", "x") != "x" {
		t.Error("should skip blanks")
	}
	if firstNonEmptyMessageParam("", "  ") != "" {
		t.Error("all blanks should be empty")
	}

	// toMessageDTO 零时间 + read 覆盖
	dto := toMessageDTO(models.Message{Status: "unread"}, true)
	if dto.Status != "read" || dto.CreatedAt != "" {
		t.Errorf("unexpected dto: %+v", dto)
	}

	db := newDB(t)
	msg := models.Message{Recipient: "u", Title: "t", Content: "c", Status: "unread"}
	if err := db.Create(&msg).Error; err != nil {
		t.Fatalf("create message: %v", err)
	}
	dto = toMessageDTO(msg, false)
	if dto.Status != "unread" || dto.CreatedAt == "" {
		t.Errorf("unexpected dto: %+v", dto)
	}
	msg.Status = " read "
	dto = toMessageDTO(msg, false)
	if dto.Status != "read" {
		t.Errorf("row-level read status should win: %+v", dto)
	}
}

func TestProfilePureHelpers(t *testing.T) {
	if !contains([]string{"a", " b ", "c"}, "B") {
		t.Error("contains should match case-insensitively with trim")
	}
	if contains([]string{"a"}, "z") {
		t.Error("no match expected")
	}
	if contains(nil, "a") {
		// nil 切片应返回 false
		t.Error("nil slice should not contain anything")
	}

	perm := codeToPermission("scripts:run")
	if perm["resource"] != "scripts" || perm["code"] != "scripts:run" {
		t.Errorf("unexpected permission: %v", perm)
	}
	actions, _ := perm["actions"].([]string)
	if len(actions) != 1 || actions[0] != "run" {
		t.Errorf("unexpected actions: %v", perm["actions"])
	}
	perm = codeToPermission("plain")
	if perm["resource"] != "plain" {
		t.Errorf("no-colon code should use itself as resource: %v", perm)
	}
	if fallbackProfileValue("  ", "default") != "default" {
		t.Error("blank value should fall back")
	}
	if fallbackProfileValue("val", "default") != "val" {
		t.Error("non-blank value should be kept")
	}
}
