package handlers

// 覆盖率收口第五轮（2026-09-22）：batch/script/tagstore 最后一批未覆盖分支。
// 缺口清单见 go tool cover -func（收口前：batch.go stop/trigger 校验与失败分支、
// runBatch 空目标、commandErrorText 四分支、ReadInline 失败 400/500、
// tagstore nil-db 分支）。

import (
	"errors"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/gin-gonic/gin"
)

// ---------- runBatch / commandErrorText 纯函数 ----------

func TestRunBatchEmptyTargets(t *testing.T) {
	called := false
	summary := runBatch(nil, func(*agent.AgentInfo) (bool, string) {
		called = true
		return true, ""
	})
	if called {
		t.Errorf("fn must not be called for empty targets")
	}
	if summary.Total != 0 || summary.Succeeded != 0 || summary.Failed != 0 {
		t.Errorf("empty batch should be all zeros, got %+v", summary)
	}
	if len(summary.Results) != 0 {
		t.Errorf("empty batch should have no results, got %d", len(summary.Results))
	}
}

func TestCommandErrorTextPriority(t *testing.T) {
	cases := []struct {
		name string
		resp map[string]any
		err  error
		want string
	}{
		{"err wins", map[string]any{"error": "ignored"}, errors.New("conn boom"), "conn boom"},
		{"error field", map[string]any{"error": "script not found"}, nil, "script not found"},
		{"message field", map[string]any{"message": "runtime died"}, nil, "runtime died"},
		{"fallback", map[string]any{"success": false}, nil, "agent command failed"},
		{"empty error field falls through", map[string]any{"error": ""}, nil, "agent command failed"},
	}
	for _, tc := range cases {
		if got := commandErrorText(tc.resp, tc.err); got != tc.want {
			t.Errorf("%s: got %q, want %q", tc.name, got, tc.want)
		}
	}
}

// ---------- HandleBatchRunScript：ReadInline 失败分支 ----------

// Resolve 只做字符串校验不查文件存在性：不存在的 .lua 能过 Resolve，
// 在 ReadInline 处 ENOENT → 500（区别于 TooLarge 的 400）。
func TestBatchRunScriptMissingFile500(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1") // targets 非空才会走到 ReadInline
	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1"},
		"path":     "no-such-script.lua",
	})
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("missing script should 500, got %d: %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "read script:") {
		t.Errorf("500 body should carry read error, got %s", w.Body.String())
	}
}

func TestBatchRunScriptOversize400(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("a1")
	big := strings.Repeat("x", scripts.MaxInlineScriptSize+1)
	if err := os.WriteFile(filepath.Join(env.dir, "big.lua"), []byte(big), 0644); err != nil {
		t.Fatalf("write big script: %v", err)
	}
	w := doJSON(env.r, "POST", "/api/agents/batch/run-script", map[string]any{
		"agentIds": []string{"a1"},
		"path":     "big.lua",
	})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("oversize script should 400, got %d: %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "too large") {
		t.Errorf("400 body should carry too-large error, got %s", w.Body.String())
	}
}

// ---------- HandleBatchStopScript：校验与失败分支 ----------

func TestBatchStopScriptValidationErrors(t *testing.T) {
	env := setupBatchEnv(t)

	w := doJSON(env.r, "POST", "/api/agents/batch/stop-script", "not-json")
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body should 400, got %d", w.Code)
	}

	w = doJSON(env.r, "POST", "/api/agents/batch/stop-script", map[string]any{
		"executionId": "exec-1", // executionId 合法、选择器为空 → parseBatchSelector 400
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("empty selector should 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestBatchStopScriptOfflineAndSendError(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("s-online")
	conn := env.addAgent("s-broken")
	// Register 后显式转 offline（Register 默认 online）
	env.registry.Register("s-offline", "s-offline-host", "10.0.0.9", &handlerMockConn{})
	env.registry.UpdateStatus("s-offline", "offline", nil)

	// broken：第一次调用返回 transport error
	conn.errs = []error{errors.New("transport dead")}

	w := doJSON(env.r, "POST", "/api/agents/batch/stop-script", map[string]any{
		"agentIds":    []string{"s-online", "s-broken", "s-offline"},
		"executionId": "exec-1",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("batch stop should 200 with per-agent results, got %d: %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 1 || summary.Failed != 2 {
		t.Fatalf("expected 1 ok + 2 failed, got %+v", summary)
	}
	byID := map[string]BatchAgentResult{}
	for _, r := range summary.Results {
		byID[r.AgentID] = r
	}
	if byID["s-offline"].Success || byID["s-offline"].Error != "agent offline" {
		t.Errorf("offline agent should fail with 'agent offline', got %+v", byID["s-offline"])
	}
	if byID["s-broken"].Success || byID["s-broken"].Error != "transport dead" {
		t.Errorf("send error should be propagated verbatim, got %+v", byID["s-broken"])
	}
}

// ---------- HandleBatchTrigger：校验与失败分支 ----------

func TestBatchTriggerValidationErrors(t *testing.T) {
	env := setupBatchEnv(t)

	w := doJSON(env.r, "POST", "/api/agents/batch/trigger", "not-json")
	if w.Code != http.StatusBadRequest {
		t.Errorf("invalid body should 400, got %d", w.Code)
	}

	w = doJSON(env.r, "POST", "/api/agents/batch/trigger", map[string]any{
		"name": "", // 选择器合法但 name 必填
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("empty name should 400, got %d: %s", w.Code, w.Body.String())
	}

	w = doJSON(env.r, "POST", "/api/agents/batch/trigger", map[string]any{
		"name": "trg",
	})
	if w.Code != http.StatusBadRequest {
		t.Errorf("missing selector should 400, got %d: %s", w.Code, w.Body.String())
	}
}

func TestBatchTriggerPerAgentFailures(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("t-online")
	conn := env.addAgent("t-err")
	conn.responses = []map[string]any{{"success": false, "error": "trigger quota exceeded"}}
	tConn := env.addAgent("t-transport")
	tConn.errs = []error{errors.New("ws transport gone")}
	env.registry.Register("t-off", "t-off-host", "10.0.0.10", &handlerMockConn{})
	env.registry.UpdateStatus("t-off", "offline", nil)

	w := doJSON(env.r, "POST", "/api/agents/batch/trigger", map[string]any{
		"agentIds": []string{"t-online", "t-err", "t-transport", "t-off"},
		"name":     "batched-trg",
		"type":     "timer",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("batch trigger should 200 with per-agent results, got %d: %s", w.Code, w.Body.String())
	}
	summary := decodeSummary(t, w.Body.String())
	if summary.Succeeded != 1 || summary.Failed != 3 {
		t.Fatalf("expected 1 ok + 3 failed, got %+v", summary)
	}
	byID := map[string]BatchAgentResult{}
	for _, r := range summary.Results {
		byID[r.AgentID] = r
	}
	if byID["t-off"].Success || byID["t-off"].Error != "agent offline" {
		t.Errorf("offline agent should fail with 'agent offline', got %+v", byID["t-off"])
	}
	if byID["t-err"].Success || byID["t-err"].Error != "trigger quota exceeded" {
		t.Errorf("resp error field should surface via commandErrorText, got %+v", byID["t-err"])
	}
	if byID["t-transport"].Success || byID["t-transport"].Error != "ws transport gone" {
		t.Errorf("transport error should be propagated verbatim, got %+v", byID["t-transport"])
	}
}

// ---------- HandleRun：ReadInline 失败 500 分支 ----------

func TestScriptHandleRunMissingFile500(t *testing.T) {
	env := setupBatchEnv(t)
	env.addAgent("run-target") // HandleRun 先选在线 agent 再读文件
	sh := NewScriptHandler(env.db, env.dir, env.registry)

	r := gin.New()
	r.POST("/run", asAdmin(1), sh.HandleRun)

	w := doJSON(r, "POST", "/run", map[string]any{"path": "ghost.lua", "agentId": "run-target"})
	if w.Code != http.StatusInternalServerError {
		t.Fatalf("missing script should 500, got %d: %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "read script:") {
		t.Errorf("500 body should carry read error, got %s", w.Body.String())
	}
}

// ---------- tagstore：nil db 分支 ----------

func TestTagStoreNilDB(t *testing.T) {
	store := NewAgentTagStore(nil)

	if tags, ok := store.LoadTags("any-agent"); ok || tags != nil {
		t.Errorf("nil db LoadTags should return (nil,false), got (%v,%v)", tags, ok)
	}
	if err := store.SaveTags("any-agent", "h", "ip", []string{"x"}); err != nil {
		t.Errorf("nil db SaveTags should be a no-op success, got %v", err)
	}
}
