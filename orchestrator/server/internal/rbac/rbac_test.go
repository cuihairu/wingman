package rbac

import (
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// newTestDB 构建一个已迁移并完成 RBAC 种子的内存数据库。
// 接收 testing.TB 以便 Test 与 Benchmark 复用。
func newTestDB(tb testing.TB) *gorm.DB {
	tb.Helper()
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{})
	if err != nil {
		tb.Fatalf("open db: %v", err)
	}
	if err := models.AutoMigrate(db); err != nil {
		tb.Fatalf("migrate: %v", err)
	}
	if err := Seed(db); err != nil {
		tb.Fatalf("seed: %v", err)
	}
	return db
}

func TestSeedIsIdempotent(t *testing.T) {
	db := newTestDB(t)
	// 第二次 Seed 不应报错，且不应重复创建角色/权限
	if err := Seed(db); err != nil {
		t.Fatalf("second seed: %v", err)
	}
	var roleCount, permCount int64
	db.Model(&models.Role{}).Count(&roleCount)
	db.Model(&models.Permission{}).Count(&permCount)
	if roleCount != 3 {
		t.Errorf("expected 3 builtin roles, got %d", roleCount)
	}
	if permCount != int64(len(builtinPermissions)) {
		t.Errorf("expected %d permissions, got %d", len(builtinPermissions), permCount)
	}
}

func TestSeedWiresRolePermissions(t *testing.T) {
	db := newTestDB(t)

	codes, err := RolePermissionCodes(db, "operator")
	if err != nil {
		t.Fatalf("operator codes: %v", err)
	}
	if len(codes) == 0 {
		t.Fatal("operator should have permissions")
	}
	if !contains(codes, "scripts:run") {
		t.Errorf("operator should include scripts:run, got %v", codes)
	}
	if contains(codes, "users:manage") {
		t.Errorf("operator must not include users:manage, got %v", codes)
	}
}

func TestAdminBypass(t *testing.T) {
	db := newTestDB(t)

	if !IsAdmin("admin") {
		t.Error("admin should be recognized")
	}
	if IsAdmin("operator") {
		t.Error("operator should not be admin")
	}

	codes, err := RolePermissionCodes(db, AdminRole)
	if err != nil {
		t.Fatalf("admin codes: %v", err)
	}
	if len(codes) != 1 || codes[0] != Wildcard {
		t.Errorf("admin should resolve to wildcard, got %v", codes)
	}

	// admin 用户对任意权限都放行
	user := models.User{Username: "root", Password: "x", Role: AdminRole, Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}
	ok, err := HasPermission(db, user.ID, "anything:weird")
	if err != nil {
		t.Fatalf("hasperm: %v", err)
	}
	if !ok {
		t.Error("admin should pass any permission check")
	}
}

func TestUserPermissionCodes(t *testing.T) {
	db := newTestDB(t)

	user := models.User{Username: "op1", Password: "x", Role: "operator", Active: true}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}

	codes, err := UserPermissionCodes(db, user.ID)
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	if !contains(codes, "workflows:run") {
		t.Errorf("operator user should have workflows:run, got %v", codes)
	}
	if contains(codes, "roles:manage") {
		t.Errorf("operator user must not have roles:manage, got %v", codes)
	}
}

func TestInactiveUserHasNoPermissions(t *testing.T) {
	db := newTestDB(t)

	user := models.User{Username: "off", Password: "x", Role: "operator", Active: false}
	if err := db.Create(&user).Error; err != nil {
		t.Fatalf("create user: %v", err)
	}
	codes, err := UserPermissionCodes(db, user.ID)
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	if len(codes) != 0 {
		t.Errorf("inactive user should have no permissions, got %v", codes)
	}
}

func TestUnknownRoleResolvesEmpty(t *testing.T) {
	db := newTestDB(t)
	codes, err := RolePermissionCodes(db, "ghost")
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	if len(codes) != 0 {
		t.Errorf("unknown role should resolve empty, got %v", codes)
	}
}

func TestEnsureRoleExists(t *testing.T) {
	db := newTestDB(t)
	if ok, _ := EnsureRoleExists(db, "admin"); !ok {
		t.Error("admin should exist")
	}
	if ok, _ := EnsureRoleExists(db, "operator"); !ok {
		t.Error("operator should exist")
	}
	if ok, _ := EnsureRoleExists(db, "nope"); ok {
		t.Error("nope should not exist")
	}
}

func contains(values []string, target string) bool {
	for _, v := range values {
		if v == target {
			return true
		}
	}
	return false
}
