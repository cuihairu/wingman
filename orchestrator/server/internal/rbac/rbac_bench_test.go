package rbac

import (
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/gorm"
)

// newBenchUser 在 db 中创建一个 operator 角色用户，返回其 id（供 benchmark 走非 admin 解析路径）。
func newBenchUser(tb testing.TB, db *gorm.DB) uint {
	tb.Helper()
	u := models.User{Username: "bench_operator", Password: "x", Role: "operator", Active: true}
	if err := db.Create(&u).Error; err != nil {
		tb.Fatalf("create bench user: %v", err)
	}
	return u.ID
}

// BenchmarkUserPermissionCodes 非 admin 用户权限解析（DB 多表 join；无请求级缓存时的开销基线）。
// 中间件层 resolvePermissionCodes 在单请求内缓存结果，本基准度量的是未命中时的原始解析成本。
func BenchmarkUserPermissionCodes(b *testing.B) {
	db := newTestDB(b)
	uid := newBenchUser(b, db)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err := UserPermissionCodes(db, uid); err != nil {
			b.Fatalf("resolve: %v", err)
		}
	}
}

// BenchmarkHasPermission 单权限命中判定（在真实权限码集合上线性扫描）。
func BenchmarkHasPermission(b *testing.B) {
	db := newTestDB(b)
	uid := newBenchUser(b, db)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err := HasPermission(db, uid, "scripts:run"); err != nil {
			b.Fatalf("has permission: %v", err)
		}
	}
}

// BenchmarkAdminBypass admin 角色短路（应显著快于非 admin 路径）。
func BenchmarkAdminBypass(b *testing.B) {
	db := newTestDB(b)
	u := models.User{Username: "bench_admin", Password: "x", Role: "admin", Active: true}
	if err := db.Create(&u).Error; err != nil {
		b.Fatalf("create admin: %v", err)
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, err := UserPermissionCodes(db, u.ID); err != nil {
			b.Fatalf("admin resolve: %v", err)
		}
	}
}
