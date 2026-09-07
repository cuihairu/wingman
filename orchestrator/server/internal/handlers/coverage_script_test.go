package handlers

import (
	"encoding/json"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
)

// ---------- Script handler 剩余分支 ----------

func TestScriptDeleteBranches(t *testing.T) {
	r, dir, db := setupScriptRouter(t)
	doJSON(r, "POST", "/api/scripts", map[string]any{"name": "del-me"})

	// bind 失败 → 400
	w := doJSON(r, "POST", "/api/scripts/delete", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing path: expected 400, got %d", w.Code)
	}

	// Resolve 失败 → 400
	w = doJSON(r, "POST", "/api/scripts/delete", map[string]any{"path": "../escape.lua"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("traversal path: expected 400, got %d", w.Code)
	}

	// 文件不存在 → 500
	w = doJSON(r, "POST", "/api/scripts/delete", map[string]any{"path": "ghost.lua"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("missing file: expected 500, got %d", w.Code)
	}

	// 成功删除
	w = doJSON(r, "POST", "/api/scripts/delete", map[string]any{"path": "del-me.lua"})
	if w.Code != http.StatusOK {
		t.Fatalf("delete: %d %s", w.Code, w.Body.String())
	}
	if _, err := os.Stat(filepath.Join(dir, "del-me.lua")); !os.IsNotExist(err) {
		t.Errorf("file should be removed: %v", err)
	}
	var count int64
	db.Model(&models.Script{}).Count(&count)
	if count != 0 {
		t.Errorf("script row should be removed, got %d", count)
	}
}

func TestScriptCreateMoreValidation(t *testing.T) {
	r, _, _ := setupScriptRouter(t)

	// 空白名（bind required 通过 trim 后为空）
	w := doJSON(r, "POST", "/api/scripts", map[string]any{"name": "   "})
	if w.Code != http.StatusBadRequest {
		t.Errorf("blank name: expected 400, got %d", w.Code)
	}

	// 过长名
	w = doJSON(r, "POST", "/api/scripts", map[string]any{"name": strings.Repeat("a", 256)})
	if w.Code != http.StatusBadRequest {
		t.Errorf("long name: expected 400, got %d", w.Code)
	}

	// 缺 name → 400
	w = doJSON(r, "POST", "/api/scripts", map[string]any{"description": "d"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing name: expected 400, got %d", w.Code)
	}

	// Resolve 失败：绝对路径
	w = doJSON(r, "POST", "/api/scripts", map[string]any{"name": "/abs/path"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("absolute path: expected 400, got %d", w.Code)
	}
}

func TestScriptGetContentAndSaveErrors(t *testing.T) {
	r, dir, _ := setupScriptRouter(t)

	// content：bind 失败 / Resolve 失败 / 文件不存在
	w := doJSON(r, "POST", "/api/scripts/content", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("content missing path: expected 400, got %d", w.Code)
	}
	w = doJSON(r, "POST", "/api/scripts/content", map[string]any{"path": "a.txt"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("content non-lua: expected 400, got %d", w.Code)
	}
	w = doJSON(r, "POST", "/api/scripts/content", map[string]any{"path": "ghost.lua"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("content missing file: expected 500, got %d", w.Code)
	}

	// save：bind 失败（缺 content）
	w = doJSON(r, "POST", "/api/scripts/save", map[string]any{"path": "x.lua"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("save missing content: expected 400, got %d", w.Code)
	}
	// save：Resolve 失败
	w = doJSON(r, "POST", "/api/scripts/save", map[string]any{"path": "x.txt", "content": "c"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("save non-lua: expected 400, got %d", w.Code)
	}
	// save：目标是一个目录 → WriteFile 失败 500
	if err := os.Mkdir(filepath.Join(dir, "dir.lua"), 0755); err != nil {
		t.Fatal(err)
	}
	w = doJSON(r, "POST", "/api/scripts/save", map[string]any{"path": "dir.lua", "content": "c"})
	if w.Code != http.StatusInternalServerError {
		t.Errorf("save onto dir: expected 500, got %d", w.Code)
	}
}

func TestScriptRunErrorVariants(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()

	// bind 失败
	reg, _ := newRegistry(t)
	sh := NewScriptHandler(db, dir, reg)
	r := gin.New()
	r.POST("/run", asAdmin(1), sh.HandleRun)
	w := doJSON(r, "POST", "/run", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("run missing path: expected 400, got %d", w.Code)
	}

	// Resolve 失败
	w = doJSON(r, "POST", "/run", map[string]any{"path": "../escape.lua"})
	if w.Code != http.StatusBadRequest {
		t.Errorf("run traversal: expected 400, got %d", w.Code)
	}

	// 指定 agent 不在线 → 502 specified
	reg.Register("a1", "h", "10.0.0.1", &handlerMockConn{})
	w = doJSON(r, "POST", "/run", map[string]any{"path": "demo.lua", "agentId": "ghost"})
	if w.Code != http.StatusBadGateway || !strings.Contains(w.Body.String(), "specified") {
		t.Errorf("run specified ghost: %d %s", w.Code, w.Body.String())
	}

	// 命令错误 → 502
	r2 := gin.New()
	r2.POST("/run", asAdmin(1), sh.HandleRun)
	reg2, _ := newRegistry(t)
	reg2.Register("a1", "h", "10.0.0.1", &handlerMockConn{errs: []error{errBoom}})
	sh2 := NewScriptHandler(db, dir, reg2)
	r3 := gin.New()
	r3.POST("/run", asAdmin(1), sh2.HandleRun)
	w = doJSON(r3, "POST", "/run", map[string]any{"path": "demo.lua"})
	if w.Code != http.StatusBadGateway {
		t.Errorf("run command error: expected 502, got %d", w.Code)
	}

	// success=false 带 message / 无任何信息两种兜底
	for i, resp := range []map[string]any{
		{"success": false, "message": "agent says no"},
		{"success": false},
	} {
		regN, _ := newRegistry(t)
		regN.Register("a1", "h", "10.0.0.1", &handlerMockConn{responses: []map[string]any{resp}})
		shN := NewScriptHandler(db, dir, regN)
		rN := gin.New()
		rN.POST("/run", asAdmin(1), shN.HandleRun)
		w = doJSON(rN, "POST", "/run", map[string]any{"path": "demo.lua"})
		if w.Code != http.StatusBadGateway {
			t.Errorf("case %d: expected 502, got %d", i, w.Code)
		}
	}
}

func TestScriptStopBranches(t *testing.T) {
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	reg.Register("a1", "h", "10.0.0.1", &handlerMockConn{responses: []map[string]any{{"success": true}}})
	db.Create(&models.Script{Name: "demo", Path: "demo.lua"})

	sh := NewScriptHandler(db, dir, reg)
	r := gin.New()
	r.POST("/stop", asAdmin(1), sh.HandleStop)

	// bind 失败
	w := doJSON(r, "POST", "/stop", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("stop missing id: expected 400, got %d", w.Code)
	}

	// 无 agent（未注册任何在线）→ 502
	regEmpty, _ := newRegistry(t)
	shEmpty := NewScriptHandler(db, dir, regEmpty)
	rEmpty := gin.New()
	rEmpty.POST("/stop", asAdmin(1), shEmpty.HandleStop)
	w = doJSON(rEmpty, "POST", "/stop", map[string]any{"executionId": "demo"})
	if w.Code != http.StatusBadGateway || !strings.Contains(w.Body.String(), "no available agent") {
		t.Errorf("stop no agent: %d %s", w.Code, w.Body.String())
	}
	w = doJSON(rEmpty, "POST", "/stop", map[string]any{"executionId": "demo", "agentId": "ghost"})
	if w.Code != http.StatusBadGateway || !strings.Contains(w.Body.String(), "specified") {
		t.Errorf("stop ghost agent: %d %s", w.Code, w.Body.String())
	}

	// 命令失败 → 502
	regErr, _ := newRegistry(t)
	regErr.Register("a1", "h", "10.0.0.1", &handlerMockConn{errs: []error{errBoom}})
	shErr := NewScriptHandler(db, dir, regErr)
	rErr := gin.New()
	rErr.POST("/stop", asAdmin(1), shErr.HandleStop)
	w = doJSON(rErr, "POST", "/stop", map[string]any{"executionId": "demo"})
	if w.Code != http.StatusBadGateway {
		t.Errorf("stop command error: expected 502, got %d", w.Code)
	}

	// 成功 → 200 且脚本状态更新
	w = doJSON(r, "POST", "/stop", map[string]any{"executionId": "demo"})
	if w.Code != http.StatusOK {
		t.Fatalf("stop: %d %s", w.Code, w.Body.String())
	}
	var stored models.Script
	if err := db.First(&stored, "name = ?", "demo").Error; err != nil {
		t.Fatal(err)
	}
	if stored.IsRunning || stored.Status != "stopped" {
		t.Errorf("script should be stopped, got running=%v status=%s", stored.IsRunning, stored.Status)
	}
}

func TestScriptLogsValidation(t *testing.T) {
	r, _, _ := setupScriptRouter(t)
	w := doJSON(r, "POST", "/api/scripts/logs", nil)
	if w.Code != http.StatusBadRequest {
		t.Errorf("logs missing id: expected 400, got %d", w.Code)
	}

	// offset/limit 参数生效
	db := newDB(t)
	reg, _ := newRegistry(t)
	dir := t.TempDir()
	sh := NewScriptHandler(db, dir, reg)
	for i := 0; i < 5; i++ {
		db.Create(&models.ExecutionLog{ScriptID: "logdemo", Output: "line", Level: "info"})
	}
	r2 := gin.New()
	r2.POST("/logs", asAdmin(1), sh.HandleLogs)
	w = doJSON(r2, "POST", "/logs", map[string]any{"executionId": "logdemo", "offset": 3})
	if w.Code != http.StatusOK {
		t.Fatalf("logs: %d", w.Code)
	}
	var resp struct {
		Data []map[string]any `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Data) != 2 {
		t.Errorf("offset=3 over 5 rows should return 2, got %d", len(resp.Data))
	}
	w = doJSON(r2, "POST", "/logs", map[string]any{"executionId": "logdemo", "limit": 2})
	json.Unmarshal(w.Body.Bytes(), &resp)
	if len(resp.Data) != 2 {
		t.Errorf("limit=2 should return 2, got %d", len(resp.Data))
	}
}
