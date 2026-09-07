package handlers

import (
	"net/http"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
)

// 覆盖各 handler 中 findByXxxParam 失败后的早退守卫分支。

func TestGuardBranchesInvalidTargets(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	uh := NewUserHandler(db)
	rh := NewRoleHandler(db)

	r := gin.New()
	r.POST("/users/:id/reset-password", asAdmin(1), uh.HandleResetPassword)
	r.DELETE("/users/:id", asAdmin(1), uh.HandleDelete)
	r.PUT("/users/:id", asAdmin(1), uh.HandleUpdate)
	r.DELETE("/roles/:code", asAdmin(1), rh.HandleDeleteRole)
	r.PUT("/roles/:code", asAdmin(1), rh.HandleUpdateRole)

	cases := []struct {
		method, path string
		body         map[string]any
		code         int
	}{
		{"POST", "/users/abc/reset-password", map[string]any{"newPassword": "Str0ng!pw"}, http.StatusBadRequest},
		{"POST", "/users/999/reset-password", map[string]any{"newPassword": "Str0ng!pw"}, http.StatusNotFound},
		{"DELETE", "/users/abc", nil, http.StatusBadRequest},
		{"DELETE", "/users/999", nil, http.StatusNotFound},
		{"PUT", "/users/abc", map[string]any{"active": true}, http.StatusBadRequest},
		{"PUT", "/users/999", map[string]any{"active": true}, http.StatusNotFound},
		{"DELETE", "/roles/ghost-role", nil, http.StatusNotFound},
		{"PUT", "/roles/ghost-role", map[string]any{"name": "n"}, http.StatusNotFound},
	}
	for _, tc := range cases {
		w := doJSON(r, tc.method, tc.path, tc.body)
		if w.Code != tc.code {
			t.Errorf("%s %s: expected %d, got %d %s", tc.method, tc.path, tc.code, w.Code, w.Body.String())
		}
	}
}

// loadReadSet 的 userID==0 早退分支（无身份上下文列出消息）。
func TestLoadReadSetZeroUserEarlyReturn(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	user := models.User{Username: "anon", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	// 广播消息对 username="" 可见
	if err := db.Create(&models.Message{Recipient: "*", Title: "bc", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}

	mh := NewMessageHandler(db)
	// userID 为 0 的匿名上下文：rows 非空 → loadReadSet 走 userID==0 早退
	anon := asUser(models.User{Username: "", Role: "viewer"})
	r := gin.New()
	r.GET("/messages", anon, mh.HandleList)
	w := doJSON(r, "GET", "/messages", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("anon list: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Total int64 `json:"total"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if resp.Total == 0 {
		t.Error("broadcast should be visible to anonymous username")
	}
}
