package agent

import (
	"testing"
)

// TestRegistryPlatformField agent.register 上报的平台标识经 UpdatePlatform
// 写入，ToJSON 归一导出（docs/android-agent-design.md §3.3）。
func TestRegistryPlatformField(t *testing.T) {
	reg, _ := newTestRegistry(t)

	// 旧版桌面 agent：不上报 → 归一为 desktop
	reg.Register("desktop-1", "win-pc", "10.0.0.1", nil)
	reg.UpdatePlatform("desktop-1", "")

	info, ok := reg.Get("desktop-1")
	if !ok {
		t.Fatal("desktop-1 should exist")
	}
	if got := info.ToJSON()["platform"]; got != "desktop" {
		t.Errorf("default platform: expected desktop, got %v", got)
	}

	// Android agent：显式上报 android
	reg.Register("android-1", "Pixel 8", "10.0.0.2", nil)
	reg.UpdatePlatform("android-1", "android")

	info, ok = reg.Get("android-1")
	if !ok {
		t.Fatal("android-1 should exist")
	}
	if info.Platform != "android" {
		t.Errorf("Platform field: expected android, got %q", info.Platform)
	}
	if got := info.ToJSON()["platform"]; got != "android" {
		t.Errorf("ToJSON platform: expected android, got %v", got)
	}

	// 未知 agent 的 UpdatePlatform 应静默（不 panic）
	reg.UpdatePlatform("ghost", "android")
}
