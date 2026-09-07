package handlers

import (
	"net/http"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// failTable 注册仅针对指定表的查询失败回调，
// 用于区分同一 handler 内不同表的查询错误分支。
func failTable(db *gorm.DB, table string) {
	db.Callback().Query().Before("gorm:query").Register("test:fail-"+table, func(tx *gorm.DB) {
		if tx.Statement.Table == table {
			tx.AddError(errBoom)
		}
	})
}

func TestTableScopedQueryFailures(t *testing.T) {
	gin.SetMode(gin.TestMode)

	// 1) HandleUpdate：用户查询成功，roles 校验查询失败 → 500
	db1 := newDB(t)
	user := models.User{Username: "tupd", Password: "x", Role: "viewer", Active: true}
	if err := db1.Create(&user).Error; err != nil {
		t.Fatal(err)
	}
	failTable(db1, "roles")
	uh := NewUserHandler(db1)
	r1 := gin.New()
	r1.PUT("/users/:id", asAdmin(1), uh.HandleUpdate)
	w := doJSON(r1, "PUT", "/users/1", map[string]any{"role": "viewer"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("update role validation failure: expected 500, got %d %s", w.Code, w.Body.String())
	}

	// 2) HandleGetPermissions：currentUser 成功，UserPermissionCodes 查 roles 失败 → 空权限兜底
	db2 := newDB(t)
	viewer2 := models.User{Username: "permq", Password: "x", Role: "viewer", Active: true}
	if err := db2.Create(&viewer2).Error; err != nil {
		t.Fatal(err)
	}
	failTable(db2, "roles")
	ph := NewProfileHandler(db2)
	r2 := gin.New()
	r2.GET("/perms", asUser(viewer2), ph.HandleGetPermissions)
	w = doJSON(r2, "GET", "/perms", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("permissions fallback: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Permissions   []gin.H  `json:"permissions"`
		PermissionIDs []string `json:"permissionIDs"`
		Admin         bool     `json:"admin"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if len(resp.Permissions) != 0 {
		t.Errorf("failed permission query should yield empty list, got %v", resp.Permissions)
	}
	if resp.Admin {
		t.Error("viewer should not be admin")
	}

	// 3) HandleList：messages 查询成功，message_reads 查询失败 → 500
	db3 := newDB(t)
	muser := models.User{Username: "reads", Password: "x", Role: "viewer", Active: true}
	if err := db3.Create(&muser).Error; err != nil {
		t.Fatal(err)
	}
	if err := db3.Create(&models.Message{Recipient: "reads", Title: "t", Content: "c", Status: "unread"}).Error; err != nil {
		t.Fatal(err)
	}
	failTable(db3, "message_reads")
	mh := NewMessageHandler(db3)
	r3 := gin.New()
	r3.GET("/messages", asUser(muser), mh.HandleList)
	w = doJSON(r3, "GET", "/messages", nil)
	if w.Code != http.StatusInternalServerError {
		t.Errorf("load read set failure: expected 500, got %d", w.Code)
	}

	// 4) rbac.UserPermissionCodes 直接验证错误传播
	db4 := newDB(t)
	if err := db4.Create(&models.User{Username: "direct", Password: "x", Role: "viewer", Active: true}).Error; err != nil {
		t.Fatal(err)
	}
	failTable(db4, "roles")
	var u4 models.User
	if err := db4.First(&u4, "username = ?", "direct").Error; err != nil {
		t.Fatal(err)
	}
	if _, err := rbac.UserPermissionCodes(db4, u4.ID); err == nil {
		t.Error("UserPermissionCodes should surface roles query error")
	}

	// 5) HandleCreateRole：role 写入成功后 permissions 查询失败 → 400
	db5 := newDB(t)
	failTable(db5, "permissions")
	rh5 := NewRoleHandler(db5)
	r5 := gin.New()
	r5.POST("/roles", asAdmin(1), rh5.HandleCreateRole)
	w = doJSON(r5, "POST", "/roles", map[string]any{
		"code": "permfail1", "permissions": []string{"scripts:run"},
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("assign permissions failure: expected 400, got %d %s", w.Code, w.Body.String())
	}
}
