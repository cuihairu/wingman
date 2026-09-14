package agent

import (
	"errors"
	"sync"
	"testing"
)

// fakeTagStore map 版 TagStore，无需 DB。
type fakeTagStore struct {
	mu    sync.Mutex
	store map[string][]string
	err   error // 非 nil 时 SaveTags 返回该错误
}

func newFakeTagStore() *fakeTagStore {
	return &fakeTagStore{store: map[string][]string{}}
}

func (f *fakeTagStore) LoadTags(agentID string) ([]string, bool) {
	f.mu.Lock()
	defer f.mu.Unlock()
	tags, ok := f.store[agentID]
	if !ok {
		return nil, false
	}
	return append([]string(nil), tags...), true
}

func (f *fakeTagStore) SaveTags(agentID, hostname, ip string, tags []string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.err != nil {
		return f.err
	}
	f.store[agentID] = append([]string(nil), tags...)
	return nil
}

func TestRegisterRestoresTagsFromStore(t *testing.T) {
	reg, _ := newTestRegistry(t)
	store := newFakeTagStore()
	store.store["a1"] = []string{"prod", "win"}
	reg.SetTagStore(store)

	reg.Register("a1", "host1", "10.0.0.1", nil)

	info, ok := reg.Get("a1")
	if !ok {
		t.Fatal("agent a1 should exist")
	}
	if len(info.Tags) != 2 || info.Tags[0] != "prod" || info.Tags[1] != "win" {
		t.Errorf("expected restored tags [prod win], got %v", info.Tags)
	}
}

func TestRegisterReconnectKeepsInMemoryTags(t *testing.T) {
	reg, _ := newTestRegistry(t)
	store := newFakeTagStore()
	reg.SetTagStore(store)

	reg.Register("a1", "host1", "10.0.0.1", nil)
	reg.SetTags("a1", []string{"fresh"})
	// 模拟 DB 仍是旧值（SetTags 失败未写穿）
	store.mu.Lock()
	store.store["a1"] = []string{"stale"}
	store.mu.Unlock()

	reg.Register("a1", "host1", "10.0.0.1", nil) // 重连

	info, _ := reg.Get("a1")
	if len(info.Tags) != 1 || info.Tags[0] != "fresh" {
		t.Errorf("expected in-memory tags [fresh] preserved, got %v", info.Tags)
	}
}

func TestSetTagsPersistsCleanedTags(t *testing.T) {
	reg, _ := newTestRegistry(t)
	store := newFakeTagStore()
	reg.SetTagStore(store)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	if !reg.SetTags("a1", []string{" prod ", "win", "prod", "  "}) {
		t.Fatal("SetTags should return true for existing agent")
	}
	stored, ok := store.LoadTags("a1")
	if !ok {
		t.Fatal("tags should be persisted")
	}
	if len(stored) != 2 || stored[0] != "prod" || stored[1] != "win" {
		t.Errorf("expected cleaned [prod win], got %v", stored)
	}
}

func TestSetTagsSaveFailureDoesNotRollback(t *testing.T) {
	reg, _ := newTestRegistry(t)
	store := newFakeTagStore()
	reg.SetTagStore(store)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	store.mu.Lock()
	store.err = errors.New("db down")
	store.mu.Unlock()

	if !reg.SetTags("a1", []string{"prod"}) {
		t.Fatal("SetTags should still return true when persist fails")
	}
	info, _ := reg.Get("a1")
	if len(info.Tags) != 1 || info.Tags[0] != "prod" {
		t.Errorf("in-memory tags should be updated, got %v", info.Tags)
	}
}

func TestRegistryWithoutTagStore(t *testing.T) {
	reg, _ := newTestRegistry(t)

	reg.Register("a1", "host1", "10.0.0.1", nil) // 无 store，不应 panic
	if !reg.SetTags("a1", []string{"prod"}) {
		t.Fatal("SetTags should work without store")
	}
	info, _ := reg.Get("a1")
	if len(info.Tags) != 1 || info.Tags[0] != "prod" {
		t.Errorf("expected [prod], got %v", info.Tags)
	}
}

func TestToJSONTagsAfterRestore(t *testing.T) {
	reg, _ := newTestRegistry(t)
	store := newFakeTagStore()
	reg.SetTagStore(store)

	// DB 无记录 → tags 归一为 []
	reg.Register("a1", "host1", "10.0.0.1", nil)
	info, _ := reg.Get("a1")
	data := info.ToJSON()
	tags, ok := data["tags"].([]string)
	if !ok {
		t.Fatalf("tags should be []string, got %T", data["tags"])
	}
	if len(tags) != 0 {
		t.Errorf("expected empty tags, got %v", tags)
	}

	// DB 有记录 → 恢复
	store.store["a2"] = []string{"prod"}
	reg.Register("a2", "host2", "10.0.0.2", nil)
	info2, _ := reg.Get("a2")
	data2 := info2.ToJSON()
	got, _ := data2["tags"].([]string)
	if len(got) != 1 || got[0] != "prod" {
		t.Errorf("expected [prod] in ToJSON, got %v", got)
	}
}

// 编译期保证 fakeTagStore 实现接口
var _ TagStore = (*fakeTagStore)(nil)
