package agentstore

import (
	"fmt"
	"math/rand"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// setupStoreDB 内存 sqlite + AutoMigrate，沿用 handlers 包的单连接惯例。
func setupStoreDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:agentstore_%d?mode=memory&cache=shared", rand.Int())), &gorm.Config{})
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	if sqlDB, err := db.DB(); err == nil {
		sqlDB.SetMaxOpenConns(1)
	}
	if err := models.AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return db
}

func TestTagStoreRoundtrip(t *testing.T) {
	db := setupStoreDB(t)
	store := NewAgentTagStore(db)

	if err := store.SaveTags("a1", "host1", "10.0.0.1", []string{"prod", "win"}); err != nil {
		t.Fatalf("save: %v", err)
	}
	tags, ok := store.LoadTags("a1")
	if !ok {
		t.Fatal("tags should load after save")
	}
	if len(tags) != 2 || tags[0] != "prod" || tags[1] != "win" {
		t.Errorf("expected [prod win], got %v", tags)
	}
}

func TestTagStoreSaveUpdatesExistingRow(t *testing.T) {
	db := setupStoreDB(t)
	store := NewAgentTagStore(db)

	_ = store.SaveTags("a1", "host1", "10.0.0.1", []string{"old"})
	_ = store.SaveTags("a1", "host1-new", "10.0.0.9", []string{"new"})

	var count int64
	db.Model(&models.Agent{}).Where("agent_id = ?", "a1").Count(&count)
	if count != 1 {
		t.Fatalf("expected 1 row for a1, got %d", count)
	}
	tags, ok := store.LoadTags("a1")
	if !ok || len(tags) != 1 || tags[0] != "new" {
		t.Fatalf("expected [new], got %v ok=%v", tags, ok)
	}
	var rec models.Agent
	db.Where("agent_id = ?", "a1").First(&rec)
	if rec.Hostname != "host1-new" || rec.IP != "10.0.0.9" {
		t.Errorf("metadata should be refreshed, got %s@%s", rec.Hostname, rec.IP)
	}
}

func TestTagStoreLoadUnknownAgent(t *testing.T) {
	db := setupStoreDB(t)
	store := NewAgentTagStore(db)

	if _, ok := store.LoadTags("ghost"); ok {
		t.Error("unknown agent should return false")
	}
}

func TestTagStoreLoadInvalidJSON(t *testing.T) {
	db := setupStoreDB(t)
	store := NewAgentTagStore(db)

	if err := db.Create(&models.Agent{AgentID: "bad", Tags: "not-json"}).Error; err != nil {
		t.Fatalf("seed: %v", err)
	}
	if _, ok := store.LoadTags("bad"); ok {
		t.Error("invalid JSON should return false")
	}
}

func TestTagStoreEmptyTagsRoundtrip(t *testing.T) {
	db := setupStoreDB(t)
	store := NewAgentTagStore(db)

	_ = store.SaveTags("a1", "host1", "ip", []string{})
	tags, ok := store.LoadTags("a1")
	if !ok {
		t.Fatal("persisted empty list should load as ok=true")
	}
	if len(tags) != 0 {
		t.Errorf("expected empty list, got %v", tags)
	}

	// nil 归一为 "[]"，同样可 roundtrip
	_ = store.SaveTags("a2", "host2", "ip", nil)
	tags2, ok2 := store.LoadTags("a2")
	if !ok2 || len(tags2) != 0 {
		t.Errorf("nil tags should persist as [], got %v ok=%v", tags2, ok2)
	}
}
