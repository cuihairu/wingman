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
	"gorm.io/gorm"
)

func setupAuthRouter(t *testing.T, user models.User) (*gin.Engine, *gorm.DB, uint) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatalf("hash password: %v", err)
	}
	user.Password = hash
	if user.Role == "" {
		user.Role = "viewer"
	}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}
	handler := NewAuthHandler(db)
	r := gin.New()
	r.POST("/login", handler.HandleLogin)
	return r, db, user.ID
}

func TestLoginUpdatesLastLoginAt(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	r, db, userID := setupAuthRouter(t, models.User{Username: "alice", Active: true})

	w := doJSON(r, "POST", "/login", map[string]any{
		"username": "alice",
		"password": "Str0ng!pw",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("login: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Token string `json:"token"`
		User  struct {
			Active bool `json:"active"`
		} `json:"user"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode login response: %v", err)
	}
	if resp.Token == "" || !resp.User.Active {
		t.Fatalf("unexpected login response: %+v", resp)
	}

	var stored models.User
	if err := db.First(&stored, userID).Error; err != nil {
		t.Fatalf("reload user: %v", err)
	}
	if stored.LastLoginAt == nil || stored.LastLoginAt.IsZero() {
		t.Fatal("lastLoginAt was not persisted")
	}
}

// 登录成功必须复位限流中间件的纯 IP 计数（中间件是封禁的唯一裁决者）。
// 回归锁定：早先成功路径传 ip:username 组合键，封禁计数永不清零——
// 4 次失败 + 1 次成功后，第 5 次失败请求会被直接 429。
func TestLoginSuccessResetsRateLimitCounter(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatalf("hash password: %v", err)
	}
	if err := db.Create(&models.User{Username: "alice", Password: hash, Role: "viewer", Active: true}).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}
	r := gin.New()
	r.POST("/login",
		middleware.RateLimitMiddleware(middleware.GetRateLimiter()),
		NewAuthHandler(db).HandleLogin)

	// 固定客户端地址，隔离全局限流器的其他测试条目
	const remoteAddr = "10.77.31.9:40123"
	login := func(password string) int {
		var buf bytes.Buffer
		_ = json.NewEncoder(&buf).Encode(map[string]any{"username": "alice", "password": password})
		req := httptest.NewRequest("POST", "/login", &buf)
		req.Header.Set("Content-Type", "application/json")
		req.RemoteAddr = remoteAddr
		w := httptest.NewRecorder()
		r.ServeHTTP(w, req)
		return w.Code
	}

	for i := 0; i < 4; i++ {
		if code := login("wrong-password"); code != http.StatusUnauthorized {
			t.Fatalf("failure %d: got %d, want 401", i+1, code)
		}
	}
	if code := login("Str0ng!pw"); code != http.StatusOK {
		t.Fatalf("successful login: got %d, want 200", code)
	}
	// 成功后计数清零：接下来 5 次失败仍应拿到 401 而非 429
	for i := 0; i < 5; i++ {
		if code := login("wrong-password"); code != http.StatusUnauthorized {
			t.Fatalf("post-success failure %d: got %d, want 401 (success must reset the counter)", i+1, code)
		}
	}
	// 第 6 次失败越过 maxAttempts，封禁生效
	if code := login("wrong-password"); code != http.StatusTooManyRequests {
		t.Fatalf("overflow failure: got %d, want 429", code)
	}
}

func TestLoginRejectsInactiveUser(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	r, _, _ := setupAuthRouter(t, models.User{Username: "disabled", Active: false})

	w := doJSON(r, "POST", "/login", map[string]any{
		"username": "disabled",
		"password": "Str0ng!pw",
	})
	if w.Code != http.StatusForbidden {
		t.Fatalf("inactive login: expected 403, got %d %s", w.Code, w.Body.String())
	}
}
