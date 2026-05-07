package websocket

import (
	"encoding/json"
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

// Message WebSocket 消息
type Message struct {
	Type      string      `json:"type"`
	Event     string      `json:"event,omitempty"`
	Data      interface{} `json:"data,omitempty"`
	RoomID    string      `json:"roomId,omitempty"`
	Timestamp int64       `json:"timestamp,omitempty"`
}

// Connection WebSocket 连接
type Connection struct {
	ID         string
	Conn       *websocket.Conn
	Send       chan *Message
	Hub        *Hub
	currentRoom string
	lastPing   time.Time
	mu         sync.Mutex
}

// Hub WebSocket 连接中心
type Hub struct {
	connections map[string]*Connection
	rooms       map[string]map[string]*Connection
	register    chan *Connection
	unregister  chan *Connection
	broadcast   chan *Message
	mu          sync.RWMutex
	connCounter  uint64
}

// NewHub 创建 Hub
func NewHub() *Hub {
	return &Hub{
		connections: make(map[string]*Connection),
		rooms:       make(map[string]map[string]*Connection),
		register:    make(chan *Connection),
		unregister:  make(chan *Connection),
		broadcast:   make(chan *Message),
	}
}

// Run 运行 Hub
func (h *Hub) Run() {
	for {
		select {
		case conn := <-h.register:
			h.mu.Lock()
			h.connections[conn.ID] = conn
			h.mu.Unlock()
			log.Printf("[WS] Connected: %s (total: %d)", conn.ID, len(h.connections))

		case conn := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.connections[conn.ID]; ok {
				delete(h.connections, conn.ID)
				close(conn.Send)
			}
			if conn.currentRoom != "" {
				if room, ok := h.rooms[conn.currentRoom]; ok {
					delete(room, conn.ID)
					if len(room) == 0 {
						delete(h.rooms, conn.currentRoom)
					}
				}
			}
			h.mu.Unlock()
			log.Printf("[WS] Disconnected: %s (total: %d)", conn.ID, len(h.connections))

		case message := <-h.broadcast:
			h.mu.RLock()
			for _, conn := range h.connections {
				select {
				case conn.Send <- message:
				default:
					close(conn.Send)
					delete(h.connections, conn.ID)
				}
			}
			h.mu.RUnlock()
		}
	}
}

// JoinRoom 加入房间
func (h *Hub) JoinRoom(connID, roomID string) {
	h.mu.Lock()
	defer h.mu.Unlock()

	conn, ok := h.connections[connID]
	if !ok {
		return
	}

	if conn.currentRoom != "" {
		if oldRoom, ok := h.rooms[conn.currentRoom]; ok {
			delete(oldRoom, connID)
		}
	}

	if _, ok := h.rooms[roomID]; !ok {
		h.rooms[roomID] = make(map[string]*Connection)
	}
	h.rooms[roomID][connID] = conn
	conn.currentRoom = roomID
}

// LeaveRoom 离开房间
func (h *Hub) LeaveRoom(connID string) {
	h.mu.Lock()
	defer h.mu.Unlock()

	conn, ok := h.connections[connID]
	if !ok || conn.currentRoom == "" {
		return
	}

	if room, ok := h.rooms[conn.currentRoom]; ok {
		delete(room, connID)
		if len(room) == 0 {
			delete(h.rooms, conn.currentRoom)
		}
	}

	conn.currentRoom = ""
}

// BroadcastToRoom 向房间广播
func (h *Hub) BroadcastToRoom(roomID string, message *Message) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	room, ok := h.rooms[roomID]
	if !ok {
		return
	}

	for _, conn := range room {
		select {
		case conn.Send <- message:
		default:
		}
	}
}

// BroadcastEvent 广播事件
func (h *Hub) BroadcastEvent(eventType string, data interface{}) {
	message := &Message{
		Type:      eventType,
		Data:      data,
		Timestamp: time.Now().Unix(),
	}
	h.broadcast <- message
}

// BroadcastDebugEvent 广播调试事件
func (h *Hub) BroadcastDebugEvent(eventType string, data interface{}) {
	message := &Message{
		Type:      "debugger",
		Event:     eventType,
		Data:      data,
		Timestamp: time.Now().Unix(),
	}
	h.broadcast <- message
}

// BroadcastAgentEvent 广播 Agent 事件
func (h *Hub) BroadcastAgentEvent(eventType string, data interface{}) {
	message := &Message{
		Type:      "agent",
		Event:     eventType,
		Data:      data,
		Timestamp: time.Now().Unix(),
	}
	h.broadcast <- message
}

// HandleWebSocket 处理 WebSocket 连接
func HandleWebSocket(c *gin.Context, hub *Hub) {
	conn, err := upgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		log.Printf("[WS] Upgrade error: %v", err)
		return
	}

	hub.connCounter++
	connID := "ws_" + string(rune(hub.connCounter))

	wsConn := &Connection{
		ID:       connID,
		Conn:     conn,
		Send:     make(chan *Message, 256),
		Hub:      hub,
		lastPing: time.Now(),
	}

	hub.register <- wsConn

	// 发送欢迎消息
	welcome := &Message{
		Type:      "connected",
		Data:      gin.H{"connectionId": connID},
		Timestamp: time.Now().Unix(),
	}
	wsConn.Send <- welcome

	// 启动读写协程
	go wsConn.writePump()
	go wsConn.readPump()
}

// readPump 读取协程
func (c *Connection) readPump() {
	defer func() {
		c.Hub.unregister <- c
		c.Conn.Close()
	}()

	c.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
	c.Conn.SetPongHandler(func(string) error {
		c.Conn.SetReadDeadline(time.Now().Add(60 * time.Second))
		c.mu.Lock()
		c.lastPing = time.Now()
		c.mu.Unlock()
		return nil
	})

	for {
		_, message, err := c.Conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
				log.Printf("[WS] Read error: %v", err)
			}
			break
		}

		var msg Message
		if err := json.Unmarshal(message, &msg); err != nil {
			log.Printf("[WS] JSON error: %v", err)
			continue
		}

		switch msg.Type {
		case "pong":
			c.mu.Lock()
			c.lastPing = time.Now()
			c.mu.Unlock()
		case "join_room":
			c.Hub.JoinRoom(c.ID, msg.RoomID)
		case "leave_room":
			c.Hub.LeaveRoom(c.ID)
		case "room_message":
			c.Hub.BroadcastToRoom(msg.RoomID, &Message{
				Type:      "room",
				Event:     "message",
				RoomID:    msg.RoomID,
				Data:      msg.Data,
				Timestamp: time.Now().Unix(),
			})
		}
	}
}

// writePump 写入协程
func (c *Connection) writePump() {
	ticker := time.NewTicker(30 * time.Second)
	defer func() {
		ticker.Stop()
		c.Conn.Close()
	}()

	for {
		select {
		case message, ok := <-c.Send:
			c.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if !ok {
				c.Conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			data, err := json.Marshal(message)
			if err != nil {
				log.Printf("[WS] Marshal error: %v", err)
				continue
			}

			if err := c.Conn.WriteMessage(websocket.TextMessage, data); err != nil {
				return
			}

		case <-ticker.C:
			c.Conn.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if err := c.Conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}
