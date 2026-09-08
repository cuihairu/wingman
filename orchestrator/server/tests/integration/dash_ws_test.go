package integration

import (
	"encoding/json"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

// wsMsg 对应 Hub 广播的 Message JSON 结构。
type wsMsg struct {
	Type  string
	Event string
	Data  map[string]any
}

// dashClient 模拟 dashboard 浏览器客户端：带 JWT 连接 /ws，后台持续收消息。
type dashClient struct {
	t    *testing.T
	conn *websocket.Conn
	mu   sync.Mutex
	msgs []wsMsg
}

// newDashClient 用指定用户 token 建立 dashboard WebSocket 连接，并等待欢迎消息。
func newDashClient(t *testing.T, httpURL, token string) *dashClient {
	t.Helper()
	wsURL := strings.Replace(httpURL, "http://", "ws://", 1) + "/ws?token=" + token
	conn, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("dashboard ws dial: %v", err)
	}
	c := &dashClient{t: t, conn: conn}
	t.Cleanup(func() { conn.Close() })
	go c.readLoop()
	c.waitFor(3*time.Second, "connected 欢迎消息", func(m wsMsg) bool {
		return m.Type == "connected"
	})
	return c
}

func (c *dashClient) readLoop() {
	for {
		_, raw, err := c.conn.ReadMessage()
		if err != nil {
			return
		}
		var m wsMsg
		if err := json.Unmarshal(raw, &m); err != nil {
			continue
		}
		if m.Data == nil {
			m.Data = map[string]any{}
		}
		c.mu.Lock()
		c.msgs = append(c.msgs, m)
		c.mu.Unlock()
	}
}

func (c *dashClient) snapshot() []wsMsg {
	c.mu.Lock()
	defer c.mu.Unlock()
	out := make([]wsMsg, len(c.msgs))
	copy(out, c.msgs)
	return out
}

// waitFor 在历史消息中轮询匹配（历史保留，可断言早于当前时刻的事件）。
func (c *dashClient) waitFor(timeout time.Duration, desc string, match func(wsMsg) bool) wsMsg {
	c.t.Helper()
	var result *wsMsg
	waitUntil(c.t, timeout, "等待 WS 消息: "+desc, func() bool {
		msgs := c.snapshot()
		for i := range msgs {
			if match(msgs[i]) {
				result = &msgs[i]
				return true
			}
		}
		return false
	})
	if result == nil {
		c.t.Fatalf("WS 消息未到达: %s", desc)
	}
	return *result
}

// waitForAgentEvent 匹配 BroadcastAgentEvent 产生的 type=agent&event=<event> 消息。
func (c *dashClient) waitForAgentEvent(timeout time.Duration, event string, pred func(map[string]any) bool) wsMsg {
	return c.waitFor(timeout, "agent."+event, func(m wsMsg) bool {
		if m.Type != "agent" || m.Event != event {
			return false
		}
		return pred == nil || pred(m.Data)
	})
}

// workflowEvent 匹配引擎 BroadcastEvent("workflow", {event, data}) 消息，
// pred 收到的是内层 data（workflowToJSON / status / progress 载荷）。
func workflowEvent(event string, pred func(inner map[string]any) bool) func(wsMsg) bool {
	return func(m wsMsg) bool {
		if m.Type != "workflow" || m.Data["event"] != event {
			return false
		}
		inner, _ := m.Data["data"].(map[string]any)
		if inner == nil {
			return false
		}
		return pred == nil || pred(inner)
	}
}
