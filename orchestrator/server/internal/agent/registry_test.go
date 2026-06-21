package agent

import (
	"testing"
	"time"

	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
)

func newTestRegistry(t *testing.T) (*Registry, *ws.Hub) {
	t.Helper()
	hub := ws.NewHub()
	go hub.Run()
	return NewRegistry(hub), hub
}

func TestRegistryRegisterAndList(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)
	reg.Register("a2", "host2", "10.0.0.2", nil)

	agents := reg.List()
	if len(agents) != 2 {
		t.Fatalf("expected 2 agents, got %d", len(agents))
	}

	info, ok := reg.Get("a1")
	if !ok || info.Hostname != "host1" || info.Status != StatusOnline {
		t.Errorf("unexpected agent info: %+v ok=%v", info, ok)
	}

	if _, ok := reg.Get("ghost"); ok {
		t.Error("ghost agent should not exist")
	}
}

func TestRegistryUpdateStatus(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	reg.UpdateStatus("a1", string(StatusBusy), ResourceStats{
		CPU: CPUStats{Usage: 42.5, Cores: 8},
	})

	info, _ := reg.Get("a1")
	if info.Status != StatusBusy {
		t.Errorf("expected busy, got %s", info.Status)
	}
	if info.Resources.CPU.Usage != 42.5 {
		t.Errorf("expected cpu usage 42.5, got %v", info.Resources.CPU.Usage)
	}
	if info.LastSeen.IsZero() {
		t.Error("LastSeen should be updated")
	}
}

func TestRegistryHeartbeatTimeout(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	// 将心跳阈值缩短，模拟超时
	reg.mu.Lock()
	reg.heartbeat = 50 * time.Millisecond
	reg.agents["a1"].LastSeen = time.Now().Add(-1 * time.Second) // 1 秒前
	reg.mu.Unlock()

	reg.checkHeartbeats()

	info, _ := reg.Get("a1")
	if info.Status != StatusOffline {
		t.Errorf("expected agent offline after heartbeat timeout, got %s", info.Status)
	}
}

func TestRegistryHeartbeatFreshNotTimedOut(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	reg.UpdateHeartbeat("a1") // 刚心跳

	reg.mu.Lock()
	reg.heartbeat = 1 * time.Second
	reg.mu.Unlock()

	reg.checkHeartbeats()

	info, _ := reg.Get("a1")
	if info.Status == StatusOffline {
		t.Error("fresh agent should not be marked offline")
	}
}

func TestRegistryStopIsIdempotent(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Stop()
	reg.Stop() // 不应 panic（重复 close 由 stopOnce 保护）
}

func TestRegistrySetTags(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("a1", "host1", "10.0.0.1", nil)

	if !reg.SetTags("a1", []string{" prod ", "gpu", "gpu", ""}) {
		t.Fatal("SetTags should succeed for existing agent")
	}
	info, _ := reg.Get("a1")
	// 去重 + 去空白：prod, gpu
	if len(info.Tags) != 2 || info.Tags[0] != "prod" || info.Tags[1] != "gpu" {
		t.Errorf("unexpected tags: %+v", info.Tags)
	}

	// ToJSON 应包含 tags（非 nil）
	j := info.ToJSON()
	tags, ok := j["tags"].([]string)
	if !ok || len(tags) != 2 {
		t.Errorf("ToJSON tags missing/wrong: %+v", j["tags"])
	}

	// 不存在的 agent
	if reg.SetTags("ghost", []string{"x"}) {
		t.Error("SetTags should fail for unknown agent")
	}
}
