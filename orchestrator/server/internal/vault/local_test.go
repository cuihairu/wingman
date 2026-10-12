package vault

import (
	"errors"
	"fmt"
	"math/rand"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// registry 与 local 实现的装配语义测试（行为面经 handlers 的 HTTP 用例
// 覆盖，这里测不依赖 HTTP 的接线口径）。

func TestActivateUnknownProviderErrors(t *testing.T) {
	err := Activate("no-such-provider", Options{})
	if err == nil || !strings.Contains(err.Error(), "unknown provider") {
		t.Fatalf("unknown provider should error with hint, got %v", err)
	}
	// 报错里列出已注册名（配置排错用）
	for _, name := range Names() {
		if !strings.Contains(err.Error(), name) {
			t.Fatalf("error should list registered %q: %v", name, err)
		}
	}
}

func TestActivateReplacesActiveInstance(t *testing.T) {
	Register("mem-a", func(Options) (Provider, error) { return memStub{name: "mem-a"}, nil })
	Register("mem-b", func(Options) (Provider, error) { return memStub{name: "mem-b"}, nil })
	defer clearActive()

	if err := Activate("mem-a", Options{}); err != nil {
		t.Fatalf("activate mem-a: %v", err)
	}
	if Active().Name() != "mem-a" || ActiveName() != "mem-a" {
		t.Fatalf("active should be mem-a, got %q", ActiveName())
	}
	if err := Activate("mem-b", Options{}); err != nil {
		t.Fatalf("activate mem-b: %v", err)
	}
	if Active().Name() != "mem-b" {
		t.Fatalf("active should be mem-b, got %q", ActiveName())
	}
}

func TestNewLocalRejectsBadOptions(t *testing.T) {
	if _, err := NewLocal(Options{}); err == nil {
		t.Fatal("missing db should be rejected")
	}
	for _, raw := range []string{"not-a-duration", "-1s", "0"} {
		if _, err := NewLocal(Options{Config: map[string]string{"SESSION_TTL": raw}}); err == nil {
			t.Fatalf("SESSION_TTL %q should be rejected", raw)
		}
	}
}

func TestLocalSessionTTLConfigAndStatus(t *testing.T) {
	db := newTestDB(t)
	_, err := NewLocal(Options{DB: db, Config: map[string]string{"SESSION_TTL": "123s"}})
	if err != nil {
		t.Fatalf("new local with SESSION_TTL: %v", err)
	}
	if _, err := NewLocal(Options{DB: db}); err != nil {
		t.Fatalf("new local without config: %v", err)
	}
	p, err := NewLocal(Options{DB: db, Config: map[string]string{"SESSION_TTL": "123s"}})
	if err != nil {
		t.Fatalf("new local: %v", err)
	}
	st, err := p.Status(1)
	if err != nil {
		t.Fatalf("status: %v", err)
	}
	if st.Configured || st.Unlocked {
		t.Fatalf("fresh user should be unconfigured+locked: %+v", st)
	}
	if st.AutoLockAfterS != 123 {
		t.Fatalf("autoLockAfterS should reflect config, got %d", st.AutoLockAfterS)
	}
}

func TestLocalSessionExpiryLazyDrops(t *testing.T) {
	db := newTestDB(t)
	factory, err := NewLocal(Options{DB: db, Config: map[string]string{"SESSION_TTL": "20ms"}})
	if err != nil {
		t.Fatalf("new local: %v", err)
	}
	p := factory.(*LocalProvider)
	if _, err := p.Setup(7, "master-pass-1"); err != nil {
		t.Fatalf("setup: %v", err)
	}
	// 未过期：驻留可取用，且刷新到期时刻
	if p.keepAlive(7) == nil {
		t.Fatal("fresh session should be alive")
	}
	time.Sleep(60 * time.Millisecond) // 20ms TTL + 余量，单调安全
	if p.keepAlive(7) != nil {
		t.Fatal("expired session must lazily drop")
	}
	if p.Lock(7) {
		t.Fatal("lock after expiry should report not-unlocked")
	}
	st, err := p.Status(7)
	if err != nil || st.Unlocked || st.UnlockedUntil != "" {
		t.Fatalf("expired status should be locked: %+v %v", st, err)
	}
}

func TestDefaultProtocolPort(t *testing.T) {
	cases := map[string]int{"rdp": 3389, "vnc": 5900, "ssh": 22, "ftp": 0, "": 0}
	for proto, want := range cases {
		if got := DefaultProtocolPort(proto); got != want {
			t.Fatalf("DefaultProtocolPort(%q) = %d, want %d", proto, got, want)
		}
	}
}

func TestRegisterRejectsBadRegistration(t *testing.T) {
	for _, bad := range []struct {
		name    string
		factory Factory
	}{
		{"", func(Options) (Provider, error) { return nil, errors.New("x") }},
		{"x", nil},
	} {
		func() {
			defer func() {
				if recover() == nil {
					t.Fatalf("Register(%q) should panic", bad.name)
				}
			}()
			Register(bad.name, bad.factory)
		}()
	}
}

// ---------- 测试脚手架 ----------

func clearActive() {
	actMu.Lock()
	active, actName = nil, ""
	actMu.Unlock()
}

// newTestDB 独立内存库（models.AutoMigrate 后单连接串行，避免
// SQLITE_LOCKED 静默失败）。
func newTestDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(fmt.Sprintf("file:vault_%d?mode=memory&cache=shared", rand.Int())), &gorm.Config{})
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

// memStub registry 测试用的最小 Provider。
type memStub struct{ name string }

func (s memStub) Name() string { return s.name }
func (s memStub) Status(userID uint) (MasterStatus, error) {
	return MasterStatus{Configured: true, Unlocked: true, AutoLockAfterS: 60}, nil
}
func (memStub) Setup(userID uint, masterPassword string) (time.Time, error) {
	return time.Time{}, errors.New("mem stub")
}
func (memStub) Unlock(userID uint, masterPassword string) (time.Time, error) {
	return time.Time{}, errors.New("mem stub")
}
func (memStub) ChangePassword(userID uint, currentPassword, newPassword string) error {
	return errors.New("mem stub")
}
func (memStub) Lock(userID uint) bool                 { return false }
func (memStub) List(userID uint) ([]EntryMeta, error) { return []EntryMeta{}, nil }
func (memStub) Upsert(userID uint, entry Entry) (EntryMeta, bool, error) {
	return EntryMeta{}, true, errors.New("mem stub")
}
func (memStub) Delete(userID uint, entryID uint) error { return nil }
func (memStub) Export(userID uint) (map[string]any, error) {
	return map[string]any{}, nil
}
func (memStub) Lookup(userID uint, agentID, protocol string, port int) (string, string, string, error) {
	return "", "", "", nil
}
