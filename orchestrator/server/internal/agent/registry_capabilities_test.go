package agent

import (
	"encoding/json"
	"net"
	"reflect"
	"sync"
	"testing"
	"time"

	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
)

// fakeCapabilityStore 内存版 CapabilityStore：记录 Save 落库值并支持预置 Load 数据。
type fakeCapabilityStore struct {
	mu    sync.Mutex
	saved map[string][]string
	load  map[string][]string
}

func (f *fakeCapabilityStore) LoadCapabilities(agentID string) ([]string, bool) {
	f.mu.Lock()
	defer f.mu.Unlock()
	s, ok := f.load[agentID]
	if !ok {
		return nil, false
	}
	return append([]string(nil), s...), true
}

func (f *fakeCapabilityStore) SaveCapabilities(agentID string, caps []string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.saved == nil {
		f.saved = map[string][]string{}
	}
	if f.load == nil {
		f.load = map[string][]string{}
	}
	f.saved[agentID] = append([]string(nil), caps...)
	f.load[agentID] = append([]string(nil), caps...) // 模拟真实 DB：保存后可读回
	return nil
}

func (f *fakeCapabilityStore) savedCaps(agentID string) ([]string, bool) {
	f.mu.Lock()
	defer f.mu.Unlock()
	s, ok := f.saved[agentID]
	return s, ok
}

func newTestRegistryCaps(t *testing.T) *Registry {
	t.Helper()
	hub := ws.NewHub()
	go hub.Run()
	return NewRegistry(hub)
}

// UpdateCapabilities 应写穿 fake store；新 Registry 从 Store 恢复能力集；
// 词汇表外的未知能力应经 unknownCapabilities 呈现。
func TestUpdateCapabilitiesPersistsAndRestores(t *testing.T) {
	store := &fakeCapabilityStore{}
	reg := newTestRegistryCaps(t)
	reg.SetCapabilityStore(store)

	reg.Register("a1", "host-a", "10.0.0.1", nil)
	caps := []string{"screen.capture", "ml.onnx", "custom.vendor.X"}
	reg.UpdateCapabilities("a1", caps)

	saved, ok := store.savedCaps("a1")
	if !ok {
		t.Fatal("capabilities were not persisted to store")
	}
	if !reflect.DeepEqual(saved, caps) {
		t.Errorf("saved caps = %v, want %v", saved, caps)
	}

	// 新注册表（模拟 server 重启）从同一 store 恢复
	reg2 := newTestRegistryCaps(t)
	reg2.SetCapabilityStore(store)
	reg2.Register("a1", "host-a", "10.0.0.1", nil)
	info, _ := reg2.Get("a1")
	if !reflect.DeepEqual(info.Capabilities, caps) {
		t.Errorf("restored caps = %v, want %v", info.Capabilities, caps)
	}

	view := info.ToJSON()
	viewCaps, _ := view["capabilities"].([]string)
	if !reflect.DeepEqual(viewCaps, caps) {
		t.Errorf("ToJSON capabilities = %v, want %v", viewCaps, caps)
	}
	unknown, _ := view["unknownCapabilities"].([]string)
	wantUnknown := []string{"custom.vendor.X"}
	if !reflect.DeepEqual(unknown, wantUnknown) {
		t.Errorf("ToJSON unknownCapabilities = %v, want %v", unknown, wantUnknown)
	}
}

// 无 CapabilityStore 时 UpdateCapabilities 应静默只改内存（单测环境不注入 store）。
func TestUpdateCapabilitiesWithoutStoreSucceeds(t *testing.T) {
	reg := newTestRegistryCaps(t)
	reg.Register("a1", "host-a", "10.0.0.1", nil)
	reg.UpdateCapabilities("a1", []string{"screen.capture"})

	info, ok := reg.Get("a1")
	if !ok || !reflect.DeepEqual(info.Capabilities, []string{"screen.capture"}) {
		t.Fatalf("expected in-memory capabilities, got %v (ok=%v)", info.Capabilities, ok)
	}
}

// 重连保留内存值：贴近首次上报后 store 数据陈旧，重连不应被旧值覆盖。
func TestRegisterReconnectKeepsMemoryCapabilities(t *testing.T) {
	store := &fakeCapabilityStore{}
	reg := newTestRegistryCaps(t)
	reg.SetCapabilityStore(store)

	reg.Register("a1", "host-a", "10.0.0.1", nil)
	reg.UpdateCapabilities("a1", []string{"screen.capture"})
	// 上报空集清空能力（等价于 agent 重启后不再支持任何能力）
	reg.UpdateCapabilities("a1", []string{})

	// 模拟陈旧 DB 行：store 仍持有旧值（如清空落库失败或重启竞态）；
	// 重连应保留内存空集而非回填旧值
	store.load["a1"] = []string{"screen.capture"}
	reg.Register("a1", "host-a", "10.0.0.1", nil)
	info, _ := reg.Get("a1")
	if len(info.Capabilities) != 0 {
		t.Errorf("reconnect should keep memory value, got %v", info.Capabilities)
	}
}

// ToJSON 未上报能力时给出空数组（而非 nil），stable JSON 序列化。
func TestToJSONCapabilitiesDefaultsEmpty(t *testing.T) {
	reg := newTestRegistryCaps(t)
	reg.Register("a1", "host-a", "10.0.0.1", nil)

	info, _ := reg.Get("a1")
	view := info.ToJSON()
	caps, _ := view["capabilities"].([]string)
	unknown, _ := view["unknownCapabilities"].([]string)
	if caps == nil || len(caps) != 0 {
		t.Errorf("capabilities should default to empty slice, got %v", caps)
	}
	if unknown == nil || len(unknown) != 0 {
		t.Errorf("unknownCapabilities should default to empty slice, got %v", unknown)
	}
}

// handleRegister 应把 agent.register 携带的 capabilities 透传给 Registry；
// 字段缺失时透传空集，不阻塞注册。
func TestHandleRegisterCapabilities(t *testing.T) {
	reg := newRecordingRegistry()
	_, addr := startTestListener(t, reg, &mockBroadcaster{})

	conn, err := net.Dial("tcp", addr)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	defer conn.Close()
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "agent.register", "agentId": "cap-agent", "hostname": "h",
		"capabilities": []any{"screen.capture", "ml.onnx"},
	})
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	ack := readFrame(t, conn)
	var payload map[string]any
	if err := json.Unmarshal(ack.body, &payload); err != nil {
		t.Fatalf("register ack: %v", err)
	}
	if payload["type"] != "agent.register_ack" || payload["success"] != true {
		t.Fatalf("expected successful register_ack, got %v", payload)
	}

	reg.mu.Lock()
	calls := append([][]string(nil), reg.capabilities...)
	reg.mu.Unlock()
	if len(calls) != 1 {
		t.Fatalf("expected 1 capabilities record, got %d", len(calls))
	}
	want := []string{"screen.capture", "ml.onnx"}
	if !reflect.DeepEqual(calls[0], want) {
		t.Errorf("forwarded capabilities = %v, want %v", calls[0], want)
	}
}
