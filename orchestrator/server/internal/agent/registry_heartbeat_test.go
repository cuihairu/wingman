package agent

import (
	"testing"
	"time"
)

// 覆盖集成测试辅助方法 SetHeartbeatTimeout / CheckHeartbeatsNow。
func TestRegistryHeartbeatHelpers(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("hb-a", "host", "10.0.0.1", nil)
	reg.Register("hb-fresh", "host", "10.0.0.2", nil)

	// 非正数应被忽略
	original := 90 * time.Second
	before := time.Duration(0)
	reg.mu.RLock()
	before = reg.heartbeat
	reg.mu.RUnlock()
	_ = original
	reg.SetHeartbeatTimeout(0)
	reg.SetHeartbeatTimeout(-time.Second)
	reg.mu.RLock()
	if reg.heartbeat != before {
		t.Errorf("non-positive timeout should be ignored: %v", reg.heartbeat)
	}
	reg.mu.RUnlock()

	// 缩短超时窗口
	reg.SetHeartbeatTimeout(50 * time.Millisecond)
	reg.mu.RLock()
	if reg.heartbeat != 50*time.Millisecond {
		t.Errorf("timeout not applied: %v", reg.heartbeat)
	}
	reg.mu.RUnlock()

	// 把 hb-a 的 LastSeen 拨回过去 → CheckHeartbeatsNow 应将其置 offline，
	// 而 hb-fresh（刚注册）保持 online
	reg.mu.Lock()
	if info, ok := reg.agents["hb-a"]; ok {
		info.LastSeen = time.Now().Add(-time.Second)
	}
	reg.mu.Unlock()

	reg.CheckHeartbeatsNow()

	info, ok := reg.Get("hb-a")
	if !ok {
		t.Fatal("agent should still exist after timeout")
	}
	if info.Status != StatusOffline {
		t.Errorf("stale agent should be offline, got %s", info.Status)
	}
	info, ok = reg.Get("hb-fresh")
	if !ok || info.Status == StatusOffline {
		t.Errorf("fresh agent should stay online, got %+v", info)
	}

	// 已 offline 的 agent 不会被重复处理
	reg.CheckHeartbeatsNow()
	if info, _ := reg.Get("hb-a"); info.Status != StatusOffline {
		t.Errorf("offline agent should stay offline, got %s", info.Status)
	}
}
