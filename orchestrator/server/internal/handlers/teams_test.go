package handlers

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/gin-gonic/gin"
)

// setupTeamRouter 装配团队路由（asAdmin 绕过 JWT）：建团 + 列表 + 详情 + 团队范围推送。
func setupTeamRouter(t *testing.T) (*gin.Engine, *agent.TeamManager) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	tm := agent.NewTeamManager()
	h := NewTeamHandler(newDB(t), tm)
	r := gin.New()
	r.POST("/api/teams", asAdmin(7), h.HandleCreate)
	r.GET("/api/teams", asAdmin(7), h.HandleList)
	r.GET("/api/teams/:teamId", asAdmin(7), h.HandleGet)
	r.POST("/api/teams/:teamId/messages", asAdmin(7), h.HandleSendMessage)
	return r, tm
}

func TestTeamHandlerCreateSuccess(t *testing.T) {
	r, tm := setupTeamRouter(t)

	w := doJSON(r, "POST", "/api/teams", map[string]any{
		"name":        "  night-farm  ",
		"description": "夜间挂机协同",
	})
	if w.Code != http.StatusOK {
		t.Fatalf("create team: expected 200, got %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"success":true`) {
		t.Errorf("expected success:true, got %s", w.Body.String())
	}

	// 经 TeamManager 读回：创建者（admin，user_id=7）即 leader，名称已去除首尾空白
	var resp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil || resp.Data["teamId"] == nil {
		t.Fatalf("decode create response: %v (%s)", err, w.Body.String())
	}
	teamID, _ := resp.Data["teamId"].(string)
	info, err := tm.GetTeamInfo(teamID)
	if err != nil {
		t.Fatal(err)
	}
	if info["name"] != "night-farm" {
		t.Errorf("name should be trimmed, got %v", info["name"])
	}
	if info["leaderId"] != "admin" {
		t.Errorf("creator should be leader, got %v", info["leaderId"])
	}
	if info["description"] != "夜间挂机协同" {
		t.Errorf("description mismatch: %v", info["description"])
	}
	if !strings.HasPrefix(teamID, "team_") {
		t.Errorf("teamId should have team_ prefix, got %s", teamID)
	}
}

func TestTeamHandlerCreateRejectsBlankName(t *testing.T) {
	r, _ := setupTeamRouter(t)

	w := doJSON(r, "POST", "/api/teams", map[string]any{"name": "   "})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("blank name: expected 400, got %d %s", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "team name is required") {
		t.Errorf("unexpected error message: %s", w.Body.String())
	}
}

func TestTeamHandlerCreateRejectsMissingBody(t *testing.T) {
	r, _ := setupTeamRouter(t)

	// 缺少必填 name 字段 → binding 失败
	w := doJSON(r, "POST", "/api/teams", map[string]any{"description": "no name"})
	if w.Code != http.StatusBadRequest {
		t.Fatalf("missing name: expected 400, got %d %s", w.Code, w.Body.String())
	}
	// 非法 JSON
	w = doJSON(r, "POST", "/api/teams", nil)
	// body 为 nil 时 doJSON 发送空 body，ShouldBindJSON 应报 EOF → 400
	if w.Code != http.StatusBadRequest {
		t.Fatalf("empty body: expected 400, got %d %s", w.Code, w.Body.String())
	}
}

func TestTeamHandlerListAndGet(t *testing.T) {
	r, tm := setupTeamRouter(t)

	// 建两个团队 + 各加入成员（含一个未绑定设备的人肉成员）
	w := doJSON(r, "POST", "/api/teams", map[string]any{"name": "b-team"})
	teamB := mustTeamID(t, w)
	w = doJSON(r, "POST", "/api/teams", map[string]any{"name": "a-team"})
	teamA := mustTeamID(t, w)
	for _, memberID := range []string{"m-2", "m-1"} {
		if err := tm.JoinTeam(teamA, memberID, "agent-"+memberID); err != nil {
			t.Fatal(err)
		}
	}
	if err := tm.JoinTeam(teamA, "m-human", ""); err != nil {
		t.Fatal(err)
	}

	// 列表：按 teamId 升序，两个团队都在，成员数正确
	w = doJSON(r, "GET", "/api/teams", nil)
	if w.Code != http.StatusOK {
		t.Fatalf("list: %d %s", w.Code, w.Body.String())
	}
	var listResp struct {
		Data []map[string]any `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &listResp); err != nil || len(listResp.Data) != 2 {
		t.Fatalf("list should have 2 teams, got %v (%v)", listResp.Data, err)
	}
	if listResp.Data[0]["teamId"] != teamB || listResp.Data[1]["teamId"] != teamA {
		// teamId 为创建序号：b-team 先建即 team_1 < team_2，列表按 teamId 升序
		t.Errorf("list should be sorted by teamId: %v %v",
			listResp.Data[0]["teamId"], listResp.Data[1]["teamId"])
	}

	// 详情：基础字段 + memberAgents（成员 -> 设备）
	w = doJSON(r, "GET", "/api/teams/"+teamA, nil)
	if w.Code != http.StatusOK {
		t.Fatalf("get: %d %s", w.Code, w.Body.String())
	}
	var getResp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &getResp); err != nil {
		t.Fatalf("decode get: %v", err)
	}
	agents, _ := getResp.Data["memberAgents"].(map[string]any)
	if agents["m-1"] != "agent-m-1" || agents["m-human"] != "" {
		t.Errorf("memberAgents mismatch: %v", getResp.Data["memberAgents"])
	}
	// leader（admin）建团时即成为成员（agentID 为空），加上 3 个 JoinTeam 成员共 4 个
	if members, _ := getResp.Data["members"].([]any); len(members) != 4 {
		t.Errorf("members should have 4 entries, got %v", getResp.Data["members"])
	}

	// 不存在的团队 → 404
	if w := doJSON(r, "GET", "/api/teams/team_missing", nil); w.Code != http.StatusNotFound {
		t.Errorf("missing team should 404, got %d", w.Code)
	}
}

func TestTeamHandlerSendMessageDeliversToBoundMembers(t *testing.T) {
	r, tm := setupTeamRouter(t)

	// 捕获通知回调：TeamManager 收到消息时按序调用 notifier（FrameListener 侧下发 inbox.message）
	type delivery struct {
		agentID string
		msgID   string
		msgType string
	}
	// 缓冲要装下全部 JoinTeam 广播（每成员两条，含 agentID=""）+ 本次推送，
	// 否则同步回调在 tm.mu 持有期间阻塞会死锁测试
	delivered := make(chan delivery, 64)
	tm.SetMessageNotifier(func(agentID string, msg *agent.InboxMessage) {
		delivered <- delivery{agentID: agentID, msgID: msg.MsgID, msgType: msg.Type}
	})

	w := doJSON(r, "POST", "/api/teams", map[string]any{"name": "push-team"})
	teamID := mustTeamID(t, w)
	// 乱序加入：两个绑定设备的成员 + 一个未绑定设备的人肉成员
	for memberID, agentID := range map[string]string{
		"m-3": "agent-3", "m-1": "agent-1", "m-human": "",
	} {
		if err := tm.JoinTeam(teamID, memberID, agentID); err != nil {
			t.Fatal(err)
		}
	}
	// JoinTeam 自身会广播 team.joined / team.member_joined，先清空以便只断言本次推送
	for len(delivered) > 0 {
		<-delivered
	}

	w = doJSON(r, "POST", "/api/teams/"+teamID+"/messages", map[string]any{
		"messageType": "system.notice",
		"payload":     map[string]any{"title": "维护窗口", "window": "02:00-03:00"},
	})
	if w.Code != http.StatusOK {
		t.Fatalf("send: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data struct {
			TeamID    string `json:"teamId"`
			Total     int    `json:"total"`
			Delivered int    `json:"delivered"`
			Skipped   int    `json:"skipped"`
			Results   []struct {
				MemberID string `json:"memberId"`
				AgentID  string `json:"agentId"`
				MsgID    string `json:"msgId"`
				Skipped  string `json:"skipped"`
			} `json:"results"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode: %v (%s)", err, w.Body.String())
	}
	// leader admin 建团即成成员（agentID 为空）也被计入 total 并 skipped
	if resp.Data.Total != 4 || resp.Data.Delivered != 2 || resp.Data.Skipped != 2 {
		t.Errorf("summary mismatch: total=%d delivered=%d skipped=%d",
			resp.Data.Total, resp.Data.Delivered, resp.Data.Skipped)
	}
	if len(resp.Data.Results) != 4 {
		t.Fatalf("results should have 4 entries, got %d", len(resp.Data.Results))
	}
	// 结果按 memberID 升序：admin、m-1、m-3、m-human
	if resp.Data.Results[0].MemberID != "admin" || resp.Data.Results[0].Skipped == "" {
		t.Errorf("first result should be the leader with no device bound: %+v", resp.Data.Results[0])
	}
	if resp.Data.Results[1].MemberID != "m-1" || resp.Data.Results[1].MsgID == "" {
		t.Errorf("second result should be m-1 with a msgId: %+v", resp.Data.Results[1])
	}
	if resp.Data.Results[2].MemberID != "m-3" || resp.Data.Results[2].AgentID != "agent-3" {
		t.Errorf("third result should be m-3 bound to agent-3: %+v", resp.Data.Results[2])
	}
	if resp.Data.Results[3].MemberID != "m-human" || resp.Data.Results[3].Skipped == "" {
		t.Errorf("fourth result should be the unbound human member: %+v", resp.Data.Results[3])
	}

	// 通知回调触发次数 = 本次入队条数（已绑定设备成员各一条），载荷类型一致
	if n := len(delivered); n != 2 {
		t.Fatalf("notifier should fire 2 times, got %d", n)
	}
	seen := map[string]bool{}
	for {
		got := <-delivered
		if got.msgType != "system.notice" {
			t.Errorf("notifier should deliver the pushed type, got %+v", got)
		}
		if got.msgID == "" || got.agentID == "" {
			t.Errorf("notifier should carry agentId and msgId: %+v", got)
		}
		seen[got.agentID] = true
		if len(delivered) == 0 {
			break
		}
	}
	if !seen["agent-1"] || !seen["agent-3"] {
		t.Errorf("notifier should cover both bound members, got %v", seen)
	}

	// 消息落入对应 agent 的收件箱缓冲（离线时也不丢）
	messages := tm.GetMessages("agent-3", 10)
	notices := 0
	for _, msg := range messages {
		if msg["type"] == "system.notice" {
			notices++
			if got, _ := msg["payload"].(map[string]any)["title"]; got != "维护窗口" {
				t.Errorf("payload should be passed through, got %v", got)
			}
		}
	}
	if notices != 1 {
		t.Errorf("agent-3 inbox should hold exactly one notice, got %d in %v", notices, messages)
	}
	// 未绑定设备的成员不得收到本次推送（JoinTeam 自身可能为 agentId="" 建过条目）
	for _, msg := range tm.GetMessages("", 10) {
		if msg["type"] == "system.notice" {
			t.Errorf("the unbound member should not receive the push: %v", msg)
		}
	}
}

func TestTeamHandlerSendMessageRejects(t *testing.T) {
	r, _ := setupTeamRouter(t)

	// 团队不存在 → 404
	w := doJSON(r, "POST", "/api/teams/team_missing/messages", map[string]any{
		"messageType": "system.notice",
	})
	if w.Code != http.StatusNotFound {
		t.Errorf("missing team should 404, got %d %s", w.Code, w.Body.String())
	}
	// messageType 为空 → 400
	w = doJSON(r, "POST", "/api/teams/team_missing/messages", map[string]any{"payload": map[string]any{}})
	if w.Code != http.StatusBadRequest {
		t.Errorf("blank messageType should 400, got %d %s", w.Code, w.Body.String())
	}
}

// mustTeamID 从创建团队响应中取出 teamId（失败即 Fatal）
func mustTeamID(t *testing.T, w *httptest.ResponseRecorder) string {
	t.Helper()
	if w.Code != http.StatusOK {
		t.Fatalf("create team: %d %s", w.Code, w.Body.String())
	}
	var resp struct {
		Data map[string]any `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode create response: %v", err)
	}
	teamID, _ := resp.Data["teamId"].(string)
	if teamID == "" {
		t.Fatalf("create response has no teamId: %s", w.Body.String())
	}
	return teamID
}
