package handlers

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// batchTestEnv 批量 handler 测试环境
type batchTestEnv struct {
	r        *gin.Engine
	dir      string
	db       *gorm.DB
	registry *agent.Registry
	conns    map[string]*handlerMockConn
	connsMu  sync.Mutex
}

// addAgent 注册一台带 mock 连接的在线 agent（可再配 tags）
func (e *batchTestEnv) addAgent(id string, tags ...string) *handlerMockConn {
	conn := &handlerMockConn{}
	e.connsMu.Lock()
	e.conns[id] = conn
	e.connsMu.Unlock()
	e.registry.Register(id, id+"-host", "10.0.0.1", conn)
	if len(tags) > 0 {
		if !e.registry.SetTags(id, tags) {
			panic("SetTags failed for " + id)
		}
	}
	return conn
}

// commandsOf 读取某 agent 已收到的命令
func (e *batchTestEnv) commandsOf(id string) []handlerMockCommand {
	e.connsMu.Lock()
	conn := e.conns[id]
	e.connsMu.Unlock()
	if conn == nil {
		return nil
	}
	return conn.dispatchedCommands()
}

func setupBatchEnv(t *testing.T) *batchTestEnv {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	reg, _ := newRegistry(t)
	bh := NewBatchHandler(db, dir, reg)

	r := gin.New()
	runGrp := r.Group("/api/agents/batch").Use(asAdmin(1))
	{
		runGrp.POST("/run-script", bh.HandleBatchRunScript)
		runGrp.POST("/stop-script", bh.HandleBatchStopScript)
	}
	manageGrp := r.Group("/api/agents/batch").Use(asAdmin(1))
	manageGrp.POST("/trigger", bh.HandleBatchTrigger)

	return &batchTestEnv{r: r, dir: dir, db: db, registry: reg, conns: map[string]*handlerMockConn{}}
}

// writeScript 在脚本目录写入一个 lua 文件并登记 DB 行
func (e *batchTestEnv) writeScript(t *testing.T, name string) string {
	t.Helper()
	path := filepath.Join(e.dir, name)
	if err := os.WriteFile(path, []byte("-- batch test"), 0644); err != nil {
		t.Fatalf("write script: %v", err)
	}
	return name
}

func decodeSummary(t *testing.T, body string) BatchSummary {
	t.Helper()
	var out struct {
		Success bool         `json:"success"`
		Data    BatchSummary `json:"data"`
	}
	if err := json.Unmarshal([]byte(body), &out); err != nil {
		t.Fatalf("decode summary: %v (%s)", err, body)
	}
	return out.Data
}

func TestBatchRunAllSuccess(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1")
	env.addAgent("a2")
	env.writeScript(t, "demo.lua")
	// 与单 agent HandleRun 一致：Updates 只更新已有行，需先登记脚本
	env.db.Create(&models.Script{Name: "demo", Path: "demo.lua", IsRunning: false, Status: "stopped"})

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1", "a2"},
		"path":     "demo.lua",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Total != 2 || summary.Succeeded != 2 || summary.Failed != 0 {
		t.Errorf("unexpected summary: %+v", summary)
	}

	// 每台收到 run_script，payload 为解析后的绝对路径
	absPath, _ := filepath.Abs(filepath.Join(env.dir, "demo.lua"))
	for _, id := range []string{"a1", "a2"} {
		cmds := env.commandsOf(id)
		if len(cmds) != 1 || cmds[0].Method != "run_script" {
			t.Fatalf("%s should receive one run_script, got %+v", id, cmds)
		}
		if cmds[0].Data["path"] != absPath {
			t.Errorf("%s path should be resolved abs path %s, got %v", id, absPath, cmds[0].Data["path"])
		}
	}

	// Script 行置 running
	var count int64
	env.db.Model(&models.Script{}).Where("path = ? AND status = ?", "demo.lua", "running").Count(&count)
	if count != 1 {
		t.Errorf("script row should be marked running, got %d", count)
	}

	// 审计落库且 meta 含选择器
	var log models.AuditLog
	if err := env.db.Where("kind = ?", "script.batch_run").First(&log).Error; err != nil {
		t.Fatalf("audit log missing: %v", err)
	}
	if log.Target != "demo.lua" {
		t.Errorf("audit target should be demo.lua, got %s", log.Target)
	}
	var meta map[string]any
	if err := json.Unmarshal([]byte(log.Meta), &meta); err != nil {
		t.Fatalf("meta decode: %v", err)
	}
	ids, _ := meta["selector_agent_ids"].([]any)
	if len(ids) != 2 {
		t.Errorf("audit meta should contain selector ids, got %v", meta)
	}
}

func TestBatchRunPartialFailure(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("good")
	bad := env.addAgent("bad")
	bad.responses = []map[string]any{{"success": false, "message": "script busy"}}
	env.writeScript(t, "demo.lua")

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"good", "bad"},
		"path":     "demo.lua",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("partial failure should still be 200, got %d", w.Code)
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 1 || summary.Failed != 1 {
		t.Fatalf("expected 1/1, got %+v", summary)
	}
	for _, r := range summary.Results {
		if r.AgentID == "bad" && r.Error != "script busy" {
			t.Errorf("error text should come from resp.message, got %q", r.Error)
		}
	}
}

func TestBatchRunConnErrorFallback(t *testing.T) {
	env := setupBatchEnv(t)
	conn := env.addAgent("flaky")
	conn.errs = []error{errors.New("connection reset")}
	env.writeScript(t, "demo.lua")

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"flaky"},
		"path":     "demo.lua",
	})
	summary := decodeSummary(t, w.Body.String())
	if summary.Failed != 1 || summary.Results[0].Error != "connection reset" {
		t.Errorf("conn error should surface per-agent, got %+v", summary)
	}

	// runtime 响应无 error/message 字段 → 兜底文案
	conn2 := env.addAgent("silent")
	conn2.responses = []map[string]any{{"success": false}}
	w = doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"silent"},
		"path":     "demo.lua",
	})
	summary = decodeSummary(t, w.Body.String())
	if summary.Results[0].Error != "agent command failed" {
		t.Errorf("fallback error text expected, got %q", summary.Results[0].Error)
	}
}

func TestBatchSelectorTagsORAndUnion(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1", "prod")
	env.addAgent("a2", "dev")
	env.addAgent("a3", "prod", "edge")
	env.addAgent("a4")
	env.writeScript(t, "demo.lua")

	// tags OR：命中任一标签的全部执行
	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"tags": []string{"prod", "dev"},
		"path": "demo.lua",
	})
	summary := decodeSummary(t, w.Body.String())
	if summary.Total != 3 || summary.Succeeded != 3 {
		t.Fatalf("OR semantics expected 3 targets, got %+v", summary)
	}

	// 并集去重：a1 被 ids 与 tags 同时命中只执行一次
	for _, conn := range env.conns {
		conn.mu.Lock()
		conn.calls = 0
		conn.commands = nil
		conn.mu.Unlock()
	}
	w = doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1", "a2"},
		"tags":     []string{"prod"},
		"path":     "demo.lua",
	})
	summary = decodeSummary(t, w.Body.String())
	if summary.Total != 3 {
		t.Fatalf("union expected 3 targets, got %+v", summary)
	}
	if cmds := env.commandsOf("a1"); len(cmds) != 1 {
		t.Errorf("a1 should be dispatched exactly once, got %d", len(cmds))
	}
}

func TestBatchSelectorValidation(t *testing.T) {
	env := setupBatchEnv(t)
	env.writeScript(t, "demo.lua")

	// 空选择器 → 400
	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"path": "demo.lua",
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("empty selector: expected 400, got %d", w.Code)
	}

	// 缺 path（binding required）→ 400
	w = doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1"},
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing path: expected 400, got %d", w.Code)
	}

	// 非法路径（穿越）→ 400 且零下发
	w = doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1"},
		"path":     "../escape.lua",
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("traversal path: expected 400, got %d", w.Code)
	}

	// 条目超限 → 400
	bigIDs := make([]string, maxBatchSelectorItems+1)
	for i := range bigIDs {
		bigIDs[i] = fmt.Sprintf("agent-%03d", i)
	}
	w = doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": bigIDs,
		"path":     "demo.lua",
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("oversized selector: expected 400, got %d", w.Code)
	}
}

func TestBatchNoMatchReturnsZeroTotal(t *testing.T) {
	env := setupBatchEnv(t)

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"tags": []string{"nobody"},
		"path": "demo.lua",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("no match should be 200, got %d", w.Code)
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Total != 0 || len(summary.Results) != 0 {
		t.Errorf("expected empty summary, got %+v", summary)
	}
}

func TestBatchOfflineAgentReported(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("online-agent")
	off := env.addAgent("offline-agent")
	env.registry.UpdateStatus("offline-agent", "offline", nil)
	_ = off
	env.writeScript(t, "demo.lua")

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"online-agent", "offline-agent"},
		"path":     "demo.lua",
	})
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 1 || summary.Failed != 1 {
		t.Fatalf("expected 1 ok + 1 offline, got %+v", summary)
	}
	for _, r := range summary.Results {
		if r.AgentID == "offline-agent" {
			if r.Success || r.Error != "agent offline" {
				t.Errorf("offline agent should fail with 'agent offline', got %+v", r)
			}
		}
	}
}

func TestBatchStopScript(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1")
	env.addAgent("a2")
	env.db.Create(&models.Script{Name: "demo", Path: "demo.lua", IsRunning: true, Status: "running"})

	w := doJSON(env.r, "POST", "/api/agents/batch/stop-script", map[string]any{
		"agentIds":    []string{"a1", "a2"},
		"executionId": "demo",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 2 {
		t.Fatalf("expected 2 succeeded, got %+v", summary)
	}
	for _, id := range []string{"a1", "a2"} {
		cmds := env.commandsOf(id)
		if len(cmds) != 1 || cmds[0].Method != "stop_script" || cmds[0].Data["script_id"] != "demo" {
			t.Errorf("%s should receive stop_script{script_id:demo}, got %+v", id, cmds)
		}
	}
	var script models.Script
	env.db.Where("name = ?", "demo").First(&script)
	if script.IsRunning || script.Status != "stopped" {
		t.Errorf("script should be stopped, got %+v", script)
	}
	var log models.AuditLog
	if err := env.db.Where("kind = ?", "script.batch_stop").First(&log).Error; err != nil {
		t.Fatalf("audit missing: %v", err)
	}
}

func TestBatchTrigger(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1")

	// name 缺失 → 400
	w := doJSON(env.r, "POST", "/api/agents/batch/trigger", map[string]any{
		"agentIds":  []string{"a1"},
		"condition": map[string]any{"type": "TimeElapsed", "value": "5000"},
	})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("missing name: expected 400, got %d", w.Code)
	}

	// 正常下发：config 平铺字段正确
	w = doJSON(env.r, "POST", "/api/agents/batch/trigger", map[string]any{
		"agentIds":  []string{"a1"},
		"name":      "hp-watch",
		"condition": map[string]any{"type": "TimeElapsed", "value": "5000"},
		"actions":   []map[string]any{{"type": "KeyPress", "value": "F1"}},
	})
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 1 {
		t.Fatalf("expected 1 succeeded, got %+v", summary)
	}
	cmds := env.commandsOf("a1")
	if len(cmds) != 1 || cmds[0].Method != "trigger.add" {
		t.Fatalf("expected trigger.add, got %+v", cmds)
	}
	config, _ := cmds[0].Data["config"].(map[string]any)
	if config == nil || config["name"] != "hp-watch" {
		t.Errorf("config should carry flattened name, got %v", cmds[0].Data)
	}
	condition, _ := config["condition"].(map[string]any)
	if condition == nil || condition["type"] != "TimeElapsed" {
		t.Errorf("config.condition should be preserved, got %v", config)
	}

	var log models.AuditLog
	if err := env.db.Where("kind = ?", "agent.batch_trigger_add").First(&log).Error; err != nil {
		t.Fatalf("audit missing: %v", err)
	}
	if log.Target != "hp-watch" {
		t.Errorf("audit target should be trigger name, got %s", log.Target)
	}
}

func TestBatchRunConcurrencyAndOrder(t *testing.T) {
	env := setupBatchEnv(t)
	ids := make([]string, 12)
	for i := range ids {
		ids[i] = fmt.Sprintf("agent-%02d", i)
		env.addAgent(ids[i])
	}
	env.writeScript(t, "demo.lua")

	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": ids,
		"path":     "demo.lua",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Total != 12 || summary.Succeeded != 12 {
		t.Fatalf("all 12 should succeed, got %+v", summary)
	}
	// 结果按 AgentID 排序（resolveTargets 确定性排序）、无丢失
	want := append([]string(nil), ids...)
	sort.Strings(want)
	for i, r := range summary.Results {
		if r.AgentID != want[i] {
			t.Errorf("results[%d] = %s, want %s", i, r.AgentID, want[i])
		}
	}
}
