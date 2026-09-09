package models

import (
	"fmt"
	"math/rand"
	"testing"
	"time"

	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func TestUserToSafe(t *testing.T) {
	now := time.Date(2026, 1, 2, 3, 4, 5, 0, time.UTC)
	last := time.Date(2026, 2, 3, 4, 5, 6, 0, time.UTC)
	u := User{
		Model:       gorm.Model{CreatedAt: now, UpdatedAt: now},
		Username:    "alice",
		Nickname:    "",
		Email:       "a@b.c",
		Phone:       "123",
		Avatar:      "img",
		Role:        "viewer",
		Active:      true,
		LastLoginAt: &last,
	}
	s := u.ToSafe()
	if s.DisplayName != "alice" {
		t.Errorf("empty nickname should fall back to username, got %q", s.DisplayName)
	}
	if s.Nickname != "" {
		t.Errorf("nickname should stay empty, got %q", s.Nickname)
	}
	if s.CreatedAt != "2026-01-02 03:04:05" || s.UpdatedAt != "2026-01-02 03:04:05" {
		t.Errorf("timestamps not formatted: %q %q", s.CreatedAt, s.UpdatedAt)
	}
	if s.LastLoginAt != "2026-02-03 04:05:06" {
		t.Errorf("lastLogin not formatted: %q", s.LastLoginAt)
	}

	// 零值时间 → 空字符串；昵称优先于用户名
	u2 := User{Username: "bob", Nickname: "Bobby"}
	s2 := u2.ToSafe()
	if s2.DisplayName != "Bobby" || s2.CreatedAt != "" || s2.UpdatedAt != "" || s2.LastLoginAt != "" {
		t.Errorf("unexpected safe view: %+v", s2)
	}

	// nil LastLoginAt
	u3 := User{Username: "carol"}
	if s3 := u3.ToSafe(); s3.LastLoginAt != "" {
		t.Errorf("nil LastLoginAt should be empty, got %q", s3.LastLoginAt)
	}
}

func TestWorkflowStepsRoundtrip(t *testing.T) {
	w := &Workflow{}
	if w.GetSteps() != nil {
		t.Error("empty StepsJSON should return nil")
	}
	if w.GetContext() != nil {
		t.Error("empty ContextJSON should return nil")
	}

	steps := []WorkflowStep{
		{ID: "a", Script: "a.lua"},
		{ID: "b", Script: "b.lua", DependsOn: []string{"a"}, MaxRetries: 2},
	}
	if err := w.SetSteps(steps); err != nil {
		t.Fatalf("set steps: %v", err)
	}
	got := w.GetSteps()
	if len(got) != 2 || got[0].ID != "a" || got[1].DependsOn[0] != "a" {
		t.Errorf("steps roundtrip failed: %+v", got)
	}

	if err := w.SetContext(map[string]interface{}{"k": "v"}); err != nil {
		t.Fatalf("set context: %v", err)
	}
	ctx := w.GetContext()
	if ctx["k"] != "v" {
		t.Errorf("context roundtrip failed: %+v", ctx)
	}

	// 含不可序列化值时 SetContext 报错
	if err := w.SetContext(map[string]interface{}{"ch": make(chan int)}); err == nil {
		t.Error("expected marshal error for unserializable context")
	}

	// 非法 JSON → 空结果
	w.StepsJSON = "{invalid"
	if s := w.GetSteps(); s != nil {
		t.Errorf("invalid JSON should yield nil, got %+v", s)
	}
	w.ContextJSON = "{invalid"
	if c := w.GetContext(); c != nil {
		t.Errorf("invalid JSON context should yield nil, got %+v", c)
	}

	// 含不可序列化参数时 SetSteps 报错
	bad := []WorkflowStep{{ID: "x", Parameters: map[string]interface{}{"ch": make(chan int)}}}
	if err := w.SetSteps(bad); err == nil {
		t.Error("expected marshal error for unserializable step parameters")
	}
}

func TestTableNames(t *testing.T) {
	if (Workflow{}).TableName() != "workflows" {
		t.Error("workflow table name mismatch")
	}
	if (StepStatus{}).TableName() != "step_statuses" {
		t.Error("step status table name mismatch")
	}
	if (MessageRead{}).TableName() != "message_reads" {
		t.Error("message read table name mismatch")
	}
}

func TestPermissionCodesSkipsEmpty(t *testing.T) {
	r := Role{Permissions: []Permission{
		{Code: "a:b"},
		{Code: ""},
		{Code: "c:d"},
	}}
	codes := r.PermissionCodes()
	if len(codes) != 2 || codes[0] != "a:b" || codes[1] != "c:d" {
		t.Errorf("unexpected codes: %v", codes)
	}
	if got := (&Role{}).PermissionCodes(); len(got) != 0 {
		t.Errorf("empty role should yield empty codes, got %v", got)
	}
}

func TestAutoMigrate(t *testing.T) {
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:models_migrate_%d?mode=memory&cache=shared", rand.Int())), &gorm.Config{})
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	// 单连接串行化，规避 cache=shared 多连接并发的 SQLITE_LOCKED
	if sqlDB, err := db.DB(); err == nil {
		sqlDB.SetMaxOpenConns(1)
	}
	if err := AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	for _, table := range []string{"users", "scripts", "settings", "execution_logs", "audit_logs",
		"messages", "message_reads", "feedbacks", "agents", "workflows", "step_statuses",
		"roles", "permissions"} {
		if !db.Migrator().HasTable(table) {
			t.Errorf("table %s should exist", table)
		}
	}
}

func TestLoadRolePermissions(t *testing.T) {
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:models_role_%d?mode=memory&cache=shared", rand.Int())), &gorm.Config{})
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	// 单连接串行化，规避 cache=shared 多连接并发的 SQLITE_LOCKED
	if sqlDB, err := db.DB(); err == nil {
		sqlDB.SetMaxOpenConns(1)
	}
	if err := AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	perm := Permission{Code: "x:y"}
	if err := db.Create(&perm).Error; err != nil {
		t.Fatal(err)
	}
	role := Role{Code: "custom"}
	if err := db.Create(&role).Error; err != nil {
		t.Fatal(err)
	}
	if err := db.Model(&role).Association("Permissions").Append(&perm); err != nil {
		t.Fatal(err)
	}

	if err := LoadRolePermissions(db, &role); err != nil {
		t.Fatalf("load: %v", err)
	}
	if len(role.Permissions) != 1 || role.Permissions[0].Code != "x:y" {
		t.Errorf("unexpected permissions: %+v", role.Permissions)
	}
}
