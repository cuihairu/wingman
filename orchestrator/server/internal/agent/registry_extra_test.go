package agent

import (
	"sync"
	"testing"
	"time"
)

// fakeConn 满足 AgentConn 接口，用于注册表测试。
type fakeConn struct{ id string }

func (f *fakeConn) SendCommand(method string, data map[string]any) (map[string]any, error) {
	return map[string]any{"success": true, "from": f.id}, nil
}
func (f *fakeConn) SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error) {
	return map[string]any{"success": true, "from": f.id}, nil
}

func TestRegistryRegisterPreservesResourcesOnReRegister(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)
	reg.UpdateStatus("a1", "busy", ResourceStats{CPU: CPUStats{Usage: 88}})

	// 重复注册应保留既有资源数据
	reg.Register("a1", "host-new", "10.0.0.9", nil)
	info, ok := reg.Get("a1")
	if !ok {
		t.Fatal("agent should exist")
	}
	if info.Resources.CPU.Usage != 88 {
		t.Errorf("resources should be preserved on re-register, got %+v", info.Resources)
	}
	if info.Hostname != "host-new" {
		t.Errorf("hostname should be updated, got %s", info.Hostname)
	}
	if info.Status != StatusOnline {
		t.Errorf("re-registered agent should be online, got %s", info.Status)
	}
}

func TestRegistryRegisterWithClientConn(t *testing.T) {
	reg, _ := newTestRegistry(t)
	conn := &fakeConn{id: "c1"}
	reg.Register("a1", "host1", "10.0.0.1", conn)

	got, ok := reg.GetClient("a1")
	if !ok || got == nil {
		t.Fatal("client conn should be stored")
	}
	// 非 AgentConn 类型的 conn 不应被保存
	reg.Register("a2", "host2", "10.0.0.2", "not-a-conn")
	if _, ok := reg.GetClient("a2"); ok {
		t.Error("non-AgentConn should not be stored as client")
	}
}

func TestRegistryUnregister(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	reg.Unregister("a1")
	info, _ := reg.Get("a1")
	if info.Status != StatusOffline {
		t.Errorf("unregistered agent should be offline, got %s", info.Status)
	}

	// 注销未知 agent 不应 panic
	reg.Unregister("ghost")
}

func TestRegistrySetClientVariants(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	conn := &fakeConn{id: "late"}
	reg.SetClient("a1", conn)
	if got, ok := reg.GetClient("a1"); !ok || got == nil {
		t.Fatal("SetClient should store the conn")
	}

	// 未知 agent / 非 AgentConn 类型
	reg.SetClient("ghost", conn)
	reg.SetClient("a1", 12345)
}

func TestRegistryGetClientVariants(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("noconn", "h", "ip", nil)

	if _, ok := reg.GetClient("noconn"); ok {
		t.Error("agent without client should not resolve")
	}
	if _, ok := reg.GetClient("ghost"); ok {
		t.Error("unknown agent should not resolve")
	}
}

func TestRegistryUpdateStatusVariants(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	// 空 status 保持不变；resources 为 map 也能解析
	reg.UpdateStatus("a1", "", map[string]any{
		"cpu": map[string]any{"usage": 12.5, "cores": 4, "model": "m1"},
	})
	info, _ := reg.Get("a1")
	if info.Status != StatusOnline {
		t.Errorf("empty status should keep current, got %s", info.Status)
	}
	if info.Resources.CPU.Usage != 12.5 || info.Resources.CPU.Cores != 4 {
		t.Errorf("map resources not parsed: %+v", info.Resources)
	}

	// 不可序列化的 resources 被忽略
	reg.UpdateStatus("a1", "idle", make(chan int))
	info, _ = reg.Get("a1")
	if info.Status != StatusIdle {
		t.Errorf("status should still update, got %s", info.Status)
	}

	// 未知 agent 静默忽略
	reg.UpdateStatus("ghost", "online", nil)
}

func TestRegistryUpdateHeartbeatUnknownAgent(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.UpdateHeartbeat("ghost") // 不应 panic
}

func TestRegistryListReturnsCopies(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	list := reg.List()
	list[0].Hostname = "mutated"

	info, _ := reg.Get("a1")
	if info.Hostname != "host1" {
		t.Error("List should return copies, mutation leaked to registry")
	}
}

func TestRegistryToJSONNilTags(t *testing.T) {
	info := &AgentInfo{AgentID: "a1", Hostname: "h", IP: "1.2.3.4", Status: StatusOnline}
	j := info.ToJSON()
	tags, ok := j["tags"].([]string)
	if !ok || len(tags) != 0 {
		t.Errorf("nil tags should serialize as empty array, got %#v", j["tags"])
	}
	if j["currentTask"] != "" {
		t.Errorf("currentTask should default empty, got %v", j["currentTask"])
	}
}

func TestRegistryStartHeartbeatCheckStops(t *testing.T) {
	reg, _ := newTestRegistry(t)
	done := make(chan struct{})
	go func() {
		reg.StartHeartbeatCheck()
		close(done)
	}()
	reg.Stop()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("StartHeartbeatCheck did not stop")
	}
	// 重复 Stop 幂等
	reg.Stop()
}

func TestRegistryConcurrentAccess(t *testing.T) {
	reg, _ := newTestRegistry(t)
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			id := "agent-" + string(rune('a'+i))
			reg.Register(id, "h", "ip", nil)
			reg.UpdateStatus(id, "busy", nil)
			reg.UpdateHeartbeat(id)
			reg.List()
			reg.Get(id)
			reg.Unregister(id)
		}(i)
	}
	wg.Wait()
}
