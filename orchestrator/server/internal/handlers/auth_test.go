package handlers

import (
	"encoding/json"
	"net/http"
	"testing"

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
