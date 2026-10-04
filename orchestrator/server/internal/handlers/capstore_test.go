package handlers

import (
	"reflect"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// CapabilityStore 的持久化 roundtrip：Save → Load 应还原；空集清除语义
// （"[]"）应可 roundtrip；无记录返回 (nil, false)。
func TestAgentCapabilityStoreRoundtrip(t *testing.T) {
	db := newDB(t)
	store := NewAgentCapabilityStore(db).(agent.CapabilityStore)

	// 无记录
	if _, ok := store.LoadCapabilities("ghost"); ok {
		t.Fatal("LoadCapabilities(ghost) should be (nil, false) for unknown agent")
	}

	// Save → Load roundtrip
	caps := []string{"screen.capture", "input.mouse", "custom.vendor.X"}
	if err := store.SaveCapabilities("a1", caps); err != nil {
		t.Fatalf("save: %v", err)
	}
	got, ok := store.LoadCapabilities("a1")
	if !ok {
		t.Fatal("load after save should hit")
	}
	if !reflect.DeepEqual(got, caps) {
		t.Errorf("roundtrip = %v, want %v", got, caps)
	}

	// 更新已有行（重复 Save 不产生第二行）
	if err := store.SaveCapabilities("a1", []string{"ocr"}); err != nil {
		t.Fatalf("resave: %v", err)
	}
	var count int64
	db.Model(&models.Agent{}).Count(&count)
	if count != 1 {
		t.Errorf("expected 1 agent row after repeated saves, got %d", count)
	}
	got, ok = store.LoadCapabilities("a1")
	if !ok || !reflect.DeepEqual(got, []string{"ocr"}) {
		t.Errorf("updated caps = %v (ok=%v), want [ocr]", got, ok)
	}

	// 空集清除语义 roundtrip
	if err := store.SaveCapabilities("a1", []string{}); err != nil {
		t.Fatalf("clear: %v", err)
	}
	got, ok = store.LoadCapabilities("a1")
	if !ok || len(got) != 0 {
		t.Errorf("cleared caps should roundtrip to empty slice, got %v (ok=%v)", got, ok)
	}
}
