package handlers

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math/rand"
	"net/http"
	"net/http/httptest"
	"strconv"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/gin-gonic/gin"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// asAdmin 是一个测试中间件，直接注入 admin 身份，绕过 JWT。
func asAdmin(userID uint) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Set("user_id", userID)
		c.Set("username", "admin")
		c.Set("role", "admin")
		c.Next()
	}
}

func setupHandlerRouter(t *testing.T) (*gin.Engine, *gorm.DB, uint) {
	t.Helper()
	gin.SetMode(gin.TestMode)
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
	admin := models.User{Username: "admin", Password: "x", Role: "admin", Active: true}
	if err := db.Create(&admin).Error; err != nil {
		t.Fatalf("create admin: %v", err)
	}

	uh := NewUserHandler(db)
	rh := NewRoleHandler(db)

	r := gin.New()
	r.Use(func(c *gin.Context) { c.Set("db", db); c.Next() })
	adminGrp := r.Group("/api/admin").Use(asAdmin(admin.ID))
	{
		adminGrp.GET("/users", uh.HandleList)
		adminGrp.POST("/users", uh.HandleCreate)
		adminGrp.GET("/users/:id", uh.HandleGet)
		adminGrp.PUT("/users/:id", uh.HandleUpdate)
		adminGrp.DELETE("/users/:id", uh.HandleDelete)
		adminGrp.POST("/users/:id/reset-password", uh.HandleResetPassword)

		adminGrp.GET("/roles", rh.HandleListRoles)
		adminGrp.GET("/roles/:code", rh.HandleGetRole)
		adminGrp.POST("/roles", rh.HandleCreateRole)
		adminGrp.PUT("/roles/:code", rh.HandleUpdateRole)
		adminGrp.DELETE("/roles/:code", rh.HandleDeleteRole)
		adminGrp.GET("/permissions", rh.HandleListPermissions)
	}
	return r, db, admin.ID
}

func doJSON(r *gin.Engine, method, path string, body any) *httptest.ResponseRecorder {
	var buf bytes.Buffer
	if body != nil {
		_ = json.NewEncoder(&buf).Encode(body)
	}
	req := httptest.NewRequest(method, path, &buf)
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	return w
}

func TestUserCRUD(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)

	// Create
	w := doJSON(r, "POST", "/api/admin/users", map[string]any{
		"username": "alice", "password": "Str0ng!pw", "role": "operator",
	})
	if w.Code != http.StatusCreated {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	var created struct {
		User models.SafeUser `json:"user"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &created); err != nil {
		t.Fatal(err)
	}
	if created.User.Role != "operator" || !created.User.Active {
		t.Errorf("unexpected user: %+v", created.User)
	}
	uid := created.User.ID

	// List
	w = doJSON(r, "GET", "/api/admin/users?page=1&size=10", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d", w.Code)
	}
	var list listUsersResponse
	if err := json.Unmarshal(w.Body.Bytes(), &list); err != nil {
		t.Fatal(err)
	}
	if list.Total < 2 {
		t.Errorf("expected >=2 users, got %d", list.Total)
	}

	// Update role
	w = doJSON(r, "PUT", "/api/admin/users/"+itoa(uid), map[string]any{"role": "viewer"})
	if w.Code != http.StatusOK {
		t.Fatalf("update: %d %s", w.Code, w.Body.String())
	}
	var u models.User
	db.First(&u, uid)
	if u.Role != "viewer" {
		t.Errorf("role not updated: %s", u.Role)
	}

	// Reset password
	w = doJSON(r, "POST", "/api/admin/users/"+itoa(uid)+"/reset-password", map[string]any{"newPassword": "N3wPass!!"})
	if w.Code != http.StatusOK {
		t.Fatalf("reset: %d %s", w.Code, w.Body.String())
	}

	// Delete
	w = doJSON(r, "DELETE", "/api/admin/users/"+itoa(uid), nil)
	if w.Code != http.StatusOK {
		t.Fatalf("delete: %d %s", w.Code, w.Body.String())
	}
}

func TestUserCreateValidation(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)

	// 短用户名
	w := doJSON(r, "POST", "/api/admin/users", map[string]any{"username": "ab", "password": "Str0ng!pw"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("short username should be rejected: %d", w.Code)
	}
	// 弱密码
	w = doJSON(r, "POST", "/api/admin/users", map[string]any{"username": "bob", "password": "weak"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("weak password should be rejected: %d", w.Code)
	}
	// 未知角色
	w = doJSON(r, "POST", "/api/admin/users", map[string]any{"username": "bob", "password": "Str0ng!pw", "role": "ghost"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("unknown role should be rejected: %d", w.Code)
	}
}

func TestCannotDeleteSelfOrBuiltinAdmin(t *testing.T) {
	r, _, adminID := setupHandlerRouter(t)

	// 自删
	w := doJSON(r, "DELETE", "/api/admin/users/"+itoa(adminID), nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("self delete should be rejected: %d %s", w.Code, w.Body.String())
	}
}

func TestRoleCRUD(t *testing.T) {
	r, db, _ := setupHandlerRouter(t)

	// List 内置
	w := doJSON(r, "GET", "/api/admin/roles", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list roles: %d", w.Code)
	}
	var rl struct {
		Items []roleDTO `json:"items"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &rl); err != nil {
		t.Fatal(err)
	}
	if len(rl.Items) < 3 {
		t.Errorf("expected >=3 builtin roles, got %d", len(rl.Items))
	}

	// Create custom role
	w = doJSON(r, "POST", "/api/admin/roles", map[string]any{
		"code": "qa", "name": "QA", "permissions": []string{"scripts:view", "scripts:run"},
	})
	if w.Code != http.StatusCreated {
		t.Fatalf("create role: %d %s", w.Code, w.Body.String())
	}

	// 验证权限已写入
	var role models.Role
	db.Preload("Permissions").Where("code = ?", "qa").First(&role)
	if len(role.Permissions) != 2 {
		t.Errorf("expected 2 permissions, got %d", len(role.Permissions))
	}

	// Update permissions
	w = doJSON(r, "PUT", "/api/admin/roles/qa", map[string]any{"permissions": []string{"scripts:view"}})
	if w.Code != http.StatusOK {
		t.Fatalf("update role: %d %s", w.Code, w.Body.String())
	}
	db.Preload("Permissions").Where("code = ?", "qa").First(&role)
	if len(role.Permissions) != 1 {
		t.Errorf("expected 1 permission after update, got %d", len(role.Permissions))
	}

	// Delete custom role
	w = doJSON(r, "DELETE", "/api/admin/roles/qa", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("delete role: %d %s", w.Code, w.Body.String())
	}
}

func TestCannotDeleteBuiltinRole(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)
	w := doJSON(r, "DELETE", "/api/admin/roles/operator", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("deleting builtin role should be rejected: %d", w.Code)
	}
}

func TestCannotDeleteRoleInUse(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)
	// 先创建自定义角色并分配给用户
	doJSON(r, "POST", "/api/admin/roles", map[string]any{"code": "qa", "name": "QA"})
	doJSON(r, "POST", "/api/admin/users", map[string]any{"username": "carol", "password": "Str0ng!pw", "role": "qa"})

	w := doJSON(r, "DELETE", "/api/admin/roles/qa", nil)
	if w.Code != http.StatusConflict {
		t.Errorf("deleting role in use should be 409: %d %s", w.Code, w.Body.String())
	}
}

func TestPermissionsCatalog(t *testing.T) {
	r, _, _ := setupHandlerRouter(t)
	w := doJSON(r, "GET", "/api/admin/permissions", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list perms: %d", w.Code)
	}
	var resp struct {
		Items []models.Permission `json:"items"`
		Total int                 `json:"total"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp.Total == 0 {
		t.Error("permission catalog should not be empty")
	}
	// 按类别过滤
	w = doJSON(r, "GET", "/api/admin/permissions?category=script", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("filter: %d", w.Code)
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	for _, p := range resp.Items {
		if p.Category != "script" {
			t.Errorf("expected script category, got %s", p.Category)
		}
	}
}

func itoa(n uint) string {
	return strconv.FormatUint(uint64(n), 10)
}
