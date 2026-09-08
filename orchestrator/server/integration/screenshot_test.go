package integration

import (
	"fmt"
	"net/http"
	"testing"
	"time"
)

const (
	reportedShotImage = "aGVsbG8gcmVwb3J0ZXIgc2NyZWVuc2hvdA=="
	agentShotImage    = "YWdlbnQtY2FwdHVyZWQtc2NyZWVuc2hvdA=="
)

// TestScreenshotAgentToDashboardBroadcast 覆盖两条截图链路：
// A) 上报端 HTTP POST /api/v1/screenshot（ScreenshotReporter 模式）→ WS 广播；
// B) workflow screenshot 步骤 → 下发 screenshot.capture → agent 回传图片 → WS 广播。
func TestScreenshotAgentToDashboardBroadcast(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	dash := newDashClient(t, env.httpSrv.URL, admin)

	// ---- A) 上报端模式：HTTP POST → screenshot 广播 ----
	res := env.do(t, "POST", "/api/v1/screenshot", admin, map[string]any{
		"image":     reportedShotImage,
		"width":     800,
		"height":    600,
		"timestamp": time.Now().UnixMilli(),
	})
	if res.Status != http.StatusOK {
		t.Fatalf("POST /api/v1/screenshot: %d %s", res.Status, res.Body)
	}
	msg := dash.waitFor(3*time.Second, "上报截图广播", func(m wsMsg) bool {
		return m.Type == "screenshot" && m.Data["image"] == reportedShotImage
	})
	if msg.Data["width"] != float64(800) || msg.Data["height"] != float64(600) {
		t.Errorf("广播缺少尺寸: %+v", msg.Data)
	}

	// 缺 image → 400，且不应产生新广播
	if res := env.do(t, "POST", "/api/v1/screenshot", admin, map[string]any{"width": 1}); res.Status != http.StatusBadRequest {
		t.Errorf("missing image: got %d want 400", res.Status)
	}

	// ---- B) workflow screenshot 步骤：agent 执行 screenshot.capture ----
	a := newSimAgent(t, env.agentAddr, "it-shot-agent", "shot-host", func(method string, data map[string]any) map[string]any {
		if method == "screenshot.capture" {
			return map[string]any{
				"success": true,
				"data": map[string]any{
					"image":  agentShotImage,
					"width":  1024,
					"height": 768,
				},
			}
		}
		return map[string]any{"success": true}
	})
	env.waitForAgentStatus(t, admin, "it-shot-agent", "online")

	wfID := env.createWorkflow(t, admin, "e2e-screenshot", []map[string]any{
		{"id": "cap", "name": "采集截图", "type": "screenshot"},
	})
	env.waitForWorkflowStatus(t, admin, wfID, "completed")

	// capture 命令确实到达 agent，且携带超时参数
	cmds := a.waitForCommands(t, "screenshot.capture", 1, 3*time.Second)
	if _, ok := cmds[0].Data["timeout"]; !ok {
		t.Errorf("screenshot.capture 命令应携带 timeout, data=%+v", cmds[0].Data)
	}

	// 步骤状态含 worker 归属与结果消息
	step := env.getStepStatus(t, admin, wfID, "cap")
	if step["status"] != "completed" {
		t.Errorf("cap 步骤状态: got %v want completed", step["status"])
	}
	if step["workerId"] != "it-shot-agent" {
		t.Errorf("cap 步骤 worker: got %v want it-shot-agent", step["workerId"])
	}

	// agent 回传的截图经 orchestrator 广播给 dashboard，带工作流上下文
	shot := dash.waitFor(3*time.Second, "workflow 截图广播", func(m wsMsg) bool {
		return m.Type == "screenshot" && m.Data["image"] == agentShotImage && m.Data["stepId"] == "cap"
	})
	if shot.Data["workerId"] != "it-shot-agent" {
		t.Errorf("截图广播 workerId: got %v", shot.Data["workerId"])
	}
	if fmt.Sprint(shot.Data["workflowId"]) != fmt.Sprint(wfID) {
		t.Errorf("截图广播 workflowId: got %v want %d", shot.Data["workflowId"], wfID)
	}
	if shot.Data["width"] != float64(1024) {
		t.Errorf("截图广播 width: got %v", shot.Data["width"])
	}
}
