package agent

import (
	"encoding/json"
	"testing"
	"time"
)

// TestRegistryUpdateLinkHealth 覆盖 agent.heartbeat 携带的链路统计解析：
// 各数值类型（float64/int/int64/json.Number）、字符串字段、无效输入保持原值。
func TestRegistryUpdateLinkHealth(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("link-a", "host", "10.0.0.1", nil)

	// nil map 与未知 agent：不生效、不 panic
	reg.UpdateLinkHealth("link-a", nil)
	reg.UpdateLinkHealth("ghost", map[string]any{"reconnects": 1.0})
	info, _ := reg.Get("link-a")
	if info.Link != (LinkHealth{}) {
		t.Errorf("nil/unknown updates should be no-ops, got %+v", info.Link)
	}

	reg.UpdateLinkHealth("link-a", map[string]any{
		"reconnects":           3.0, // float64（JSON 数字默认类型）
		"dropped":              2,   // int
		"outboxPending":        int64(7),
		"lastDisconnectReason": "network reset",
		"sessionUptimeMs":      json.Number("123456"),
	})
	info, _ = reg.Get("link-a")
	if info.Link.Reconnects != 3 {
		t.Errorf("reconnects: got %d want 3", info.Link.Reconnects)
	}
	if info.Link.Dropped != 2 {
		t.Errorf("dropped: got %d want 2", info.Link.Dropped)
	}
	if info.Link.OutboxPending != 7 {
		t.Errorf("outboxPending: got %d want 7", info.Link.OutboxPending)
	}
	if info.Link.LastDisconnectReason != "network reset" {
		t.Errorf("lastDisconnectReason: got %q", info.Link.LastDisconnectReason)
	}
	if info.Link.SessionUptimeMs != 123456 {
		t.Errorf("sessionUptimeMs: got %d want 123456", info.Link.SessionUptimeMs)
	}

	// 非数字 / 非法 json.Number：宽容解析，保持原值
	reg.UpdateLinkHealth("link-a", map[string]any{
		"reconnects":      "not-a-number",
		"dropped":         true,
		"outboxPending":   json.Number("oops"),
		"sessionUptimeMs": []any{1},
		// lastDisconnectReason 非字符串也保持原值
		"lastDisconnectReason": 42,
	})
	info, _ = reg.Get("link-a")
	if info.Link.Reconnects != 3 || info.Link.Dropped != 2 || info.Link.OutboxPending != 7 ||
		info.Link.SessionUptimeMs != 123456 || info.Link.LastDisconnectReason != "network reset" {
		t.Errorf("invalid types should keep previous values, got %+v", info.Link)
	}

	// ToJSON 应透出 link 统计与 server 实测 heartbeatAgeMs
	j := info.ToJSON()
	link, ok := j["link"].(map[string]any)
	if !ok {
		t.Fatalf("ToJSON link missing: %+v", j["link"])
	}
	if link["reconnects"] != 3 || link["lastDisconnectReason"] != "network reset" {
		t.Errorf("ToJSON link fields wrong: %+v", link)
	}
	if _, ok := link["heartbeatAgeMs"]; !ok {
		t.Error("ToJSON link should carry heartbeatAgeMs")
	}
}

// TestToIntAndToInt64 直接覆盖宽容整数解析的全部类型分支。
func TestToIntAndToInt64(t *testing.T) {
	cases := []struct {
		in     any
		want   int64
		wantOK bool
	}{
		{float64(12.9), 12, true},
		{int(-5), -5, true},
		{int64(9007199254740993), 9007199254740993, true},
		{json.Number("42"), 42, true},
		{json.Number("3.14"), 0, false},
		{json.Number(""), 0, false},
		{"42", 0, false},
		{true, 0, false},
		{nil, 0, false},
	}
	for _, tc := range cases {
		got, ok := toInt(tc.in)
		if ok != tc.wantOK || (ok && int64(got) != tc.want) {
			t.Errorf("toInt(%#v): got (%d,%v) want (%d,%v)", tc.in, got, ok, tc.want, tc.wantOK)
		}
		got64, ok := toInt64(tc.in)
		if ok != tc.wantOK || (ok && got64 != tc.want) {
			t.Errorf("toInt64(%#v): got (%d,%v) want (%d,%v)", tc.in, got64, ok, tc.want, tc.wantOK)
		}
	}
}

// TestStartHeartbeatCheckLoop 覆盖 StartHeartbeatCheck 的周期巡检分支：
// 缩短巡检间隔后，后台 goroutine 应将过期 agent 置 offline，随后 Stop 退出。
func TestStartHeartbeatCheckLoop(t *testing.T) {
	reg, _ := newTestRegistry(t)
	reg.Register("hb-loop", "host", "10.0.0.1", nil)

	reg.mu.Lock()
	reg.heartbeat = 50 * time.Millisecond
	reg.checkInterval = 15 * time.Millisecond
	reg.agents["hb-loop"].LastSeen = time.Now().Add(-time.Second)
	reg.mu.Unlock()

	go reg.StartHeartbeatCheck()
	t.Cleanup(reg.Stop)

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if info, _ := reg.Get("hb-loop"); info.Status == StatusOffline {
			reg.Stop() // 同时覆盖 stopCh 退出分支
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatal("periodic check should mark stale agent offline within 2s")
}

// TestStartHeartbeatCheckIntervalFallback 覆盖非正巡检周期的兜底分支（回退 30s）。
func TestStartHeartbeatCheckIntervalFallback(t *testing.T) {
	reg, _ := newTestRegistry(t)

	reg.mu.Lock()
	reg.checkInterval = -1 * time.Second // 非正 → 兜底为 30s
	reg.mu.Unlock()

	go reg.StartHeartbeatCheck()
	// 兜底分支执行后 goroutine 正常阻塞在 30s ticker / stopCh；Stop 退出
	time.Sleep(50 * time.Millisecond)
	reg.Stop()
}
