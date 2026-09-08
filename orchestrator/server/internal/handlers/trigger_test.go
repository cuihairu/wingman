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
	r.POST("/agents/:agentId/triggers", th.HandleCreate)
	r.PUT("/agents/:agentId/triggers/:triggerId", th.HandleUpdate)
	r.DELETE("/agents/:agentId/triggers/:triggerId", th.HandleRemove)
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

	// runtime 明确报告 not found → 404（而非笼统 502）
	w := doJSON(r, "POST", "/agents/a1/triggers/toggle", map[string]any{"id": "99"})
	if w.Code != http.StatusNotFound {
		t.Errorf("expected 404 for missing trigger, got %d", w.Code)
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

func TestTriggerCreateSuccess(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
		"data":    map[string]any{"id": "7"},
	}}}
	r := setupTriggerRouter(t, conn)

	config := map[string]any{
		"name":      "hp-watch",
		"condition": map[string]any{"type": "ColorFound", "value": "#ff0000"},
		"actions":   []any{map[string]any{"type": "RunScript", "value": "heal.lua"}},
	}
	w := doJSON(r, "POST", "/agents/a1/triggers", config)
	if w.Code != http.StatusOK {
		t.Fatalf("create: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data struct {
			ID   string `json:"id"`
			Name string `json:"name"`
		} `json:"data"`
	}
	readJSON(t, w.Body.Bytes(), &resp)
	if resp.Data.ID != "7" || resp.Data.Name != "hp-watch" {
		t.Errorf("unexpected create response: %+v", resp.Data)
	}

	// 命令应为 trigger.add 且 config 原样透传
	cmds := conn.dispatchedCommands()
	if len(cmds) != 1 || cmds[0].Method != "trigger.add" {
		t.Fatalf("expected trigger.add command, got %+v", cmds)
	}
	cfg, _ := cmds[0].Data["config"].(map[string]any)
	if cfg["name"] != "hp-watch" {
		t.Errorf("config should pass through, got %+v", cmds[0].Data)
	}
}

func TestTriggerCreateValidation(t *testing.T) {
	r := setupTriggerRouter(t, &handlerMockConn{})

	// 空 body → 400
	w := doJSON(r, "POST", "/agents/a1/triggers", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("empty body: expected 400, got %d", w.Code)
	}
	// 缺 name → 400
	w = doJSON(r, "POST", "/agents/a1/triggers", map[string]any{"cooldown": 100})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing name: expected 400, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "trigger name is required") {
		t.Errorf("unexpected error: %s", w.Body.String())
	}
}

func TestTriggerUpdateSuccess(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "PUT", "/agents/a1/triggers/3", map[string]any{"cooldown": 5000})
	if w.Code != http.StatusOK {
		t.Fatalf("update: %d %s", w.Code, w.Body.String())
	}

	cmds := conn.dispatchedCommands()
	if len(cmds) != 1 || cmds[0].Method != "trigger.update" {
		t.Fatalf("expected trigger.update command, got %+v", cmds)
	}
	if cmds[0].Data["id"] != "3" {
		t.Errorf("update should carry id=3, got %+v", cmds[0].Data)
	}
	cfg, _ := cmds[0].Data["config"].(map[string]any)
	if cooldown, _ := cfg["cooldown"].(float64); cooldown != 5000 {
		t.Errorf("config should pass through, got %+v", cmds[0].Data)
	}
}

func TestTriggerUpdateValidationAndNotFound(t *testing.T) {
	// 非数字 id → 400（避免 runtime std::stoull 抛异常）
	r := setupTriggerRouter(t, &handlerMockConn{})
	if w := doJSON(r, "PUT", "/agents/a1/triggers/abc", map[string]any{"name": "x"}); w.Code != http.StatusBadRequest {
		t.Errorf("invalid id: expected 400, got %d", w.Code)
	}

	// runtime 报 not found → 404
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": false,
		"error":   "Trigger not found",
	}}}
	r = setupTriggerRouter(t, conn)
	if w := doJSON(r, "PUT", "/agents/a1/triggers/99", map[string]any{"name": "x"}); w.Code != http.StatusNotFound {
		t.Errorf("missing trigger: expected 404, got %d", w.Code)
	}
}

func TestTriggerRemoveSuccess(t *testing.T) {
	conn := &handlerMockConn{responses: []map[string]any{{
		"success": true,
	}}}
	r := setupTriggerRouter(t, conn)

	w := doJSON(r, "DELETE", "/agents/a1/triggers/3", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("remove: %d %s", w.Code, w.Body.String())
	}

	cmds := conn.dispatchedCommands()
	if len(cmds) != 1 || cmds[0].Method != "trigger.remove" {
		t.Fatalf("expected trigger.remove command, got %+v", cmds)
	}
	if cmds[0].Data["id"] != "3" {
		t.Errorf("remove should carry id=3, got %+v", cmds[0].Data)
	}
}

func TestTriggerRemoveInvalidID(t *testing.T) {
	r := setupTriggerRouter(t, &handlerMockConn{})
	if w := doJSON(r, "DELETE", "/agents/a1/triggers/not-a-number", nil); w.Code != http.StatusBadRequest {
		t.Errorf("invalid id: expected 400, got %d", w.Code)
	}
}
