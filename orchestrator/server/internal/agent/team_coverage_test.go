package agent

import (
	"testing"
	"time"
)

// ---------- TeamManager 辅助方法覆盖 ----------

func TestTeamManagerHelpersCoverage(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")
	if err := tm.JoinTeam(team.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatal(err)
	}

	// getMemberList 加锁版本
	if got := tm.getMemberList(team); len(got) != 2 {
		t.Errorf("getMemberList: expected 2 members, got %v", got)
	}

	// FindMemberByAgent 命中 / agent 不存在 / team 不存在
	if mid, ok := tm.FindMemberByAgent(team.TeamID, "agent-m1"); !ok || mid != "m1" {
		t.Errorf("FindMemberByAgent: expected m1, got %q ok=%v", mid, ok)
	}
	if _, ok := tm.FindMemberByAgent(team.TeamID, "agent-404"); ok {
		t.Error("unknown agent should not resolve")
	}
	if _, ok := tm.FindMemberByAgent("team_404", "agent-m1"); ok {
		t.Error("unknown team should not resolve")
	}

	// GetMemberAgents 命中 / team 不存在
	if _, err := tm.GetMemberAgents("team_404"); err == nil {
		t.Error("unknown team GetMemberAgents should fail")
	}
	agents, err := tm.GetMemberAgents(team.TeamID)
	if err != nil {
		t.Fatal(err)
	}
	if agents["m1"] != "agent-m1" || agents["leader"] != "agent-leader" {
		t.Errorf("unexpected member-agent mapping: %v", agents)
	}

	// GetVoteInfo 未知投票
	if _, err := tm.GetVoteInfo("vote_404"); err == nil {
		t.Error("unknown vote should fail")
	}
}

// monitorVote 在投票被提前清理后应直接返回，不修改团队状态
func TestMonitorVoteEarlyCleanup(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")
	vote, err := tm.CreateVote(team.TeamID, "leader", "gone?", 60*time.Millisecond)
	if err != nil {
		t.Fatal(err)
	}

	// 在 monitor 唤醒前删除投票
	tm.mu.Lock()
	delete(tm.votes, vote.VoteID)
	tm.mu.Unlock()

	time.Sleep(300 * time.Millisecond)

	info, err := tm.GetTeamInfo(team.TeamID)
	if err != nil {
		t.Fatal(err)
	}
	if info["state"] != "voting" {
		t.Errorf("team state should stay voting after early cleanup, got %v", info["state"])
	}
}

// ---------- listener 处理器缺省回退分支 ----------

func TestInboxHandlersFallbackAgentID(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "fb-agent")
	tm := listener.GetTeamManager()

	// inbox.ack 无 agentId → 回退连接 agentID；未知 msgId → 失败日志分支
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.ack", "msgId": "msg_404"})

	// inbox.report 无 agentId → 回退连接 agentID 并删除消息
	msgID := tm.SendMessageToAgent("fb-agent", "task", map[string]any{"n": 1})
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.report", "msgId": msgID, "result": map[string]any{"ok": true}})
	waitForCond(t, time.Second, func() bool {
		return len(tm.GetMessages("fb-agent", 10)) == 0
	}, "inbox.report fallback should remove message")

	// inbox.ack 无 agentId → 回退连接 agentID 成功 ack
	msgID2 := tm.SendMessageToAgent("fb-agent", "task", map[string]any{"n": 2})
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.ack", "msgId": msgID2})
	waitForCond(t, time.Second, func() bool {
		return len(tm.GetMessages("fb-agent", 10)) == 0
	}, "inbox.ack fallback should hide message")
}

func TestTeamJoinLeaveFallbackIDs(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "fb-agent")
	tm := listener.GetTeamManager()

	// team.join 无 agentId → 回退连接 agentID
	team := tm.CreateTeam("leader2", "agent-leader2")
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.join", "teamId": team.TeamID, "memberId": "fb-agent"})
	waitForCond(t, time.Second, func() bool {
		agents, err := tm.GetMemberAgents(team.TeamID)
		return err == nil && agents["fb-agent"] == "fb-agent"
	}, "team.join fallback should use connection agentID")

	// team.leave 只传 agentId 且 memberID 与 agentID 不同 → 应通过映射反查 memberID
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.join", "teamId": team.TeamID, "memberId": "member-x", "agentId": "agent-x"})
	waitForCond(t, time.Second, func() bool {
		agents, err := tm.GetMemberAgents(team.TeamID)
		return err == nil && agents["member-x"] == "agent-x"
	}, "team.join with explicit ids should be applied")

	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.leave", "teamId": team.TeamID, "agentId": "agent-x"})
	waitForCond(t, time.Second, func() bool {
		agents, err := tm.GetMemberAgents(team.TeamID)
		return err == nil && len(agents) == 2
	}, "team.leave should resolve memberID via agent mapping")

	// team.leave 反查失败 → memberId = agentID 同值兜底（未知团队仅记录日志）
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.leave", "teamId": "team_404", "agentId": "ghost"})
	time.Sleep(100 * time.Millisecond)
}

// ---------- readLoop Response 分发分支 ----------

func TestDispatchResponseBranches(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "orphan-agent")

	// 无人等待的 Response 帧 → orphan 分支
	sendMessage(t, conn, Response, 42, map[string]any{"type": "resp"})

	// 非法 JSON body 的 Response → 解析错误仍需安全处理
	writeMessageHeader(t, conn, 4, Response, 43)
	if _, err := conn.Write([]byte("\xff\xfe\xfd\xfc")); err != nil {
		t.Fatalf("write invalid body: %v", err)
	}

	// 连接仍应存活
	writeMessageHeader(t, conn, 4, Notify, 0)
	if _, err := conn.Write([]byte("PING")); err != nil {
		t.Fatalf("write ping: %v", err)
	}
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if string(pong.body) != "PONG" {
		t.Fatalf("expected PONG, got %q", string(pong.body))
	}
}
