package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
)

func asUser(user models.User) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Set("user_id", user.ID)
		c.Set("username", user.Username)
		c.Set("role", user.Role)
		c.Next()
	}
}

func TestProfileUpdatePersists(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "alice", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}

	handler := NewProfileHandler(db)
	r := gin.New()
	r.GET("/profile", asUser(user), handler.HandleGetProfile)
	r.PUT("/profile", asUser(user), handler.HandleUpdateProfile)

	w := doJSON(r, "PUT", "/profile", map[string]any{
		"nickname": "Alice A",
		"email":    "alice@example.com",
		"phone":    "13800138000",
		"avatar":   "https://example.com/a.png",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("update profile: %d %s", w.Code, w.Body.String())
	}

	var stored models.User
	if err := db.First(&stored, user.ID).Error; err != nil {
		t.Fatalf("reload user: %v", err)
	}
	if stored.Nickname != "Alice A" || stored.Email != "alice@example.com" || stored.Phone != "13800138000" || stored.Avatar != "https://example.com/a.png" {
		t.Fatalf("profile fields not persisted: %+v", stored)
	}

	w = doJSON(r, "PUT", "/profile", map[string]any{
		"avatar": "https://example.com/b.png",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("partial update profile: %d %s", w.Code, w.Body.String())
	}
	if err := db.First(&stored, user.ID).Error; err != nil {
		t.Fatalf("reload partial user: %v", err)
	}
	if stored.Email != "alice@example.com" || stored.Avatar != "https://example.com/b.png" {
		t.Fatalf("partial update overwrote unrelated fields: %+v", stored)
	}

	recorder := httptest.NewRecorder()
	r.ServeHTTP(recorder, httptest.NewRequest("GET", "/profile", nil))
	if recorder.Code != http.StatusOK {
		t.Fatalf("get profile: %d %s", recorder.Code, recorder.Body.String())
	}
	var resp struct {
		Nickname    string   `json:"nickname"`
		DisplayName string   `json:"displayName"`
		Email       string   `json:"email"`
		Phone       string   `json:"phone"`
		Avatar      string   `json:"avatar"`
		Roles       []string `json:"roles"`
		Active      bool     `json:"active"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode profile: %v", err)
	}
	if resp.DisplayName != "Alice A" || resp.Nickname != "Alice A" || resp.Email != "alice@example.com" || resp.Phone != "13800138000" || resp.Avatar != "https://example.com/b.png" {
		t.Fatalf("unexpected profile response: %+v", resp)
	}
	if !resp.Active || len(resp.Roles) != 1 || resp.Roles[0] != "viewer" {
		t.Fatalf("unexpected role/status response: %+v", resp)
	}
}
