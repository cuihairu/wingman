package rbac

import (
	"errors"
	"strings"
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

// Seed 过程中内置角色权限同步（code IN 查询）失败应原样返回错误。
// 注入精度需区分步骤 1 的 `code = ?` 目录查询与步骤 2 的 `code IN (?)` 批量同步，
// 否则会在更早的分支提前返回（该分支已由上面的测试覆盖）。
func TestSeedRolePermissionSyncFailure(t *testing.T) {
	db := newTestDB(t)
	db.Callback().Query().After("gorm:query").Register("test:fail-perms-in", func(tx *gorm.DB) {
		if tx.Statement != nil && tx.Statement.Table == "permissions" &&
			strings.Contains(strings.ToUpper(tx.Statement.SQL.String()), "CODE IN") {
			tx.AddError(errors.New("forced role permission sync failure"))
		}
	})
	if err := Seed(db); err == nil {
		t.Fatal("Seed should surface role permission sync error")
	}
}
