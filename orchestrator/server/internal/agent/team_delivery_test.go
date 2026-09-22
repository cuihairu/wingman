package agent

import (
	"encoding/binary"
	"encoding/json"
	"net"
	"sync"
	"testing"
	"time"
)

// ---------- TeamManager 单元：notifier / RemoveAgent / CreateTeamNamed ----------

type recordedPush struct {
	agentID string
	msgType string
	msgID   string
}

type pushRecorder struct {
	mu    sync.Mutex
	calls []recordedPush
}

func (r *pushRecorder) note(agentID string, msg *InboxMessage) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.calls = append(r.calls, recordedPush{agentID: agentID, msgType: msg.Type, msgID: msg.MsgID})
}

func (r *pushRecorder) snapshot() []recordedPush {
	r.mu.Lock()
	defer r.mu.Unlock()
	return append([]recordedPush{}, r.calls...)
}

// SetMessageNotifier：消息入队后应同步回调通知者。
func TestSetMessageNotifierInvokedOnEnqueue(t *testing.T) {
	tm := NewTeamManager()
	rec := &pushRecorder{}
	tm.SetMessageNotifier(rec.note)

	msgID := tm.SendMessageToAgent("agent-a", "task.assign", map[string]any{"taskId": "t1"})

	calls := rec.snapshot()
	if len(calls) != 1 {
		t.Fatalf("expected 1 notifier call, got %d", len(calls))
	}
	if calls[0].agentID != "agent-a" || calls[0].msgType != "task.assign" || calls[0].msgID != msgID {
		t.Errorf("unexpected call: %+v (want msgID %s)", calls[0], msgID)
	}
	// 消息同时仍在内存缓冲中
	if msgs := tm.GetMessages("agent-a", 10); len(msgs) != 1 {
		t.Errorf("message should stay buffered, got %d", len(msgs))
	}
}

// CreateTeamNamed：创建者即 leader，名称/描述可经 GetTeamInfo 读回。
func TestCreateTeamNamedSemantics(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeamNamed("夜间小队", "挂机协同", "alice", "")
	if team.Name != "夜间小队" || team.Description != "挂机协同" || team.LeaderID != "alice" {
		t.Fatalf("unexpected team: %+v", team)
	}
	if team.Members["alice"] != "" {
		t.Errorf("creator should be mapped as leader member, got %v", team.Members)
	}

	info, err := tm.GetTeamInfo(team.TeamID)
	if err != nil {
		t.Fatal(err)
	}
	if info["name"] != "夜间小队" || info["description"] != "挂机协同" || info["leaderId"] != "alice" {
		t.Errorf("team info mismatch: %v", info)
	}
}

// RemoveAgent：清空收件箱、移除全部团队关系、通知剩余成员；空团队随之解散。
func TestRemoveAgentCleansAndNotifies(t *testing.T) {
	tm := NewTeamManager()
	rec := &pushRecorder{}
	tm.SetMessageNotifier(rec.note)

	team1 := tm.CreateTeam("L1", "agent-L1")
	if err := tm.JoinTeam(team1.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatal(err)
	}
	team2 := tm.CreateTeam("L2", "agent-L2")
	if err := tm.JoinTeam(team2.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatal(err)
	}
	// 同一 agent 独占的团队（唯一成员）→ RemoveAgent 后应解散
	team3 := tm.CreateTeam("m1", "agent-m1")

	tm.SendMessageToAgent("agent-m1", "task", map[string]any{"n": 1})

	tm.RemoveAgent("agent-m1")

	// 收件箱清空
	if msgs := tm.GetMessages("agent-m1", 10); len(msgs) != 0 {
		t.Errorf("inbox should be cleared, got %d messages", len(msgs))
	}
	// 两个团队均只剩 leader
	for _, tc := range []struct{ name, teamID string }{{"team1", team1.TeamID}, {"team2", team2.TeamID}} {
		members, err := tm.GetMemberAgents(tc.teamID)
		if err != nil {
			t.Fatalf("%s: %v", tc.name, err)
		}
		if len(members) != 1 {
			t.Errorf("%s: expected only leader left, got %v", tc.name, members)
		}
	}
	// 空团队解散
	if _, err := tm.GetTeamInfo(team3.TeamID); err == nil {
		t.Error("team with only the removed agent should be disbanded")
	}
	// 剩余成员（两队的 leader）收到 member_left，memberId 为被移除成员的 memberID
	waitForCond(t, time.Second, func() bool {
		return hasBufferedType(tm, "agent-L1", "team.member_left") && hasBufferedType(tm, "agent-L2", "team.member_left")
	}, "remaining leaders should each receive one member_left notice")
	msgs := tm.GetMessages("agent-L1", 10)
	last := msgs[len(msgs)-1]
	if last["type"] != "team.member_left" || last["payload"].(map[string]any)["memberId"] != "m1" {
		t.Errorf("unexpected member_left notice: %v", last)
	}
}

// hasBufferedType 判断 agent 的收件箱缓冲中是否存在指定类型的消息。
func hasBufferedType(tm *TeamManager, agentID, msgType string) bool {
	for _, m := range tm.GetMessages(agentID, 50) {
		if m["type"] == msgType {
			return true
		}
	}
	return false
}

// readNonPushFrame 读取下一帧非 inbox.message 推送的帧。消息实时推送（新功能）
// 可能插队出现在服务端回执之前，既有断言回执的测试经此跳过推送帧。
func readNonPushFrame(t *testing.T, conn net.Conn, wantSubstr string) frame {
	t.Helper()
	for i := 0; i < 10; i++ {
		conn.SetReadDeadline(time.Now().Add(2 * time.Second))
		f := readFrame(t, conn)
		var payload map[string]any
		if err := json.Unmarshal(f.body, &payload); err == nil && payload["type"] == "inbox.message" {
			continue
		}
		if contains(string(f.body), wantSubstr) {
			return f
		}
		t.Fatalf("expected frame containing %s, got %s", wantSubstr, string(f.body))
	}
	t.Fatalf("no frame containing %s after skipping push frames", wantSubstr)
	return frame{}
}

// ---------- 端到端（TCP）：inbox.message 实时推送 ----------

// 消息入队时 agent 在线 → 连接应收到 inbox.message notify（契约对齐 runtime inbox 模块）。
func TestInboxMessagePushedToOnlineAgent(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "push-agent")
	tm := listener.GetTeamManager()

	msgID := tm.SendMessageToAgent("push-agent", "task.assign", map[string]any{"taskId": "t1"})

	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	frame := readFrame(t, conn)
	if frame.msgType != Notify {
		t.Fatalf("expected Notify frame, got %d", frame.msgType)
	}
	var payload map[string]any
	if err := json.Unmarshal(frame.body, &payload); err != nil {
		t.Fatalf("decode push: %v", err)
	}
	if payload["type"] != "inbox.message" {
		t.Errorf("expected inbox.message, got %v", payload["type"])
	}
	if payload["messageType"] != "task.assign" {
		t.Errorf("expected messageType task.assign, got %v", payload["messageType"])
	}
	if payload["msgId"] != msgID {
		t.Errorf("expected msgId %s, got %v", msgID, payload["msgId"])
	}
	payloadObj, _ := payload["payload"].(map[string]any)
	if payloadObj == nil || payloadObj["taskId"] != "t1" {
		t.Errorf("unexpected payload: %v", payload["payload"])
	}
	if ts, _ := payload["timestamp"].(float64); ts <= 0 {
		t.Errorf("expected positive timestamp, got %v", payload["timestamp"])
	}
}

// team.join 全链路：加入确认（team.joined）应实时推送给新成员；
// 离线 leader 的 member_joined 通知保留在内存缓冲。
func TestTeamJoinDeliversJoinedMessageAndBuffersOffline(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "join-agent")
	tm := listener.GetTeamManager()
	team := tm.CreateTeam("boss", "agent-boss") // leader 离线

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.join", "teamId": team.TeamID, "memberId": "m1",
	})

	// 新成员实时收到 inbox.message（messageType=team.joined）
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	frame := readFrame(t, conn)
	var payload map[string]any
	if err := json.Unmarshal(frame.body, &payload); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if payload["type"] != "inbox.message" || payload["messageType"] != "team.joined" {
		t.Fatalf("expected inbox.message(team.joined), got %v / %v", payload["type"], payload["messageType"])
	}
	if payloadObj, _ := payload["payload"].(map[string]any); payloadObj == nil || payloadObj["leaderId"] != "boss" {
		t.Errorf("joined payload should carry leaderId, got %v", payload["payload"])
	}

	// 离线 leader：member_joined 应留在缓冲（不断连不清理）
	waitForCond(t, time.Second, func() bool {
		for _, m := range tm.GetMessages("agent-boss", 10) {
			if m["type"] == "team.member_joined" {
				return true
			}
		}
		return false
	}, "offline leader should have buffered member_joined")
}

// team.status_report 应转发给团队其他成员：在线成员实时推送、离线成员缓冲、上报者不收。
func TestTeamStatusReportForwardedToMembers(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	connM1 := dialAndRegister(t, addr, "sr-agent-m1")
	connM2 := dialAndRegister(t, addr, "sr-agent-m2")
	tm := listener.GetTeamManager()
	team := tm.CreateTeam("sr-leader", "sr-agent-leader") // leader 离线
	if err := tm.JoinTeam(team.TeamID, "m1", "sr-agent-m1"); err != nil {
		t.Fatal(err)
	}
	if err := tm.JoinTeam(team.TeamID, "m2", "sr-agent-m2"); err != nil {
		t.Fatal(err)
	}
	// 消费各自的 team.joined 推送
	connM1.SetReadDeadline(time.Now().Add(2 * time.Second))
	readFrame(t, connM1)
	connM2.SetReadDeadline(time.Now().Add(2 * time.Second))
	readFrame(t, connM2)

	sendMessage(t, connM1, Notify, 0, map[string]any{
		"type": "team.status_report", "teamId": team.TeamID, "memberId": "m1",
		"status": map[string]any{"hp": 88},
	})

	// m2 实时收到转发
	connM2.SetReadDeadline(time.Now().Add(2 * time.Second))
	frame := readFrame(t, connM2)
	var payload map[string]any
	if err := json.Unmarshal(frame.body, &payload); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if payload["type"] != "inbox.message" || payload["messageType"] != "team.status_report" {
		t.Fatalf("expected inbox.message(team.status_report), got %v / %v", payload["type"], payload["messageType"])
	}
	body, _ := payload["payload"].(map[string]any)
	if body == nil || body["memberId"] != "m1" || body["teamId"] != team.TeamID {
		t.Errorf("unexpected forwarded payload: %v", payload["payload"])
	}
	if status, _ := body["status"].(map[string]any); status == nil || status["hp"] != float64(88) {
		t.Errorf("status should be forwarded verbatim, got %v", body["status"])
	}

	// m1（上报者）不应收到任何转发：其推送队列里此前只有 m2 加入时的
	// member_joined，向 m1 发一条新消息探测帧序，读到 probe 前若出现
	// status_report 转发即为泄露。
	tm.SendMessageToAgent("sr-agent-m1", "probe", nil)
	foundProbe := false
	for i := 0; i < 5 && !foundProbe; i++ {
		connM1.SetReadDeadline(time.Now().Add(2 * time.Second))
		probe := readFrame(t, connM1)
		var probePayload map[string]any
		if err := json.Unmarshal(probe.body, &probePayload); err != nil {
			t.Fatalf("decode probe: %v", err)
		}
		switch probePayload["messageType"] {
		case "probe":
			foundProbe = true
		case "team.status_report":
			t.Errorf("reporter should not receive forwarded status_report, got %v", probePayload["messageType"])
		}
	}
	if !foundProbe {
		t.Error("probe message was never delivered to the reporter")
	}

	// 离线 leader 留在缓冲
	waitForCond(t, time.Second, func() bool {
		for _, m := range tm.GetMessages("sr-agent-leader", 10) {
			if m["type"] == "team.status_report" {
				return true
			}
		}
		return false
	}, "offline leader should have buffered status_report")
}

// ---------- 断连清理（readLoop defer → TeamManager.RemoveAgent） ----------

// agent 断连后：团队关系移除、收件箱清空、剩余成员收到 member_left。
func TestDisconnectCleansInboxAndTeamMembership(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "cleanup-agent")
	tm := listener.GetTeamManager()
	team := tm.CreateTeam("cl-leader", "cl-agent-leader")
	if err := tm.JoinTeam(team.TeamID, "cl-m1", "cleanup-agent"); err != nil {
		t.Fatal(err)
	}
	tm.SendMessageToAgent("cleanup-agent", "task", map[string]any{"n": 1})
	// 消费 joined 推送
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	readFrame(t, conn)

	conn.Close()
	waitForCond(t, 2*time.Second, func() bool {
		members, err := tm.GetMemberAgents(team.TeamID)
		return err == nil && len(members) == 1
	}, "disconnected agent should be removed from team")

	if msgs := tm.GetMessages("cleanup-agent", 10); len(msgs) != 0 {
		t.Errorf("inbox of disconnected agent should be cleared, got %d", len(msgs))
	}
	// 离线 leader 收到 member_left 缓冲
	waitForCond(t, time.Second, func() bool {
		for _, m := range tm.GetMessages("cl-agent-leader", 10) {
			if m["type"] == "team.member_left" {
				return true
			}
		}
		return false
	}, "offline leader should receive member_left notice")
}

// 重连竞态：同一 agentID 有两个活跃连接时，旧连接断开不应清掉新会话的状态。
func TestDisconnectCleanupSkippedWhenAnotherConnHoldsAgent(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn1 := dialAndRegister(t, addr, "dual-agent")
	conn2 := dialAndRegister(t, addr, "dual-agent")
	tm := listener.GetTeamManager()
	team := tm.CreateTeam("dual-leader", "dual-agent-leader")
	if err := tm.JoinTeam(team.TeamID, "dual-m1", "dual-agent"); err != nil {
		t.Fatal(err)
	}
	tm.SendMessageToAgent("dual-agent", "task", map[string]any{"n": 1})
	// 注：同一 agentID 的实时推送只投递给其中一条连接（getConnByAgent 返回
	// 单个连接），因此这里不消费推送帧，仅断言状态不被旧连接断连清理。

	// 旧连接断开：仍有 conn2 持有同一 agentID → 不清理
	conn1.Close()
	waitForCond(t, 2*time.Second, func() bool {
		registered, unregistered, _, _ := registry.snapshot()
		return len(registered) >= 2 && len(unregistered) >= 1
	}, "old connection should be unregistered")
	if members, err := tm.GetMemberAgents(team.TeamID); err != nil || len(members) != 2 {
		t.Fatalf("membership must survive old-conn disconnect, got %v err=%v", members, err)
	}
	if !hasBufferedType(tm, "dual-agent", "task") {
		t.Error("buffered messages must survive old-conn disconnect")
	}

	// 新连接也断开：此时才清理
	conn2.Close()
	waitForCond(t, 2*time.Second, func() bool {
		members, err := tm.GetMemberAgents(team.TeamID)
		return err == nil && len(members) == 1
	}, "membership should be cleaned once all conns are gone")
}

// ---------- 防御分支 ----------

// status_report 发到未知团队 → GetMemberAgents 失败即静默返回；teamMgr 为 nil
// 时同样直接返回。两种防御路径下连接都保持可用。
func TestTeamStatusReportUnknownTeamNoop(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "sr-unknown-agent")

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.status_report", "teamId": "team_404", "memberId": "x",
		"status": map[string]any{"hp": 1},
	})

	// teamMgr 为 nil 的防御路径
	listener.SetTeamManager(nil)
	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.status_report", "teamId": "team_404", "memberId": "x",
	})

	// 连接仍存活：PING → PONG
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	writeMessageHeader(t, conn, 4, Notify, 0)
	if _, err := conn.Write([]byte("PING")); err != nil {
		t.Fatalf("write ping: %v", err)
	}
	frame := readFrame(t, conn)
	if string(frame.body) != "PONG" {
		t.Fatalf("expected PONG after unknown-team status_report, got %q", string(frame.body))
	}
}

// SetTeamManager(nil) 后收到 team.leave 不应空指针（nil 检查前置回归），连接保持可用。
func TestTeamLeaveWithNilTeamManagerNoPanic(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "nil-tm-agent")

	listener.SetTeamManager(nil)

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "team.leave", "teamId": "team_1", "memberId": "x",
	})

	// 连接仍存活：PING → PONG
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	writeMessageHeader(t, conn, 4, Notify, 0)
	if _, err := conn.Write([]byte("PING")); err != nil {
		t.Fatalf("write ping: %v", err)
	}
	frame := readFrame(t, conn)
	if string(frame.body) != "PONG" {
		t.Fatalf("expected PONG after nil teamMgr leave, got %q", string(frame.body))
	}
}

// ---------- 字节序判定（纯函数两分支） ----------

func TestDetermineByteOrder(t *testing.T) {
	if determineByteOrder(true) != binary.LittleEndian {
		t.Error("little-endian host should select binary.LittleEndian")
	}
	if determineByteOrder(false) != binary.BigEndian {
		t.Error("big-endian host should select binary.BigEndian")
	}
	// 包级 listenerEndian 必须与宿主机实际字节序一致
	if hostIsLittleEndian() && listenerEndian != binary.LittleEndian {
		t.Error("listenerEndian should follow host byte order")
	}
	if !hostIsLittleEndian() && listenerEndian != binary.BigEndian {
		t.Error("listenerEndian should follow host byte order")
	}
}

// 编译期引用 net 包，保持与既有测试文件一致的导入面（readFrame/writeMessageHeader 依赖）。
var _ net.Conn = (net.Conn)(nil)
