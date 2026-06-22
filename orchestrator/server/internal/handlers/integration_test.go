package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/gin-gonic/gin"
)

// TestIntegrationAuthAndPermissionFlow 端到端集成测试：
// 覆盖 unit 测试（asUser）绕过的真实中间件链——
// JWT 签发 → AuthRequired 校验 → PermissionRequired 鉴权 → handler。
func TestIntegrationAuthAndPermissionFlow(t *testing.T) {
	gin.SetMode(gin.TestMode)
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	db := newDB(t)

	// 创建真实用户（bcrypt 哈希密码）
	adminPw, _ := security.HashPassword("admin123")
	viewerPw, _ := security.HashPassword("viewer123")
	admin := models.User{Username: "itadmin", Password: adminPw, Role: "admin", Active: true}
	viewer := models.User{Username: "itviewer", Password: viewerPw, Role: "viewer", Active: true}
	if err := db.Create(&admin).Error; err != nil {
		t.Fatalf("create admin: %v", err)
	}
	if err := db.Create(&viewer).Error; err != nil {
		t.Fatalf("create viewer: %v", err)
	}

	// 装配真实路由（与 main.go 同构的子集）
	authHandler := NewAuthHandler(db)
	profileHandler := NewProfileHandler(db)
	userHandler := NewUserHandler(db)
	r := gin.New()
	r.POST("/api/v1/auth/login", middleware.RateLimitMiddleware(middleware.GetRateLimiter()), authHandler.HandleLogin)

	v1 := r.Group("/api/v1", middleware.AuthRequired())
	{
		v1.GET("/profile", profileHandler.HandleGetProfile)
	}
	adm := r.Group("/api/admin", middleware.AuthRequired(), middleware.PermissionRequired(db, "users:manage"))
	{
		adm.GET("/users", userHandler.HandleList)
	}

	login := func(username, password string) string {
		body, _ := json.Marshal(map[string]string{"username": username, "password": password})
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("POST", "/api/v1/auth/login", bytes.NewReader(body))
		req.Header.Set("Content-Type", "application/json")
		r.ServeHTTP(w, req)
		if w.Code != http.StatusOK {
			t.Fatalf("login %s: got %d %s", username, w.Code, w.Body.String())
		}
		var resp map[string]any
		json.Unmarshal(w.Body.Bytes(), &resp)
		tok, _ := resp["token"].(string)
		if tok == "" {
			t.Fatalf("login %s: no token in response", username)
		}
		return tok
	}

	get := func(path, token string) *httptest.ResponseRecorder {
		w := httptest.NewRecorder()
		req, _ := http.NewRequest("GET", path, nil)
		if token != "" {
			req.Header.Set("Authorization", "Bearer "+token)
		}
		r.ServeHTTP(w, req)
		return w
	}

	adminToken := login("itadmin", "admin123")
	viewerToken := login("itviewer", "viewer123")

	// 1. admin 带 token 访问 profile → 200
	if w := get("/api/v1/profile", adminToken); w.Code != http.StatusOK {
		t.Errorf("admin profile: got %d, want 200 (%s)", w.Code, w.Body.String())
	}

	// 2. viewer 带 token 访问 profile → 200（任何已认证用户）
	if w := get("/api/v1/profile", viewerToken); w.Code != http.StatusOK {
		t.Errorf("viewer profile: got %d, want 200", w.Code)
	}

	// 3. 无 token 访问 profile → 401
	if w := get("/api/v1/profile", ""); w.Code != http.StatusUnauthorized {
		t.Errorf("no token profile: got %d, want 401", w.Code)
	}

	// 4. 畸形 token → 401
	if w := get("/api/v1/profile", "garbage.token.here"); w.Code != http.StatusUnauthorized {
		t.Errorf("bad token profile: got %d, want 401", w.Code)
	}

	// 5. 错误格式 Authorization（无 Bearer 前缀）→ 401
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("GET", "/api/v1/profile", nil)
	req.Header.Set("Authorization", adminToken)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusUnauthorized {
		t.Errorf("raw token (no Bearer): got %d, want 401", w.Code)
	}

	// 6. admin 访问 /admin/users（PermissionRequired admin 旁路）→ 200
	if w := get("/api/admin/users", adminToken); w.Code != http.StatusOK {
		t.Errorf("admin users: got %d, want 200 (%s)", w.Code, w.Body.String())
	}

	// 7. viewer 访问 /admin/users（无 users:manage 权限）→ 403
	if w := get("/api/admin/users", viewerToken); w.Code != http.StatusForbidden {
		t.Errorf("viewer users: got %d, want 403", w.Code)
	}

	// 8. viewer 的 profile 响应携带正确用户名
	w = get("/api/v1/profile", viewerToken)
	var prof map[string]any
	json.Unmarshal(w.Body.Bytes(), &prof)
	if username, _ := prof["username"].(string); username != "itviewer" {
		t.Errorf("profile username: got %q, want itviewer", username)
	}
}
