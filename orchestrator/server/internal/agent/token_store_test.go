package agent

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

func newTokenTestDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:token_store_%d?mode=memory&cache=shared", rand.Int())), &gorm.Config{})
	if err != nil {
		t.Fatalf("open sqlite: %v", err)
	}
	if err := db.AutoMigrate(&models.AgentToken{}); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	t.Cleanup(func() { sqlDB, _ := db.DB(); _ = sqlDB.Close() })
	return db
}

func mustCreateToken(t *testing.T, store *TokenStore, label, agentID string) string {
	t.Helper()
	plain, rec, err := store.Create(label, agentID, "tester")
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if plain == "" || !strings.HasPrefix(plain, TokenPrefix) {
		t.Fatalf("plain token should have %q prefix, got %q", TokenPrefix, plain)
	}
	if rec.TokenHash == "" || rec.TokenHash == plain {
		t.Fatalf("hash must differ from plain token and be non-empty")
	}
	if rec.Prefix != plain[:len(TokenPrefix)+6] {
		t.Fatalf("prefix mismatch: %q vs %q", rec.Prefix, plain[:len(TokenPrefix)+6])
	}
	return plain
}

func TestTokenStoreCreateVerifyRoundtrip(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "label-1", "")

	rec, ok := store.Verify(plain, "any-agent")
	if !ok {
		t.Fatalf("Verify should accept freshly created token")
	}
	if rec.LastSeenAt == nil {
		t.Fatalf("successful verify should refresh LastSeenAt")
	}
	if rec.Label != "label-1" {
		t.Fatalf("label mismatch: %q", rec.Label)
	}
}

func TestTokenStoreVerifyRejectsUnknownAndEmpty(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	mustCreateToken(t, store, "label-1", "")

	if _, ok := store.Verify("wt_definitely-not-a-real-token", "agent-x"); ok {
		t.Fatalf("unknown token should be rejected")
	}
	if _, ok := store.Verify("", "agent-x"); ok {
		t.Fatalf("empty token should be rejected")
	}
}

func TestTokenStoreRevokeIdempotent(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "label-1", "")

	revoked, err := store.Revoke(1)
	if err != nil || !revoked {
		t.Fatalf("first revoke: revoked=%v err=%v", revoked, err)
	}
	if _, ok := store.Verify(plain, "agent-x"); ok {
		t.Fatalf("revoked token must be rejected")
	}
	revoked, err = store.Revoke(1)
	if err != nil || revoked {
		t.Fatalf("second revoke should be no-op: revoked=%v err=%v", revoked, err)
	}
}

func TestTokenStoreAgentBinding(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	plain := mustCreateToken(t, store, "bound", "agent-bound")

	if _, ok := store.Verify(plain, "agent-other"); ok {
		t.Fatalf("bound token must not validate for a different agentId")
	}
	if _, ok := store.Verify(plain, "agent-bound"); !ok {
		t.Fatalf("bound token must validate for the bound agentId")
	}
}

func TestTokenStoreListHidesHash(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	mustCreateToken(t, store, "visible", "a1")

	list, err := store.List()
	if err != nil || len(list) != 1 {
		t.Fatalf("List: len=%d err=%v", len(list), err)
	}
	encoded, err := json.Marshal(list[0])
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	if strings.Contains(string(encoded), "tokenHash") || strings.Contains(string(encoded), "TokenHash") {
		t.Fatalf("List JSON must not leak hash field, got %s", encoded)
	}
	if list[0].Prefix == "" || list[0].AgentID != "a1" {
		t.Fatalf("List DTO fields missing: %+v", list[0])
	}
}

func TestTokenStoreAnyExistsFailClosed(t *testing.T) {
	store := NewTokenStore(newTokenTestDB(t))
	ok, err := store.AnyExists()
	if err != nil || ok {
		t.Fatalf("empty store: exists=%v err=%v", ok, err)
	}
	mustCreateToken(t, store, "t1", "")
	mustCreateToken(t, store, "t2", "")
	if ok, _ := store.AnyExists(); !ok {
		t.Fatalf("issued records should enable DB source")
	}
	// 吊销全部记录后 DB 源必须保持启用（fail-closed，防「吊销即开门」）
	_, _ = store.Revoke(1)
	_, _ = store.Revoke(2)
	if ok, _ := store.AnyExists(); !ok {
		t.Fatalf("revoked-all store must still count as configured (fail-closed)")
	}
}
