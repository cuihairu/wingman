package websocket

import (
	"testing"
	"time"
)

// ConnectionCount 应反映 hub 当前活跃连接数（供指标端点采样）。
func TestConnectionCountTracksRegistrations(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	if hub.ConnectionCount() != 0 {
		t.Fatalf("fresh hub should report 0, got %d", hub.ConnectionCount())
	}

	c := makeTestConn(hub, "cc1")
	hub.register <- c
	if !waitFor(t, time.Second, func() bool {
		return hub.ConnectionCount() == 1
	}) {
		t.Fatal("connection count should be 1 after register")
	}

	hub.unregister <- c
	if !waitFor(t, time.Second, func() bool {
		return hub.ConnectionCount() == 0
	}) {
		t.Fatal("connection count should be 0 after unregister")
	}
}
