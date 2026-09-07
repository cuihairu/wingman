package rbac

import (
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

func TestSeedRejectsNilDB(t *testing.T) {
	if err := Seed(nil); err == nil {
		t.Fatal("nil db should be rejected")
	}
}

func closedDB(t *testing.T) *gorm.DB {
	t.Helper()
	db := newTestDB(t)
	sqlDB, err := db.DB()
	if err != nil {
		t.Fatalf("raw db: %v", err)
	}
	if err := sqlDB.Close(); err != nil {
		t.Fatalf("close db: %v", err)
	}
	return db
}

func TestSeedBackfillsMissingPermissionMetadata(t *testing.T) {
	db := newTestDB(t)

	// 模拟历史数据：权限存在但元数据为空 → Seed 应补齐
	var perm models.Permission
	if err := db.Where("code = ?", "users:manage").First(&perm).Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Model(&perm).Updates(map[string]any{"name": "", "category": ""}).Error; err != nil {
		t.Fatal(err)
	}
	// 角色存在但名称为空 → Seed 应补齐
	var role models.Role
	if err := db.Where("code = ?", "viewer").First(&role).Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Model(&role).Updates(map[string]any{"name": ""}).Error; err != nil {
		t.Fatal(err)
	}

	if err := Seed(db); err != nil {
		t.Fatalf("reseed: %v", err)
	}
	if err := db.Where("code = ?", "users:manage").First(&perm).Error; err != nil {
		t.Fatal(err)
	}
	if perm.Name == "" || perm.Category == "" {
		t.Errorf("permission metadata not backfilled: %+v", perm)
	}
	if err := db.Where("code = ?", "viewer").First(&role).Error; err != nil {
		t.Fatal(err)
	}
	if role.Name == "" {
		t.Errorf("role name not backfilled: %+v", role)
	}
}

func TestSeedResolvesWildcardAssociationForAdmin(t *testing.T) {
	db := newTestDB(t)
	var role models.Role
	if err := db.Preload("Permissions").Where("code = ?", AdminRole).First(&role).Error; err != nil {
		t.Fatal(err)
	}
	// admin 角色的权限码包含通配符，实际关联了全部内置权限
	if len(role.Permissions) != len(builtinPermissions) {
		t.Errorf("admin should be wired to all builtin permissions, got %d", len(role.Permissions))
	}
}

func TestSeedHandlesClosedDB(t *testing.T) {
	// 权限目录写入在关闭的库上失败 → Seed 报错
	db := closedDB(t)
	if err := Seed(db); err == nil {
		t.Fatal("seed on closed db should fail")
	}
}

func TestSeedRoleCreateError(t *testing.T) {
	db := newTestDB(t)
	// 权限目录已完好；删除 roles 表使角色创建失败
	if err := db.Migrator().DropTable("roles"); err != nil {
		t.Fatalf("drop roles: %v", err)
	}
	if err := Seed(db); err == nil {
		t.Fatal("seed should fail when roles table is missing")
	}
}

func TestSeedAssociationReplaceError(t *testing.T) {
	db := newTestDB(t)
	// 删除多对多关联表，使 Replace 失败
	if err := db.Migrator().DropTable("role_permissions"); err != nil {
		t.Fatalf("drop role_permissions: %v", err)
	}
	if err := Seed(db); err == nil {
		t.Fatal("seed should fail when association table is missing")
	}
}

func TestRolePermissionCodesDBError(t *testing.T) {
	db := closedDB(t)
	if _, err := RolePermissionCodes(db, "operator"); err == nil {
		t.Fatal("expected db error")
	}
}

func TestUserPermissionCodesDBError(t *testing.T) {
	db := closedDB(t)
	if _, err := UserPermissionCodes(db, 1); err == nil {
		t.Fatal("expected db error")
	}
}

func TestUserPermissionCodesUnknownUser(t *testing.T) {
	db := newTestDB(t)
	codes, err := UserPermissionCodes(db, 99999)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(codes) != 0 {
		t.Errorf("unknown user should resolve empty, got %v", codes)
	}
}

func TestHasPermission(t *testing.T) {
	db := newTestDB(t)
	user := models.User{Username: "hp", Password: "x", Role: "operator", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatal(err)
	}

	ok, err := HasPermission(db, user.ID, "scripts:run")
	if err != nil || !ok {
		t.Errorf("operator scripts:run: ok=%v err=%v", ok, err)
	}
	// 大小写不敏感匹配
	ok, _ = HasPermission(db, user.ID, "  SCRIPTS:RUN  ")
	if !ok {
		t.Error("case-insensitive permission should match")
	}
	ok, _ = HasPermission(db, user.ID, "users:manage")
	if ok {
		t.Error("operator should not have users:manage")
	}
	// admin 通配
	admin := models.User{Username: "ha", Password: "x", Role: AdminRole, Active: true}
	if err := db.Create(&admin).Error; err != nil {
		t.Fatal(err)
	}
	ok, _ = HasPermission(db, admin.ID, "whatever:thing")
	if !ok {
		t.Error("admin wildcard should match anything")
	}

	// 数据库错误透传
	closed := closedDB(t)
	if _, err := HasPermission(closed, 1, "x:y"); err == nil {
		t.Error("expected error from closed db")
	}
}

func TestEnsureRoleExistsDBError(t *testing.T) {
	db := closedDB(t)
	if _, err := EnsureRoleExists(db, "operator"); err == nil {
		t.Fatal("expected db error")
	}
}

func TestIsAdminVariants(t *testing.T) {
	for _, in := range []string{"admin", "ADMIN", "  Admin  "} {
		if !IsAdmin(in) {
			t.Errorf("IsAdmin(%q) should be true", in)
		}
	}
	for _, in := range []string{"", "operator", "admini"} {
		if IsAdmin(in) {
			t.Errorf("IsAdmin(%q) should be false", in)
		}
	}
}

func TestAllPermissionCodesIncludesWildcard(t *testing.T) {
	codes := allPermissionCodes()
	if codes[0] != Wildcard {
		t.Errorf("first code should be wildcard, got %v", codes[0])
	}
	if len(codes) != len(builtinPermissions)+1 {
		t.Errorf("unexpected code count: %d", len(codes))
	}
}
