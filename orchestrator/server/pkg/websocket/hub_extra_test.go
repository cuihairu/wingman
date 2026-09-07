package websocket

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

const wsTestSecret = "0123456789abcdef0123456789abcdef"

// newGinContext 构造仅含 Request/Writer 的最小 gin.Context，供 HandleWebSocket 使用。
func newGinContext(w http.ResponseWriter, req *http.Request) *gin.Context {
	gin.SetMode(gin.TestMode)
	c, _ := gin.CreateTestContext(w)
	c.Request = req
	return c
}

// ---------- 房间与广播 ----------

func TestLeaveRoom(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c1")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})

	hub.JoinRoom("c1", "alpha")
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.rooms["alpha"]) == 1
	})

	hub.LeaveRoom("c1")
	hub.mu.RLock()
	_, roomExists := hub.rooms["alpha"]
	hub.mu.RUnlock()
	if roomExists {
		t.Error("empty room should be removed after leave")
	}

	// 重复 leave / 未知连接不应 panic
	hub.LeaveRoom("c1")
	hub.LeaveRoom("ghost")
}

func TestJoinRoomSwitchesRooms(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c1")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})

	hub.JoinRoom("c1", "one")
	hub.JoinRoom("c1", "two")
	hub.mu.RLock()
	_, inOne := hub.rooms["one"]["c1"]
	inTwo := len(hub.rooms["two"]) == 1
	hub.mu.RUnlock()
	if inOne {
		t.Error("old room membership should be removed")
	}
	if !inTwo {
		t.Error("new room membership should exist")
	}

	// 未知连接 join 无效果
	hub.JoinRoom("ghost", "three")
}

func TestBroadcastToRoomWithFullChannelDoesNotBlock(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c1")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})
	hub.JoinRoom("c1", "alpha")

	// 排空 Send：容量 16，灌 20 条不应阻塞
	done := make(chan struct{})
	go func() {
		defer close(done)
		for i := 0; i < 20; i++ {
			hub.BroadcastToRoom("alpha", &Message{Type: "flood"})
		}
	}()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		t.Fatal("BroadcastToRoom blocked on full channel")
	}

	// 未知房间广播无效果
	hub.BroadcastToRoom("nope", &Message{Type: "x"})
}

func TestBroadcastDebugEventShape(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})

	hub.BroadcastDebugEvent("breakpoint_hit", map[string]any{"file": "a.lua"})
	select {
	case msg := <-c.Send:
		if msg.Type != "debugger" || msg.Event != "breakpoint_hit" {
			t.Errorf("unexpected debug event: %+v", msg)
		}
	case <-time.After(time.Second):
		t.Fatal("timeout waiting for debug event")
	}
}

func TestBroadcastDropsStaleConnections(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	healthy := makeTestConn(hub, "healthy")
	stale := &Connection{ID: "stale", Send: make(chan *Message), Hub: hub} // 无缓冲且无读者
	hub.register <- healthy
	hub.register <- stale
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 2
	})
	hub.JoinRoom("stale", "stale-room") // 房间成员被清理时也应移除

	hub.BroadcastEvent("ping", nil)

	select {
	case msg := <-healthy.Send:
		if msg.Type != "ping" {
			t.Errorf("healthy conn expected ping, got %s", msg.Type)
		}
	case <-time.After(time.Second):
		t.Fatal("healthy conn did not receive broadcast")
	}

	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		_, ok := hub.connections["stale"]
		return !ok
	}) {
		t.Fatal("stale connection should be removed")
	}
	hub.mu.RLock()
	_, roomStillThere := hub.rooms["stale-room"]
	hub.mu.RUnlock()
	if roomStillThere {
		t.Error("stale member should be removed from its room")
	}

	// stale 的 Send 应被关闭
	if _, open := <-stale.Send; open {
		t.Error("stale conn Send should be closed")
	}
}

func TestUnregisterCleansRoomMembership(t *testing.T) {
	hub := NewHub()
	go hub.Run()

	c := makeTestConn(hub, "c1")
	hub.register <- c
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	})
	hub.JoinRoom("c1", "alpha")
	waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.rooms["alpha"]) == 1
	})

	hub.unregister <- c
	if !waitFor(t, time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 0
	}) {
		t.Fatal("connection not removed")
	}
	hub.mu.RLock()
	_, roomExists := hub.rooms["alpha"]
	hub.mu.RUnlock()
	if roomExists {
		t.Error("room should be cleaned when last member disconnects")
	}
}

// ---------- CheckOrigin ----------

func TestUpgraderCheckOrigin(t *testing.T) {
	mkReq := func(origin, host string) *http.Request {
		r := httptest.NewRequest("GET", "/ws", nil)
		r.Host = host
		if origin != "" {
			r.Header.Set("Origin", origin)
		}
		return r
	}

	cases := []struct {
		name   string
		origin string
		host   string
		want   bool
	}{
		{"no origin allowed", "", "h", true},
		{"same host allowed", "http://h", "h", true},
		{"cross host denied", "http://evil", "h", false},
		{"empty host denied", "http://x", "", false},
		{"invalid url denied", "://bad", "h", false},
		{"scheme included", "https://h:443", "h", false},
	}
	for _, tc := range cases {
		if got := upgrader.CheckOrigin(mkReq(tc.origin, tc.host)); got != tc.want {
			t.Errorf("%s: CheckOrigin(origin=%q, host=%q) = %v, want %v", tc.name, tc.origin, tc.host, got, tc.want)
		}
	}
}

// ---------- HandleWebSocket 端到端 ----------

func setupWSServer(t *testing.T) (*httptest.Server, *Hub) {
	t.Helper()
	t.Setenv("WINGMAN_JWT_SECRET", wsTestSecret)
	hub := NewHub()
	go hub.Run()

	r := http.NewServeMux()
	r.HandleFunc("/ws", func(w http.ResponseWriter, req *http.Request) {
		// 适配 gin.Context 所需的最小接口：HandleWebSocket 只用 Query/GetHeader/Writer/Request
		fake := newGinContext(w, req)
		HandleWebSocket(fake, hub)
	})
	server := httptest.NewServer(r)
	t.Cleanup(server.Close)
	return server, hub
}

func dialWS(t *testing.T, url string) *websocket.Conn {
	t.Helper()
	dialer := &websocket.Dialer{HandshakeTimeout: 2 * time.Second}
	conn, _, err := dialer.Dial(url, nil)
	if err != nil {
		t.Fatalf("dial %s: %v", url, err)
	}
	return conn
}

func TestHandleWebSocketRejectsUnauthorized(t *testing.T) {
	server, hub := setupWSServer(t)
	_ = hub

	// 无 token → 401（Dial 失败）
	dialer := &websocket.Dialer{HandshakeTimeout: 2 * time.Second}
	if _, resp, err := dialer.Dial(strings.Replace(server.URL, "http", "ws", 1)+"/ws", nil); err == nil {
		t.Fatal("dial without token should fail")
	} else if resp != nil && resp.StatusCode != http.StatusUnauthorized {
		t.Errorf("expected 401, got %d", resp.StatusCode)
	}

	// 无效 token → 401
	if _, resp, err := dialer.Dial(strings.Replace(server.URL, "http", "ws", 1)+"/ws?token=garbage", nil); err == nil {
		t.Fatal("dial with garbage token should fail")
	} else if resp != nil && resp.StatusCode != http.StatusUnauthorized {
		t.Errorf("expected 401 for garbage token, got %d", resp.StatusCode)
	}
}

func TestHandleWebSocketEndToEnd(t *testing.T) {
	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(7, "wsuser", "admin")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws?token=" + token

	conn := dialWS(t, wsURL)
	defer conn.Close()
	conn.SetReadDeadline(time.Now().Add(5 * time.Second))

	// 1. 欢迎消息
	var welcome Message
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatalf("read welcome: %v", err)
	}
	if welcome.Type != "connected" {
		t.Fatalf("expected connected welcome, got %+v", welcome)
	}

	// 等待注册完成
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("connection not registered in hub")
	}

	// 2. join_room
	if err := conn.WriteJSON(map[string]any{"type": "join_room", "roomId": "room1"}); err != nil {
		t.Fatalf("write join_room: %v", err)
	}
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.rooms["room1"]) == 1
	}) {
		t.Fatal("join_room not processed")
	}

	// 3. room_message：自己应在房间内收到回声
	if err := conn.WriteJSON(map[string]any{"type": "room_message", "roomId": "room1", "data": "hi"}); err != nil {
		t.Fatalf("write room_message: %v", err)
	}
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	var roomMsg Message
	if err := conn.ReadJSON(&roomMsg); err != nil {
		t.Fatalf("read room echo: %v", err)
	}
	if roomMsg.Type != "room" || roomMsg.RoomID != "room1" {
		t.Errorf("unexpected room echo: %+v", roomMsg)
	}

	// 4. 服务器广播（BroadcastEvent → writePump）
	hub.BroadcastEvent("agent", map[string]any{"x": 1})
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	var broadcast Message
	if err := conn.ReadJSON(&broadcast); err != nil {
		t.Fatalf("read broadcast: %v", err)
	}
	if broadcast.Type != "agent" {
		t.Errorf("expected agent broadcast, got %+v", broadcast)
	}

	// 5. pong 控制帧 → SetPongHandler 回调
	if err := conn.WriteControl(websocket.PongMessage, nil, time.Now().Add(time.Second)); err != nil {
		t.Fatalf("write pong control frame: %v", err)
	}

	// 6. 应用层 pong 消息
	if err := conn.WriteJSON(map[string]any{"type": "pong"}); err != nil {
		t.Fatalf("write pong: %v", err)
	}

	// 7. 非法 JSON：连接应保持（仅记录错误）
	if err := conn.WriteMessage(websocket.TextMessage, []byte("{invalid")); err != nil {
		t.Fatalf("write invalid json: %v", err)
	}

	// 8. 未知消息类型：忽略
	if err := conn.WriteJSON(map[string]any{"type": "mystery"}); err != nil {
		t.Fatalf("write unknown type: %v", err)
	}

	// 9. 服务器广播（BroadcastEvent → writePump），含不可序列化数据 → writePump 容错
	hub.BroadcastEvent("agent", map[string]any{"x": 2})
	hub.BroadcastEvent("unserializable", map[string]any{"ch": make(chan int)})
	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	var broadcast2 Message
	if err := conn.ReadJSON(&broadcast2); err != nil {
		t.Fatalf("read broadcast: %v", err)
	}
	if broadcast2.Type != "agent" {
		t.Errorf("expected agent broadcast, got %+v", broadcast2)
	}

	// 10. leave_room
	if err := conn.WriteJSON(map[string]any{"type": "leave_room"}); err != nil {
		t.Fatalf("write leave_room: %v", err)
	}
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		_, exists := hub.rooms["room1"]
		return !exists
	}) {
		t.Fatal("leave_room not processed")
	}

	// 11. 强制写失败：关闭服务端底层连接后广播 → writePump WriteMessage 出错退出
	hub.mu.RLock()
	var serverConn *Connection
	for _, c := range hub.connections {
		serverConn = c
	}
	hub.mu.RUnlock()
	if serverConn == nil {
		t.Fatal("server connection not found")
	}
	serverConn.Conn.Close()
	hub.BroadcastEvent("post-mortem", nil)

	// 12. 客户端异常断开（无关闭帧）→ readPump 走 abnormal closure 分支 + unregister
	conn.UnderlyingConn().Close()
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 0
	}) {
		t.Fatal("connection not unregistered after close")
	}
}

func TestHandleWebSocketInvalidTokenReturns401(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", wsTestSecret)
	hub := NewHub()
	go hub.Run()

	// 直接调用：token 存在但非法 → 401（在 upgrade 之前返回）
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/ws?token=garbage", nil)
	HandleWebSocket(newGinContext(w, req), hub)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("expected 401, got %d %s", w.Code, w.Body.String())
	}

	// Authorization 头里的非法 token 同样 401
	w = httptest.NewRecorder()
	req = httptest.NewRequest("GET", "/ws", nil)
	req.Header.Set("Authorization", "Bearer not-a-jwt")
	HandleWebSocket(newGinContext(w, req), hub)
	if w.Code != http.StatusUnauthorized {
		t.Fatalf("header auth: expected 401, got %d", w.Code)
	}
}

func TestHandleWebSocketUpgradeFailure(t *testing.T) {
	t.Setenv("WINGMAN_JWT_SECRET", wsTestSecret)
	hub := NewHub()
	go hub.Run()

	token, err := middleware.GenerateToken(10, "upgrader", "admin")
	if err != nil {
		t.Fatal(err)
	}

	// httptest.ResponseRecorder 未实现 http.Hijacker → Upgrade 失败（记录日志并返回）
	w := httptest.NewRecorder()
	req := httptest.NewRequest("GET", "/ws?token="+token, nil)
	HandleWebSocket(newGinContext(w, req), hub)

	hub.mu.RLock()
	count := len(hub.connections)
	hub.mu.RUnlock()
	if count != 0 {
		t.Errorf("failed upgrade must not register a connection, got %d", count)
	}
}

func TestWritePumpPingAndWriteError(t *testing.T) {
	orig := pingInterval
	pingInterval = 40 * time.Millisecond
	defer func() { pingInterval = orig }()

	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(11, "pinger", "admin")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws?token=" + token

	alive := dialWS(t, wsURL)
	defer alive.Close()
	dead := dialWS(t, wsURL)

	var welcome Message
	if err := alive.ReadJSON(&welcome); err != nil {
		t.Fatalf("welcome: %v", err)
	}
	if err := dead.ReadJSON(&welcome); err != nil {
		t.Fatalf("welcome: %v", err)
	}

	// dead 客户端发送“非常规关闭码”的 close 帧后断开 → 服务端记录 unexpected close
	dead.WriteControl(websocket.CloseMessage,
		websocket.FormatCloseMessage(websocket.CloseNormalClosure, "bye"),
		time.Now().Add(time.Second))
	dead.Close()

	// alive 客户端持续读取（控制帧仅在读取期间被分发），用 PingHandler 观察 ping
	pingCh := make(chan struct{}, 8)
	alive.SetPingHandler(func(string) error {
		pingCh <- struct{}{}
		return nil
	})
	go func() {
		for {
			if _, _, err := alive.ReadMessage(); err != nil {
				return
			}
		}
	}()
	select {
	case <-pingCh:
	case <-time.After(3 * time.Second):
		t.Fatal("expected ping control frame within window")
	}

	// 等待 dead 连接被注销
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("dead connection should be unregistered")
	}
}

func TestWritePumpWriteErrorOnClosedConn(t *testing.T) {
	orig := pingInterval
	pingInterval = 30 * time.Second // 避免 ping 干扰
	defer func() { pingInterval = orig }()

	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(12, "wfail", "admin")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws?token=" + token

	conn := dialWS(t, wsURL)
	defer conn.Close()
	var welcome Message
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatalf("welcome: %v", err)
	}
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("connection not registered")
	}

	// 先从 hub 摘除（Run 不再 close(Send)），再关闭底层连接并推送消息：
	// writePump 收到消息后在已关闭的 conn 上 WriteMessage 失败退出。
	serverConn := takeServerConn(t, hub)
	if serverConn == nil {
		t.Fatal("no server connection found")
	}
	serverConn.Conn.Close()
	select {
	case serverConn.Send <- &Message{Type: "post-mortem"}:
	default:
		t.Fatal("expected Send to accept message")
	}

	// 等待 writePump 消费（发送方无回执，等待底层关闭完成即可）
	time.Sleep(100 * time.Millisecond)
	conn.Close()
}

// takeServerConn 从 hub 中取出（并摘除）当前连接，避免 Run 后续 close(Send) 干扰。
func takeServerConn(t *testing.T, hub *Hub) *Connection {
	t.Helper()
	hub.mu.Lock()
	defer hub.mu.Unlock()
	for id, c := range hub.connections {
		delete(hub.connections, id)
		return c
	}
	return nil
}

func TestWritePumpCloseFrameOnClosedSend(t *testing.T) {
	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(13, "closer", "admin")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws?token=" + token

	conn := dialWS(t, wsURL)
	defer conn.Close()
	var welcome Message
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatalf("welcome: %v", err)
	}
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("connection not registered")
	}

	serverConn := takeServerConn(t, hub)
	if serverConn == nil {
		t.Fatal("no server connection found")
	}

	// 关闭 Send → writePump 通过 !ok 分支向健康的客户端发送 close 帧
	close(serverConn.Send)

	conn.SetReadDeadline(time.Now().Add(3 * time.Second))
	sawClose := false
	for i := 0; i < 16; i++ {
		mt, _, err := conn.ReadMessage()
		if err != nil {
			if websocket.IsCloseError(err, websocket.CloseNoStatusReceived) || websocket.IsCloseError(err, websocket.CloseNormalClosure) || websocket.IsCloseError(err, websocket.CloseGoingAway) {
				sawClose = true
			}
			break
		}
		_ = mt
	}
	if !sawClose {
		t.Errorf("expected close frame after Send closed (err path may vary)")
	}
}

func TestWritePumpPingWriteError(t *testing.T) {
	orig := pingInterval
	pingInterval = 40 * time.Millisecond
	defer func() { pingInterval = orig }()

	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(14, "deadping", "admin")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws?token=" + token

	conn := dialWS(t, wsURL)
	defer conn.Close()
	var welcome Message
	if err := conn.ReadJSON(&welcome); err != nil {
		t.Fatalf("welcome: %v", err)
	}
	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("connection not registered")
	}

	// 先从 hub 摘除（Run 不再 close(Send)，writePump 只能经 ping 写失败退出）
	serverConn := takeServerConn(t, hub)
	if serverConn == nil {
		t.Fatal("no server connection found")
	}
	serverConn.Conn.Close()

	// 客户端随后断开；等待一小段确保 ping tick 已触发
	time.Sleep(150 * time.Millisecond)
	conn.Close()
}

func TestHandleWebSocketTokenViaAuthorizationHeader(t *testing.T) {
	server, hub := setupWSServer(t)

	token, err := middleware.GenerateToken(9, "headeruser", "viewer")
	if err != nil {
		t.Fatal(err)
	}
	wsURL := strings.Replace(server.URL, "http", "ws", 1) + "/ws"

	dialer := &websocket.Dialer{HandshakeTimeout: 2 * time.Second}
	conn, resp, err := dialer.Dial(wsURL, http.Header{"Authorization": []string{"Bearer " + token}})
	if err != nil {
		status := 0
		if resp != nil {
			status = resp.StatusCode
		}
		t.Fatalf("dial with bearer token failed: %v (status %d)", err, status)
	}
	defer conn.Close()
	conn.SetReadDeadline(time.Now().Add(5 * time.Second))

	var welcome Message
	if err := conn.ReadJSON(&welcome); err != nil || welcome.Type != "connected" {
		t.Fatalf("welcome via header auth: err=%v msg=%+v", err, welcome)
	}

	if !waitFor(t, 2*time.Second, func() bool {
		hub.mu.RLock()
		defer hub.mu.RUnlock()
		return len(hub.connections) == 1
	}) {
		t.Fatal("header-auth connection not registered")
	}
}
