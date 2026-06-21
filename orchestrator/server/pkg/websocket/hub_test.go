package websocket

import (
	"testing"
	"time"
)

func makeTestConn(hub *Hub, id string) *Connection {
	return &Connection{
		ID:   id,
		Send: make(chan *Message, 16),
		Hub:  hub,
	}
}

// waitFor 持续调用 fn 直到返回 true 或超时，避免测试中硬编码 sleep 造成 flaky。
func waitFor(t *testing.T, timeout time.Duration, fn func() bool) bool {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if fn() {
			return true
		}
		time.Sleep(5 * time.Millisecond)
	}
	return fn()
}

func TestHubRegisterAndBroadcast(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c1 := makeTestConn(hub, "c1")
	c2 := makeTestConn(hub, "c2")
	hub.register <- c1
	hub.register <- c2

	if !waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 2
	}) {
		t.Fatal("expected 2 connections registered")
	}

	hub.BroadcastEvent("test", map[string]any{"x": 1})

	for _, c := range []*Connection{c1, c2} {
		select {
		case msg := <-c.Send:
			if msg.Type != "test" {
				t.Errorf("expected type test, got %s", msg.Type)
			}
		case <-time.After(time.Second):
			t.Fatal("timeout waiting for broadcast on", c.ID)
		}
	}
}

func TestHubUnregister(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c1 := makeTestConn(hub, "c1")
	hub.register <- c1
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})

	hub.unregister <- c1
	if !waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 0
	}) {
		t.Fatal("expected connection removed after unregister")
	}
}

func TestHubRooms(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	inRoom := makeTestConn(hub, "in")
	outRoom := makeTestConn(hub, "out")
	hub.register <- inRoom
	hub.register <- outRoom
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 2
	})

	hub.JoinRoom("in", "alpha")
	if !waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.rooms["alpha"]) == 1
	}) {
		t.Fatal("expected 1 member in room alpha")
	}

	hub.BroadcastToRoom("alpha", &Message{Type: "roomcast"})

	select {
	case msg := <-inRoom.Send:
		if msg.Type != "roomcast" {
			t.Errorf("in-room conn expected roomcast, got %s", msg.Type)
		}
	case <-time.After(time.Second):
		t.Fatal("in-room conn did not receive roomcast")
	}

	// outRoom 不应收到
	select {
	case msg := <-outRoom.Send:
		t.Errorf("out-room conn should not receive, got %+v", msg)
	default:
	}
}

func TestHubBroadcastAgentEventShape(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})

	hub.BroadcastAgentEvent("connected", map[string]any{"agentId": "a1"})

	select {
	case msg := <-c.Send:
		if msg.Type != "agent" || msg.Event != "connected" {
			t.Errorf("unexpected agent event: type=%s event=%s", msg.Type, msg.Event)
		}
	case <-time.After(time.Second):
		t.Fatal("timeout waiting for agent event")
	}
}
