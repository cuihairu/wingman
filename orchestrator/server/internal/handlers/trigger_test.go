package handlers

import (
	"net/http"
	"strings"
	"testing"

	"github.com/gin-gonic/gin"
)

func setupTriggerRouter(t *testing.T, conn *handlerMockConn) *gin.Engine {
	t.Helper()
	gin.SetMode(gin.TestMode)
	reg, _ := newRegistry(t)
	if conn != nil {
		reg.Register("a1", "host1", "10.0.0.1", conn)
	}
	th := NewTriggerHandler(reg, newDB(t))

	r := gin.New()
	r.GET("/agents/:agentId/triggers", th.HandleList)
	r.POST("/agents/:agentId/triggers/toggle", th.HandleToggle)
	return r
}

func TestTriggerListSuccess(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
		"data": map[string]any{
			"triggers": []any{
				map[string]any{"id": "1", "name": "hp-watch", "enabled": true, "type": "ColorFound"},
				map[string]any{"id": "2", "name": "boss-alert", "enabled": false, "type": "ImageFound"},
			},
		},
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "GET", "/agents/a1/triggers", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data []map[string]any `json:"data"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if len(resp.Data) != 2 {
		t.Fatalf("expected 2 triggers, got %d", len(resp.Data))
	}
	if resp.Data[0]["name"] != "hp-watch" {
		t.Errorf("unexpected trigger: %+v", resp.Data[0])
	}
	if conn.callCount() != 1 {
		t.Errorf("expected 1 trigger.list command, got %d", conn.callCount())
	}
}

func TestTriggerListEmptyNormalizes(t *testing.T) {
	// runtime 返回空列表或缺失字段 → 统一为空数组而非 null
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
		"data":    map[string]any{},
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "GET", "/agents/a1/triggers", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"data":[]`) {
		t.Errorf("expected empty array, got %s", w.Body.String())
	}
}

func TestTriggerListAgentNotConnected(t *testing.T) {
	r := setupTriggerRouter(t, nil) // 无 agent

	w := doJSON(r, "GET", "/agents/ghost/triggers", nil)
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502 for missing agent, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "agent not connected") {
		t.Errorf("unexpected error: %s", w.Body.String())
	}
}

func TestTriggerListRuntimeFailure(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": false,
		"error":   "trigger handlers not available",
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "GET", "/agents/a1/triggers", nil)
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502 on runtime failure, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "trigger handlers not available") {
		t.Errorf("error should pass through runtime message, got %s", w.Body.String())
	}
}

func TestTriggerToggleSuccess(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
		"data":    map[string]any{"enabled": false},
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "POST", "/agents/a1/triggers/toggle", map[string]any{"id": "1"})
	if w.Code != http.StatusOK {
		t.Fatalf("toggle: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data struct {
			ID      string `json:"id"`
			Enabled bool   `json:"enabled"`
		} `json:"data"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if resp.Data.ID != "1" || resp.Data.Enabled {
		t.Errorf("unexpected toggle response: %+v", resp.Data)
	}
}

func TestTriggerToggleMissingTrigger(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": false,
		"error":   "Trigger not found",
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "POST", "/agents/a1/triggers/toggle", map[string]any{"id": "99"})
	if w.Code != http.StatusBadGateway {
		t.Errorf("expected 502 for missing trigger, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "Trigger not found") {
		t.Errorf("unexpected error: %s", w.Body.String())
	}
}

func TestTriggerToggleValidation(t *testing.T) {
	r := setupTriggerRouter(t, &handlerMockConn{})

	// 缺 id → 400
	w := doJSON(r, "POST", "/agents/a1/triggers/toggle", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing id: expected 400, got %d", w.Code)
	}
}
