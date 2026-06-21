package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
)

func newDebugRouter(t *testing.T) *gin.Engine {
	t.Helper()
	gin.SetMode(gin.TestMode)
	hub := ws.NewHub()
	go hub.Run() // 消费广播通道，避免 Register 时阻塞
	reg := agent.NewRegistry(hub)
	reg.Register("agent-1", "host1", "10.0.0.5", nil)
	h := NewDebugHandler(reg)
	r := gin.New()
	r.GET("/api/debugger/info", h.HandleDebuggerInfo)
	r.POST("/api/debugger/connect", h.HandleDebuggerConnect)
	return r
}

func TestDebuggerInfoReturnsDirectAttachModel(t *testing.T) {
	r := newDebugRouter(t)
	req := httptest.NewRequest(http.MethodGet, "/api/debugger/info", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("info: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Success     bool              `json:"success"`
		Mode        string            `json:"mode"`
		DefaultPort int               `json:"defaultPort"`
		Agents      []agentDebugEndpoint `json:"agents"`
		Launch      map[string]any    `json:"launchConfig"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp.Mode != "direct_attach" {
		t.Errorf("expected mode direct_attach, got %s", resp.Mode)
	}
	if resp.DefaultPort != 9966 {
		t.Errorf("expected default port 9966, got %d", resp.DefaultPort)
	}
	if len(resp.Agents) != 1 || resp.Agents[0].AgentID != "agent-1" {
		t.Errorf("expected 1 agent, got %+v", resp.Agents)
	}
	if resp.Agents[0].Endpoint != "10.0.0.5:9966" {
		t.Errorf("expected endpoint 10.0.0.5:9966, got %s", resp.Agents[0].Endpoint)
	}
}

func TestDebuggerConnectReturnsDirectAttachGuidance(t *testing.T) {
	r := newDebugRouter(t)
	req := httptest.NewRequest(http.MethodPost, "/api/debugger/connect", nil)
	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)

	if w.Code != http.StatusNotImplemented {
		t.Fatalf("expected 501, got %d", w.Code)
	}
	var resp map[string]any
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatal(err)
	}
	if resp["mode"] != "direct_attach" {
		t.Errorf("expected mode direct_attach, got %v", resp["mode"])
	}
	if resp["hint"] == nil {
		t.Error("expected hint pointing to /api/debugger/info")
	}
}
