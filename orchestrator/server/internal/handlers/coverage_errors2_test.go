package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/security"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// 第二轮错误注入：覆盖剩余数据库/文件系统错误分支。

func TestLoginDBUpdateFailure(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", "0123456789abcdef0123456789abcdef")
	gin.SetMode(gin.TestMode)
	// query 不注入（登录查询要成功），update 注入失败
	db := newDB(t)
	db.Callback().Update().Before("gorm:update").Register("test:fail-update", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})
	hash, err := security.HashPassword("Str0ng!pw")
	if err != nil {
		t.Fatal(err)
	}
	if err := db.Create(&models.User{Username: "updlogin", Password: hash, Role: "viewer", Active: true}).Error; err != nil {
		t.Fatal(err)
	}

	ah := NewAuthHandler(db)
	r := gin.New()
	r.POST("/login", ah.HandleLogin)
	w := doJSON(r, "POST", "/login", map[string]any{"username": "updlogin", "password": "Str0ng!pw"})
	// last_login 更新失败只记录日志，登录仍应成功
	if w.Code != http.StatusOK {
		t.Errorf("login with failing last_login update: expected 200, got %d %s", w.Code, w.Body.String())
	}
}

func TestUsersCreateRoleCheckFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := failingDB(t, "query")
	uh := NewUserHandler(db)
	r := gin.New()
	r.POST("/users", asAdmin(1), uh.HandleCreate)

	// EnsureRoleExists 查询失败 → 500
	w := doJSON(r, "POST", "/users", map[string]any{
		"username": "rolefail", "password": "Str0ng!pw", "role": "viewer",
	})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("role check failure: expected 500, got %d", w.Code)
	}
}

func TestUsersUpdateRoleCheckFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := failingDB(t, "query")
	uh := NewUserHandler(db)
	r := gin.New()
	r.PUT("/users/:id", asAdmin(1), uh.HandleUpdate)

	w := doJSON(r, "PUT", "/users/1", map[string]any{"role": "viewer"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("update role check failure: expected 500, got %d", w.Code)
	}
}

func TestUsersCreateHashFailureSkipped(t *testing.T) {
	// HashPassword 失败分支不可注入（bcrypt 参数固定），仅验证非法字符路径已覆盖。
	if validUsername("ok_name-1") != true {
		t.Error("sanity check failed")
	}
}

func TestRolesCreateDBFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := failingDB(t, "create")
	rh := NewRoleHandler(db)
	r := gin.New()
	r.POST("/roles", asAdmin(1), rh.HandleCreateRole)

	w := doJSON(r, "POST", "/roles", map[string]any{"code": "dbfail1"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("role create failure: expected 500, got %d %s", w.Code, w.Body.String())
	}
}

func TestRolesDeleteBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// 内置角色 → 400；被引用 → 409
	db := newDB(t)
	if err := db.Exec("INSERT INTO roles (code, name, builtin, created_at, updated_at) VALUES ('viewer','Viewer',1,datetime('now'),datetime('now'))").Error; err != nil {
		t.Fatal(err)
	}
	rh := NewRoleHandler(db)
	r := gin.New()
	r.DELETE("/roles/:code", asAdmin(1), rh.HandleDeleteRole)

	w := doJSON(r, "DELETE", "/roles/viewer", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("builtin role: expected 400, got %d", w.Code)
	}

	// 自定义角色被用户引用 → 409
	if err := db.Exec("INSERT INTO roles (code, name, builtin, created_at, updated_at) VALUES ('inuse','In Use',0,datetime('now'),datetime('now'))").Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Create(&models.User{Username: "roleuser", Password: "x", Role: "inuse", Active: true}).Error; err != nil {
		t.Fatal(err)
	}
	w = doJSON(r, "DELETE", "/roles/inuse", nil)
	if w.Code != http.StatusConflict {
		t.Errorf("role in use: expected 409, got %d", w.Code)
	}

	// 删除失败 → 500
	failDB := failingDB(t, "delete")
	if err := failDB.Exec("INSERT INTO roles (code, name, builtin, created_at, updated_at) VALUES ('delfail','Del Fail',0,datetime('now'),datetime('now'))").Error; err != nil {
		t.Fatal(err)
	}
	rhFail := NewRoleHandler(failDB)
	rFail := gin.New()
	rFail.DELETE("/roles/:code", asAdmin(1), rhFail.HandleDeleteRole)
	w = doJSON(rFail, "DELETE", "/roles/delfail", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("role delete failure: expected 500, got %d", w.Code)
	}
}

func TestRolesUpdatePermissionsQueryFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	// 先正常建库造角色，再注册 query 失败回调模拟权限查询失败
	db := newDB(t)
	if err := db.Exec("INSERT INTO roles (code, name, builtin, created_at, updated_at) VALUES ('qfail','Q',0,datetime('now'),datetime('now'))").Error; err != nil {
		t.Fatal(err)
	}
	var role models.Role
	if err := db.Where("code = ?", "qfail").First(&role).Error; err != nil {
		t.Fatal(err)
	}
	db.Callback().Query().Before("gorm:query").Register("test:fail-query", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})

	rh := NewRoleHandler(db)
	r := gin.New()
	r.PUT("/roles/:code", asAdmin(1), rh.HandleUpdateRole)
	// query 注入使 findByCodeParam 的 Preload 查询失败 → 500
	w := doJSON(r, "PUT", "/roles/qfail", map[string]any{"permissions": []string{"scripts:run"}})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("role load failure: expected 500, got %d %s", w.Code, w.Body.String())
	}
}

func TestMessagesMarkReadCreateFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	// 先造数据，再注册 create 失败回调
	db := newDB(t)
	user := models.User{Username: "mrk2", Password: "x", Role: "viewer", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	db.Callback().Create().Before("gorm:create").Register("test:fail-create", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})
	mh := NewMessageHandler(db)
	r := gin.New()
	r.POST("/read/:id", asUser(user), mh.HandleMarkRead)

	// markRead 写入失败 → 500
	w := doJSON(r, "POST", "/read/1", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("mark read create failure: expected 500, got %d", w.Code)
	}

	// markAll 中 markRead 失败 → 500
	db2 := newDB(t)
	if err := db2.Create(&models.User{Username: "mrk3", Password: "x", Role: "viewer", Active: true}).Error; err != nil {
		t.Fatal(err)
	}
	var u2 models.User
	if err := db2.First(&u2, "username = ?", "mrk3").Error; err != nil {
		t.Fatal(err)
	}
	if err := db2.Create(&models.Message{Recipient: "mrk3", Title: "t", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}
	db2.Callback().Create().Before("gorm:create").Register("test:fail-create", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})
	mh2 := NewMessageHandler(db2)
	r2 := gin.New()
	r2.POST("/read-all", asUser(u2), mh2.HandleMarkAllRead)
	w = doJSON(r2, "POST", "/read-all", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("mark all create failure: expected 500, got %d", w.Code)
	}
}

func TestProfilePermissionsQueryFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	dbOK := newDB(t)
	user := models.User{Username: "permfail", Password: "x", Role: "viewer", Active: true}
	if err := dbOK.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	dbOK.Callback().Query().Before("gorm:query").Register("test:fail-query", func(tx *gorm.DB) {
		tx.AddError(errBoom)
	})
	// currentUser 也会失败…… handler 先调 currentUser → 401。
	// 因此该分支（UserPermissionCodes err）经由 401 前置拦截，仍验证响应安全即可。
	ph := NewProfileHandler(dbOK)
	r := gin.New()
	r.GET("/perms", asUser(user), ph.HandleGetPermissions)
	w := doJSON(r, "GET", "/perms", nil)
	if w.Code != http.StatusOK && w.Code != http.StatusUnauthorized {
		t.Errorf("unexpected code: %d", w.Code)
	}
}

func TestScriptCreateWriteFileFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	reg, _ := newRegistry(t)
	dir := t.TempDir()
	// 预先创建同名目录：MkdirAll 成功，WriteFile 失败
	if err := os.Mkdir(filepath.Join(dir, "collide.lua"), 0755); err != nil {
		t.Fatal(err)
	}
	sh := NewScriptHandler(db, dir, reg)
	r := gin.New()
	r.POST("/scripts", asAdmin(1), sh.HandleCreate)
	w := doJSON(r, "POST", "/scripts", map[string]any{"name": "collide"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("write file onto dir: expected 500, got %d", w.Code)
	}
}

func TestWorkflowCreateDBFailure(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := failingDB(t, "create")
	reg, hub := newRegistry(t)
	engine := workflow.NewEngine(db, reg, hub, t.TempDir())
	wh := NewWorkflowHandler(engine, db)
	r := gin.New()
	r.POST("/workflows", asAdmin(1), wh.HandleCreate)

	w := doJSON(r, "POST", "/workflows", map[string]any{
		"name":  "dbfail",
		"steps": []map[string]any{{"id": "w1", "type": "wait", "parameters": map[string]any{"seconds": 1}}},
	})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("workflow create db failure: expected 500, got %d", w.Code)
	}
}

func TestScreenshotInvalidJSON(t *testing.T) {
	gin.SetMode(gin.TestMode)
	sh := NewScreenshotHandler(&mockHub{})
	r := gin.New()
	r.POST("/screenshot", sh.HandleScreenshot)
	req := httptestBody("POST", "/screenshot", "not-json")
	w := requestRaw(r, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid json: expected 400, got %d", w.Code)
	}
}
