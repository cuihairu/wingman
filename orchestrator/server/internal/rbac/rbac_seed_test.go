package rbac

import (
	"errors"
	"testing"

	"gorm.io/gorm"
)

// Seed 过程中权限目录查询失败应原样返回错误。
func TestSeedPermissionsQueryFailure(t *testing.T) {
	db := newTestDB(t)
	// 仅对 permissions 表注入查询失败（roles/role_permissions 不受影响）
	db.Callback().Query().Before("gorm:query").Register("test:fail-perms", func(tx *gorm.DB) {
		if tx.Statement.Table == "permissions" {
			tx.AddError(errors.New("forced permissions failure"))
		}
	})
	if err := Seed(db); err == nil {
		t.Fatal("Seed should surface permissions query error")
	}
}
