package agent

import (
	"strings"
	"sync"
	"testing"
	"time"
)

// ---------- TeamManager 直接单测 ----------

func TestTeamLifecycle(t *testing.T) {
	tm := NewTeamManager()

	team := tm.CreateTeam("leader", "agent-leader")
	if team.TeamID != "team_1" || team.LeaderID != "leader" {
		t.Fatalf("unexpected team: %+v", team)
	}
	if _, ok := team.Members["leader"]; !ok {
		t.Fatal("leader should be a member")
	}

	// 加入
	if err := tm.JoinTeam(team.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatalf("join: %v", err)
	}
	// 重复加入
	if err := tm.JoinTeam(team.TeamID, "m1", "agent-m1"); err == nil {
		t.Error("duplicate join should fail")
	}
	// 未知团队
	if err := tm.JoinTeam("team_999", "m2", "a"); err == nil {
		t.Error("unknown team join should fail")
	}

	// 新成员与老成员都应收到 inbox 消息
	if msgs := tm.GetMessages("agent-m1", 10); len(msgs) == 0 {
		t.Error("new member should receive join confirmation")
	}
	if msgs := tm.GetMessages("agent-leader", 10); len(msgs) == 0 {
		t.Error("leader should be notified of new member")
	}

	// 团队信息
	info, err := tm.GetTeamInfo(team.TeamID)
	if err != nil {
		t.Fatalf("team info: %v", err)
	}
	if info["state"] != "idle" {
		t.Errorf("expected idle state, got %v", info["state"])
	}
	members, _ := info["members"].([]string)
	if len(members) != 2 {
		t.Errorf("expected 2 members, got %v", members)
	}

	// 离开
	if err := tm.LeaveTeam(team.TeamID, "m1"); err != nil {
		t.Fatalf("leave: %v", err)
	}
	if err := tm.LeaveTeam(team.TeamID, "ghost"); err == nil {
		t.Error("non-member leave should fail")
	}
	if err := tm.LeaveTeam("team_999", "m1"); err == nil {
		t.Error("unknown team leave should fail")
	}

	// 最后一个成员离开 → 团队解散
	if err := tm.LeaveTeam(team.TeamID, "leader"); err != nil {
		t.Fatalf("leader leave: %v", err)
	}
	if _, err := tm.GetTeamInfo(team.TeamID); err == nil {
		t.Error("disbanded team should not resolve")
	}
}

func TestVoteLifecycle(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")
	if err := tm.JoinTeam(team.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatal(err)
	}

	// 非成员不能发起投票
	if _, err := tm.CreateVote(team.TeamID, "outsider", "topic", time.Minute); err == nil {
		t.Error("outsider should not create vote")
	}
	// 未知团队
	if _, err := tm.CreateVote("team_404", "leader", "topic", time.Minute); err == nil {
		t.Error("unknown team should not create vote")
	}

	vote, err := tm.CreateVote(team.TeamID, "leader", "upgrade?", time.Minute)
	if err != nil {
		t.Fatalf("create vote: %v", err)
	}
	if vote.VoteID != "vote_1" {
		t.Errorf("unexpected vote id: %s", vote.VoteID)
	}

	// 团队进入 voting 状态
	info, _ := tm.GetTeamInfo(team.TeamID)
	if info["state"] != "voting" {
		t.Errorf("expected voting state, got %v", info["state"])
	}

	// 投票信息
	voteInfo, err := tm.GetVoteInfo(vote.VoteID)
	if err != nil {
		t.Fatalf("vote info: %v", err)
	}
	if voteInfo["active"] != true {
		t.Errorf("vote should be active: %+v", voteInfo)
	}
	if _, err := tm.GetVoteInfo("vote_404"); err == nil {
		t.Error("unknown vote should not resolve")
	}

	// 未知投票
	if err := tm.CastVote("vote_404", "m1", "yes"); err == nil {
		t.Error("unknown vote cast should fail")
	}

	// 两票全投 → 自动结束并回到 idle
	if err := tm.CastVote(vote.VoteID, "leader", "yes"); err != nil {
		t.Fatalf("cast leader: %v", err)
	}
	if err := tm.CastVote(vote.VoteID, "m1", "no"); err != nil {
		t.Fatalf("cast m1: %v", err)
	}

	info, _ = tm.GetTeamInfo(team.TeamID)
	if info["state"] != "idle" {
		t.Errorf("expected idle after vote ended, got %v", info["state"])
	}
	// 已删除的投票
	if _, err := tm.GetVoteInfo(vote.VoteID); err == nil {
		t.Error("ended vote should be cleaned up")
	}
	// 再投票 → vote not found
	if err := tm.CastVote(vote.VoteID, "m1", "yes"); err == nil {
		t.Error("casting on removed vote should fail")
	}

	// 成员应收到投票开始/结束消息
	if msgs := tm.GetMessages("agent-m1", 100); len(msgs) < 2 {
		t.Errorf("member should receive vote notifications, got %d", len(msgs))
	}
}

func TestVoteExpired(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")

	// 已过期的 deadline
	vote, err := tm.CreateVote(team.TeamID, "leader", "expired?", -time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	if err := tm.CastVote(vote.VoteID, "leader", "yes"); err == nil {
		t.Error("expired vote should reject casting")
	}
	// 投票被置为 inactive 后再投 → not active
	if err := tm.CastVote(vote.VoteID, "leader", "yes"); err == nil {
		t.Error("inactive vote should reject casting")
	}
}

func TestEndVoteUnknown(t *testing.T) {
	tm := NewTeamManager()
	if err := tm.endVote("vote_404"); err == nil {
		t.Error("ending unknown vote should fail")
	}
}

func TestMonitorVoteEndsOnTimeout(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")

	vote, err := tm.CreateVote(team.TeamID, "leader", "timeout?", 60*time.Millisecond)
	if err != nil {
		t.Fatal(err)
	}
	// monitorVote 在 timeout+100ms 后结束投票
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := tm.GetVoteInfo(vote.VoteID); err != nil {
			break // 投票已被清理
		}
		time.Sleep(20 * time.Millisecond)
	}
	if _, err := tm.GetVoteInfo(vote.VoteID); err == nil {
		t.Error("vote should be ended by monitor")
	}
}

func TestCastVoteTeamMissing(t *testing.T) {
	tm := NewTeamManager()
	// 手工放入一个指向不存在团队的投票
	tm.mu.Lock()
	vote := &VoteInfo{
		VoteID: "vote_orphan", TeamID: "team_404", ProposerID: "p",
		Responses: map[string]string{}, Active: true,
		Deadline: time.Now().Add(time.Minute),
	}
	tm.votes["vote_orphan"] = vote
	tm.mu.Unlock()

	// 全员投票判定时找不到团队 → 直接返回 nil
	if err := tm.CastVote("vote_orphan", "p", "yes"); err != nil {
		t.Errorf("missing team should not error: %v", err)
	}
}

func TestInboxOperations(t *testing.T) {
	tm := NewTeamManager()

	// 未知 agent → 空列表
	if msgs := tm.GetMessages("ghost", 10); len(msgs) != 0 {
		t.Errorf("unknown agent should have no messages, got %v", msgs)
	}

	msgID := tm.SendMessageToAgent("a1", "task", map[string]any{"x": 1})
	if msgID == "" {
		t.Fatal("message id should not be empty")
	}
	tm.SendMessageToAgent("a1", "task", map[string]any{"x": 2})

	msgs := tm.GetMessages("a1", 10)
	if len(msgs) != 2 {
		t.Fatalf("expected 2 messages, got %d", len(msgs))
	}

	// limit 截断
	if msgs := tm.GetMessages("a1", 1); len(msgs) != 1 {
		t.Errorf("limit should cap results, got %d", len(msgs))
	}

	// ack
	if err := tm.AckMessage("a1", msgID); err != nil {
		t.Fatalf("ack: %v", err)
	}
	if err := tm.AckMessage("a1", "msg_404"); err == nil {
		t.Error("unknown message ack should fail")
	}
	if err := tm.AckMessage("ghost", msgID); err == nil {
		t.Error("unknown agent ack should fail")
	}

	// 已 ack 的消息不再返回
	msgs = tm.GetMessages("a1", 10)
	if len(msgs) != 1 {
		t.Fatalf("acked message should be hidden, got %d", len(msgs))
	}

	// report 删除
	if err := tm.ReportMessage("a1", msgs[0]["msgId"].(string), map[string]any{"done": true}); err != nil {
		t.Fatalf("report: %v", err)
	}
	if err := tm.ReportMessage("a1", "msg_404", nil); err == nil {
		t.Error("unknown message report should fail")
	}
	if msgs := tm.GetMessages("a1", 10); len(msgs) != 0 {
		t.Errorf("reported message should be removed, got %d", len(msgs))
	}
}

// ---------- 经 listener 的 inbox/team 处理器 ----------

func TestInboxHandlersViaListener(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "inbox-agent")
	tm := listener.GetTeamManager()

	// inbox.register（无 agentId → 回退到连接注册的 agentID）
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.register"})
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	ack := readFrame(t, conn)
	if got := string(ack.body); !contains(got, "inbox.register_ack") || !contains(got, "inbox-agent") {
		t.Fatalf("unexpected register ack: %s", got)
	}

	// inbox.heartbeat（携带 agentId）→ 心跳更新 + ack
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.heartbeat", "agentId": "inbox-agent"})
	ack = readFrame(t, conn)
	if !contains(string(ack.body), "inbox.heartbeat_ack") {
		t.Fatalf("unexpected heartbeat ack: %s", string(ack.body))
	}
	waitForCond(t, time.Second, func() bool {
		_, _, hb, _ := registry.snapshot()
		for _, id := range hb {
			if id == "inbox-agent" {
				return true
			}
		}
		return false
	}, "inbox.heartbeat should update registry heartbeat")

	// inbox.heartbeat 无 agentId → 仅 ack
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.heartbeat"})
	ack = readFrame(t, conn)
	if !contains(string(ack.body), "inbox.heartbeat_ack") {
		t.Fatalf("unexpected no-id heartbeat ack: %s", string(ack.body))
	}

	// inbox.ack / inbox.report
	msgID := tm.SendMessageToAgent("inbox-agent", "task", map[string]any{"n": 1})
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.ack", "agentId": "inbox-agent", "msgId": msgID})
	if err := tm.AckMessage("ghost-agent", "msg_404"); err == nil {
		t.Error("unknown ack should fail")
	}
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "inbox.report", "agentId": "inbox-agent", "msgId": msgID, "result": map[string]any{"ok": true}})
	if err := tm.ReportMessage("ghost-agent", "msg_404", nil); err == nil {
		t.Error("unknown report should fail")
	}
	time.Sleep(100 * time.Millisecond)
}

func TestTeamHandlersViaListener(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "team-agent")
	tm := listener.GetTeamManager()

	// 预建团队：leader = team-agent
	team := tm.CreateTeam("team-agent", "team-agent")
	teamID := team.TeamID

	// team.join：另一成员加入
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.join", "teamId": teamID, "memberId": "m2", "agentId": "agent-m2",
	})
	waitForCond(t, time.Second, func() bool {
		info, err := tm.GetTeamInfo(teamID)
		if err != nil {
			return false
		}
		members, _ := info["members"].([]string)
		return len(members) == 2
	}, "team.join not processed")

	// team.join 失败（未知团队）→ team.error 回执
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.join", "teamId": "team_404", "memberId": "x", "agentId": "y"})
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	errReply := readFrame(t, conn)
	if !contains(string(errReply.body), "team.error") {
		t.Fatalf("expected team.error reply, got %s", string(errReply.body))
	}

	// team.vote_create：leader 发起（成功仅记录日志）
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.vote_create", "teamId": teamID, "proposerId": "team-agent", "subject": "lunch?", "timeout": 5000,
	})
	// team.vote_create 失败（非成员发起）→ team.error
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.vote_create", "teamId": teamID, "proposerId": "outsider", "subject": "x",
	})
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	errReply = readFrame(t, conn)
	if !contains(string(errReply.body), "team.error") {
		t.Fatalf("expected team.error for outsider vote, got %s", string(errReply.body))
	}

	// team.vote_cast：leader 投票（未知 voteId 仅记录）
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.vote_cast", "voteId": "vote_404", "memberId": "team-agent", "response": "yes",
	})
	time.Sleep(100 * time.Millisecond)

	// team.status_report
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.status_report", "teamId": teamID, "memberId": "m2", "status": map[string]any{"busy": true},
	})
	time.Sleep(100 * time.Millisecond)

	// team.broadcast：m2 应收到消息（memberId → agentId 同值映射）
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.broadcast", "teamId": teamID, "memberId": "team-agent", "message": map[string]any{"text": "hello"},
	})
	waitForCond(t, time.Second, func() bool {
		msgs := tm.GetMessages("agent-m2", 50)
		for _, m := range msgs {
			if m["type"] == "task" {
				continue
			}
			payload, _ := m["payload"].(map[string]any)
			if payload != nil && payload["type"] == "team.broadcast_received" {
				return true
			}
		}
		return false
	}, "team.broadcast not delivered to other member")

	// team.leave：m2 离开（memberId 缺省 = agentID）
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.leave", "teamId": teamID, "agentId": "agent-m2"})
	waitForCond(t, time.Second, func() bool {
		info, err := tm.GetTeamInfo(teamID)
		if err != nil {
			return false
		}
		members, _ := info["members"].([]string)
		return len(members) == 1
	}, "team.leave not processed")

	// team.leave 失败（未知团队）
	sendMessage(t, conn, Notify, 0, map[string]any{"type": "team.leave", "teamId": "team_404", "memberId": "x"})
	time.Sleep(100 * time.Millisecond)
}

func TestTeamBroadcastUnknownTeam(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "bcast-agent")

	// 未知团队 / 未知成员 → 仅记录
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.broadcast", "teamId": "team_404", "memberId": "x", "message": map[string]any{"a": 1},
	})
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.status_report", "teamId": "team_404", "memberId": "x", "status": map[string]any{},
	})
	time.Sleep(100 * time.Millisecond)

	// 连接仍应存活
	writeMessageHeader(t, conn, 4, Notify, 0)
	conn.Write([]byte("PING"))
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	pong := readFrame(t, conn)
	if string(pong.body) != "PONG" {
		t.Fatalf("listener should survive unknown team messages, got %q", string(pong.body))
	}
}

// ---------- 并发 ----------

func TestTeamManagerConcurrent(t *testing.T) {
	tm := NewTeamManager()
	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			team := tm.CreateTeam("leader", "agent-leader")
			tm.JoinTeam(team.TeamID, "member", "agent-m")
			tm.SendMessageToAgent("agent-m", "task", map[string]any{"i": i})
			tm.GetMessages("agent-m", 10)
			tm.LeaveTeam(team.TeamID, "member")
			tm.LeaveTeam(team.TeamID, "leader")
		}(i)
	}
	wg.Wait()
}

func contains(s, sub string) bool {
	return strings.Contains(s, sub)
}
