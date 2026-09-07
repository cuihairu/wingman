package middleware

import (
	"fmt"
	"math/rand"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
)

const testSecret = "0123456789abcdef0123456789abcdef"

func newTestDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:%s_%d?mode=memory&cache=shared", t.Name(), rand.Int())), &gorm.Config{})
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	if err := models.AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	if err := rbac.Seed(db); err != nil {
		t.Fatalf("seed: %v", err)
	}
	return db
}

func perform(mw gin.HandlerFunc, setup func(c *gin.Context)) *httptest.ResponseRecorder {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(mw)
	r.GET("/x", func(c *gin.Context) {
		if setup != nil {
			setup(c)
		}
		c.JSON(http.StatusOK, gin.H{"ok": true})
	})
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	return w
}

// ---------- jwtSecret / ParseBearerToken ----------

func TestJWTSecretRejectsLeadingTrailingWhitespace(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "  0123456789abcdef0123456789abcdef  ")
	if _, err := GenerateToken(1, "u", "admin"); err == nil {
		t.Fatal("expected error for secret with surrounding whitespace")
	}
}

func TestParseBearerToken(t *testing.T) {
	cases := []struct {
		in   string
		want string
	}{
		{"Bearer abc", "abc"},
		{"bearer abc", ""}, // 大小写敏感
		{"Basic abc", ""},
		{"Bearer", ""},
		{"Bearer a b", "a b"},
		{"", ""},
	}
	for _, tc := range cases {
		if got := ParseBearerToken(tc.in); got != tc.want {
			t.Errorf("ParseBearerToken(%q) = %q, want %q", tc.in, got, tc.want)
		}
	}
}

// ---------- AuthRequired / GetCurrentUser ----------

func TestAuthRequiredRejectsMissingOrMalformed(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)

	// 无 Authorization 头
	if w := perform(AuthRequired(), nil); w.Code != http.StatusUnauthorized {
		t.Errorf("missing header: got %d", w.Code)
	}
	// 非 Bearer 格式
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(AuthRequired())
	r.GET("/x", func(c *gin.Context) { c.JSON(200, nil) })
	for _, header := range []string{"Token abc", "Bearer"} {
		req := httptest.NewRequest("GET", "/x", nil)
		req.Header.Set("Authorization", header)
		w := httptest.NewRecorder()
		r.ServeHTTP(w, req)
		if w.Code != http.StatusUnauthorized {
			t.Errorf("header %q: got %d, want 401", header, w.Code)
		}
	}
	// 无效 token
	req := httptest.NewRequest("GET", "/x", nil)
	req.Header.Set("Authorization", "Bearer garbage")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Code != http.StatusUnauthorized {
		t.Errorf("garbage token: got %d, want 401", w.Code)
	}
}

func TestAuthRequiredPassesAndStoresClaims(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	token, err := GenerateToken(42, "alice", "operator")
	if err != nil {
		t.Fatal(err)
	}

	var gotID uint
	var gotName, gotRole string
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(AuthRequired())
	r.GET("/x", func(c *gin.Context) {
		gotID, gotName, gotRole = GetCurrentUser(c)
		c.JSON(http.StatusOK, nil)
	})
	req := httptest.NewRequest("GET", "/x", nil)
	req.Header.Set("Authorization", "Bearer "+token)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("got %d, want 200", w.Code)
	}
	if gotID != 42 || gotName != "alice" || gotRole != "operator" {
		t.Errorf("claims not propagated: %d %s %s", gotID, gotName, gotRole)
	}
}

func TestGetCurrentUserDefaultsWhenMissing(t *testing.T) {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Request = httptest.NewRequest("GET", "/x", nil)
	if id, name, role := GetCurrentUser(c); id != 0 || name != "" || role != "" {
		t.Errorf("expected zero values, got %d %s %s", id, name, role)
	}

	// 类型不匹配的上下文值也应回退为零值
	c.Set("user_id", "not-a-uint")
	c.Set("username", 42)
	c.Set("role", 1.5)
	if id, name, role := GetCurrentUser(c); id != 0 || name != "" || role != "" {
		t.Errorf("expected zero values for wrong types, got %d %s %s", id, name, role)
	}
}

// ---------- RoleRequired ----------

func TestRoleRequired(t *testing.T) {
	handler := func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) }

	// 无 role 上下文 → 403
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(RoleRequired("admin"))
	r.GET("/x", handler)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusForbidden {
		t.Errorf("no role: got %d, want 403", w.Code)
	}

	// role 为非字符串 → 403
	r2 := gin.New()
	r2.Use(func(c *gin.Context) { c.Set("role", 123) }, RoleRequired("admin"))
	r2.GET("/x", handler)
	w = httptest.NewRecorder()
	r2.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusForbidden {
		t.Errorf("non-string role: got %d, want 403", w.Code)
	}

	// 角色不匹配 → 403
	r3 := gin.New()
	r3.Use(func(c *gin.Context) { c.Set("role", "viewer") }, RoleRequired("admin"))
	r3.GET("/x", handler)
	w = httptest.NewRecorder()
	r3.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusForbidden {
		t.Errorf("wrong role: got %d, want 403", w.Code)
	}

	// 角色匹配 → 200
	r4 := gin.New()
	r4.Use(func(c *gin.Context) { c.Set("role", "operator") }, RoleRequired("admin", "operator"))
	r4.GET("/x", handler)
	w = httptest.NewRecorder()
	r4.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusOK {
		t.Errorf("matching role: got %d, want 200", w.Code)
	}
}

// ---------- PermissionRequired ----------

func TestPermissionRequiredVariants(t *testing.T) {
	db := newTestDB(t)
	viewer := models.User{Username: "pv", Password: "x", Role: "viewer", Active: true}
	operator := models.User{Username: "po", Password: "x", Role: "operator", Active: true}
	inactive := models.User{Username: "pi", Password: "x", Role: "operator", Active: false}
	db.Create(&viewer)
	db.Create(&operator)
	db.Create(&inactive)

	mk := func(userID uint, role string) *gin.Engine {
		gin.SetMode(gin.TestMode)
		r := gin.New()
		r.Use(func(c *gin.Context) {
			if userID != 0 {
				c.Set("user_id", userID)
			}
			if role != "" {
				c.Set("role", role)
			}
		}, PermissionRequired(db, "scripts:run"))
		r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })
		return r
	}

	get := func(r *gin.Engine) int {
		w := httptest.NewRecorder()
		r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
		return w.Code
	}

	// admin 角色（含大小写与空白）→ 放行
	if code := get(mk(1, "admin")); code != http.StatusOK {
		t.Errorf("admin: got %d", code)
	}
	if code := get(mk(1, "  ADMIN  ")); code != http.StatusOK {
		t.Errorf("ADMIN (case/space): got %d", code)
	}
	// 无 user_id → 403
	if code := get(mk(0, "viewer")); code != http.StatusForbidden {
		t.Errorf("no user id: got %d", code)
	}
	// operator 有 scripts:run → 200
	if code := get(mk(operator.ID, "operator")); code != http.StatusOK {
		t.Errorf("operator: got %d", code)
	}
	// viewer 无 scripts:run → 403
	if code := get(mk(viewer.ID, "viewer")); code != http.StatusForbidden {
		t.Errorf("viewer: got %d", code)
	}
	// 未激活用户 → 403
	if code := get(mk(inactive.ID, "operator")); code != http.StatusForbidden {
		t.Errorf("inactive: got %d", code)
	}
}

func TestPermissionRequiredEmptyRequirements(t *testing.T) {
	db := newTestDB(t)
	w := perform(PermissionRequired(db), nil)
	if w.Code != http.StatusOK {
		t.Errorf("empty required should pass: got %d", w.Code)
	}
}

func TestPermissionRequiredUsesCachedPermissions(t *testing.T) {
	db := newTestDB(t)
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", uint(99))
		c.Set("role", "viewer")
		// 预置请求级缓存：包含通配权限
		c.Set("permissions", []string{"*"})
	}, PermissionRequired(db, "anything:weird"))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusOK {
		t.Errorf("cached wildcard permission should pass: got %d", w.Code)
	}
}

func TestPermissionRequiredDBError(t *testing.T) {
	db := newTestDB(t)
	sqlDB, _ := db.DB()
	if err := sqlDB.Close(); err != nil {
		t.Fatalf("close db: %v", err)
	}

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", uint(1))
		c.Set("role", "viewer")
	}, PermissionRequired(db, "scripts:run"))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusInternalServerError {
		t.Errorf("db error: got %d, want 500", w.Code)
	}
}

func TestResolvePermissionCodesAdminRoleInDB(t *testing.T) {
	db := newTestDB(t)
	admin := models.User{Username: "adm", Password: "x", Role: "admin", Active: true}
	db.Create(&admin)

	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(httptest.NewRecorder())
	c.Request = httptest.NewRequest("GET", "/x", nil)
	c.Set("user_id", admin.ID)
	c.Set("role", "user") // claims 角色不是 admin，但 DB 角色是 → 通配

	codes, err := resolvePermissionCodes(c, db, admin.ID)
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	if len(codes) != 1 || codes[0] != "*" {
		t.Errorf("expected wildcard, got %v", codes)
	}

	// 数据库中角色不存在（Role.Code 无匹配）→ 空 codes
	ghost := models.User{Username: "ghost", Password: "x", Role: "no-such-role", Active: true}
	db.Create(&ghost)
	c2, _ := gin.CreateTestContext(httptest.NewRecorder())
	c2.Request = httptest.NewRequest("GET", "/x", nil)
	codes, err = resolvePermissionCodes(c2, db, ghost.ID)
	if err != nil || len(codes) != 0 {
		t.Errorf("unknown role should resolve empty, got %v err=%v", codes, err)
	}
}

func TestValidateTokenRejectsWrongSigningMethod(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", testSecret)
	claims := Claims{
		UserID:   1,
		Username: "u",
		Role:     "admin",
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS512, claims)
	signed, err := token.SignedString([]byte(testSecret))
	if err != nil {
		t.Fatal(err)
	}
	if _, err := ValidateTokenString(signed); err == nil {
		t.Fatal("HS512-signed token should be rejected")
	}
}

func TestPermissionRequiredBlankRequirementIsSkipped(t *testing.T) {
	db := newTestDB(t)
	viewer := models.User{Username: "blankreq", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&viewer).Error; err != nil {
		t.Fatal(err)
	}

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", viewer.ID)
		c.Set("role", "viewer")
	}, PermissionRequired(db, "   "))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusForbidden {
		t.Errorf("blank-only requirements should not match: got %d", w.Code)
	}
}

func TestPermissionRequiredPluckError(t *testing.T) {
	db := newTestDB(t)
	viewer := models.User{Username: "pluck", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&viewer).Error; err != nil {
		t.Fatal(err)
	}
	// 删除 permissions 表，使权限码查询失败
	if err := db.Migrator().DropTable("permissions"); err != nil {
		t.Fatalf("drop table: %v", err)
	}

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", viewer.ID)
		c.Set("role", "viewer")
	}, PermissionRequired(db, "scripts:run"))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusInternalServerError {
		t.Errorf("pluck failure: got %d, want 500", w.Code)
	}
}

func TestPermissionRequiredRoleQueryError(t *testing.T) {
	db := newTestDB(t)
	viewer := models.User{Username: "roleq", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&viewer).Error; err != nil {
		t.Fatal(err)
	}
	// 删除 roles 表，使角色查询失败
	if err := db.Migrator().DropTable("roles"); err != nil {
		t.Fatalf("drop table: %v", err)
	}

	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(func(c *gin.Context) {
		c.Set("user_id", viewer.ID)
		c.Set("role", "viewer")
	}, PermissionRequired(db, "scripts:run"))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusInternalServerError {
		t.Errorf("role query failure: got %d, want 500", w.Code)
	}
}

// ---------- CORS ----------

func TestCORSDefaultsAndEnv(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// 默认来源
	t.Setenv("WINGMAN_CORS_ORIGINS", "")
	r := gin.New()
	r.Use(CORS())
	r.OPTIONS("/x", func(c *gin.Context) {})
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, nil) })

	req := httptest.NewRequest("OPTIONS", "/x", nil)
	req.Header.Set("Origin", "http://localhost:8000")
	req.Header.Set("Access-Control-Request-Method", "GET")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Header().Get("Access-Control-Allow-Origin") != "http://localhost:8000" {
		t.Errorf("default origin not allowed: %v", w.Header())
	}

	// 非允许来源 → 无 CORS 头（预检被拒）
	req = httptest.NewRequest("OPTIONS", "/x", nil)
	req.Header.Set("Origin", "http://evil.example")
	req.Header.Set("Access-Control-Request-Method", "GET")
	w = httptest.NewRecorder()
	r.ServeHTTP(w, req)
	if w.Header().Get("Access-Control-Allow-Origin") != "" {
		t.Errorf("disallowed origin must not get CORS headers: %v", w.Header())
	}

	// 环境变量来源
	t.Setenv("WINGMAN_CORS_ORIGINS", "https://custom.example")
	r2 := gin.New()
	r2.Use(CORS())
	r2.OPTIONS("/x", func(c *gin.Context) {})
	req = httptest.NewRequest("OPTIONS", "/x", nil)
	req.Header.Set("Origin", "https://custom.example")
	req.Header.Set("Access-Control-Request-Method", "GET")
	w = httptest.NewRecorder()
	r2.ServeHTTP(w, req)
	if w.Header().Get("Access-Control-Allow-Origin") != "https://custom.example" {
		t.Errorf("env origin not allowed: %v", w.Header())
	}
}

func TestSplitOrigins(t *testing.T) {
	if got := splitOrigins(""); len(got) != 0 {
		t.Errorf("empty should yield empty, got %v", got)
	}
	got := splitOrigins(" a , b ,,c ")
	if len(got) != 3 || got[0] != "a" || got[1] != "b" || got[2] != "c" {
		t.Errorf("unexpected: %v", got)
	}
}

// ---------- RateLimiter ----------

func TestRateLimiterCheckBlocksAfterMaxAttempts(t *testing.T) {
	rl := &RateLimiter{clients: make(map[string]*clientInfo)}

	for i := 0; i < maxAttempts; i++ {
		if !rl.Check("c1") {
			t.Fatalf("attempt %d should be allowed", i+1)
		}
	}
	if rl.Check("c1") {
		t.Error("6th attempt should be blocked")
	}
	if rl.Check("c1") {
		t.Error("still-blocked client should remain blocked")
	}
	// 其他 client 不受影响
	if !rl.Check("c2") {
		t.Error("other client should be allowed")
	}
}

func TestRateLimiterWindowReset(t *testing.T) {
	rl := &RateLimiter{clients: make(map[string]*clientInfo)}
	rl.Check("c1")

	// 手动把窗口回拨，模拟窗口过期
	rl.mu.Lock()
	rl.clients["c1"].lastReset = time.Now().Add(-windowDuration - time.Second)
	rl.mu.Unlock()

	if !rl.Check("c1") {
		t.Error("expired window should reset counter")
	}
	rl.mu.Lock()
	count := rl.clients["c1"].attempts
	rl.mu.Unlock()
	if count != 1 {
		t.Errorf("expected attempts reset to 1, got %d", count)
	}
}

func TestRateLimiterRecordSuccess(t *testing.T) {
	rl := &RateLimiter{clients: make(map[string]*clientInfo)}
	rl.Check("c1")
	rl.RecordSuccess("c1")

	rl.mu.Lock()
	attempts := rl.clients["c1"].attempts
	rl.mu.Unlock()
	if attempts != 0 {
		t.Errorf("expected attempts reset to 0, got %d", attempts)
	}
	// 未知 client 不应 panic
	rl.RecordSuccess("ghost")
}

func TestRateLimitMiddleware(t *testing.T) {
	rl := &RateLimiter{clients: make(map[string]*clientInfo)}
	gin.SetMode(gin.TestMode)
	r := gin.New()
	r.Use(RateLimitMiddleware(rl))
	r.GET("/x", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"ok": true}) })

	// 消耗完配额
	for i := 0; i < maxAttempts; i++ {
		w := httptest.NewRecorder()
		r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
		if w.Code != http.StatusOK {
			t.Fatalf("attempt %d: got %d", i+1, w.Code)
		}
	}
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest("GET", "/x", nil))
	if w.Code != http.StatusTooManyRequests {
		t.Errorf("blocked: got %d, want 429", w.Code)
	}
}

func TestGetRateLimiterReturnsSingleton(t *testing.T) {
	if GetRateLimiter() != globalRateLimiter {
		t.Error("GetRateLimiter should return the global instance")
	}
	if GetRateLimiter() == nil {
		t.Error("global rate limiter should be initialized")
	}
}

func TestCleanupRemovesStaleClients(t *testing.T) {
	orig := cleanupInterval
	cleanupInterval = 10 * time.Millisecond
	defer func() { cleanupInterval = orig }()

	rl := NewRateLimiter()

	// stale：超过 1 小时未活动且不在封锁期
	rl.mu.Lock()
	rl.clients["stale"] = &clientInfo{
		attempts:  1,
		lastReset: time.Now().Add(-2 * time.Hour),
		blockUntil: time.Now().Add(-time.Hour),
	}
	// blocked：未到封锁截止期，不应清理
	rl.clients["blocked"] = &clientInfo{
		attempts:  6,
		lastReset: time.Now().Add(-2 * time.Hour),
		blockUntil: time.Now().Add(time.Hour),
	}
	// fresh：最近活动，不应清理
	rl.clients["fresh"] = &clientInfo{lastReset: time.Now()}
	rl.mu.Unlock()

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		rl.mu.Lock()
		_, hasStale := rl.clients["stale"]
		rl.mu.Unlock()
		if !hasStale {
			break
		}
		time.Sleep(5 * time.Millisecond)
	}

	rl.mu.Lock()
	defer rl.mu.Unlock()
	if _, ok := rl.clients["stale"]; ok {
		t.Error("stale client should have been cleaned up")
	}
	if _, ok := rl.clients["blocked"]; !ok {
		t.Error("blocked client should be retained")
	}
	if _, ok := rl.clients["fresh"]; !ok {
		t.Error("fresh client should be retained")
	}
}
