package integration

import (
	"encoding/json"
	"net/http"
	"testing"
	"time"
)

// 端到端：Dashboard 创建团队（POST /api/teams）→ runtime agent 连接并 team.join
// → 加入确认经 inbox.message 实时推送到 agent（断链修复 A/B 的链路验证）
// → status_report 转发给团队其他在线成员（断链修复 C）→ 权限校验（viewer 403）。
func TestTeamCreateJoinInboxPush(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	viewer := env.login(t, "viewer")

	// 1. Dashboard 创建团队（创建者 admin 即 leader）
	res := env.do(t, "POST", "/api/teams", admin, map[string]any{
		"name":        "e2e-squad",
		"description": "集成测试团队",
	})
	if res.Status != http.StatusOK {
		t.Fatalf("create team: %d %s", res.Status, res.Body)
	}
	var createResp struct {
		Success bool `json:"success"`
		Data    struct {
			TeamID      string `json:"teamId"`
			Name        string `json:"name"`
			Description string `json:"description"`
			LeaderID    string `json:"leaderId"`
		} `json:"data"`
	}
	if err := json.Unmarshal(res.Body, &createResp); err != nil || !createResp.Success {
		t.Fatalf("decode create response: %v (%s)", err, res.Body)
	}
	teamID := createResp.Data.TeamID
	if teamID == "" {
		t.Fatal("teamId should not be empty")
	}
	if createResp.Data.Name != "e2e-squad" || createResp.Data.LeaderID != "admin" {
		t.Errorf("unexpected team data: %+v", createResp.Data)
	}

	// 2. runtime agent 连接并加入团队
	worker := newSimAgent(t, env.agentAddr, "it-team-agent", "team-host", func(string, map[string]any) map[string]any { return nil })
	worker.notify("team.join", map[string]any{"teamId": teamID, "memberId": "worker-1"})

	// 3. 加入确认应经 inbox.message 实时推送（而非仅留在内存缓冲）
	worker.waitForNotify(t, "inbox.message", 5*time.Second)

	// 4. 服务端团队名单：leader（离线占位）+ worker
	tm := env.listener.GetTeamManager()
	env.waitForAgentStatus(t, admin, "it-team-agent", "online") // 确保 agent 注册完成
	waitUntil(t, 5*time.Second, "team roster should include worker", func() bool {
		members, err := tm.GetMemberAgents(teamID)
		if err != nil {
			return false
		}
		_, ok := members["worker-1"]
		return ok
	})

	// 5. 第二个 agent 加入后上报 status_report → 转发给第一个 agent
	peer := newSimAgent(t, env.agentAddr, "it-team-peer", "peer-host", func(string, map[string]any) map[string]any { return nil })
	peer.notify("team.join", map[string]any{"teamId": teamID, "memberId": "peer-1"})
	peer.waitForNotify(t, "inbox.message", 5*time.Second)

	peer.notify("team.status_report", map[string]any{
		"teamId": teamID, "memberId": "peer-1",
		"status": map[string]any{"hp": 77},
	})
	worker.waitForNotify(t, "inbox.message", 5*time.Second) // worker 收到 peer 的转发

	// 6. viewer 无 agents:manage 权限 → 403
	res = env.do(t, "POST", "/api/teams", viewer, map[string]any{"name": "sneaky"})
	if res.Status != http.StatusForbidden {
		t.Fatalf("viewer create team: expected 403, got %d %s", res.Status, res.Body)
	}
}
