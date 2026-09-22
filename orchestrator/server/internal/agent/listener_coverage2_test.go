package agent

import (
	"strings"
	"testing"
	"time"
)

// SendCommand 的 JSON 序列化失败分支：payload 含不可序列化类型。
func TestSendCommandMarshalFailure(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "marshal-agent")
	_ = conn

	ac := findAgentConn(t, listener, "marshal-agent")
	_, err := ac.SendCommand("bad.command", map[string]any{"fn": func() {}})
	if err == nil {
		t.Fatal("marshalling func value should fail")
	}
}

// SendCommand 的写失败分支：连接关闭后写入报错。
func TestSendCommandWriteFailure(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "write-agent")
	ac := findAgentConn(t, listener, "write-agent")

	conn.Close()
	time.Sleep(100 * time.Millisecond)

	done := make(chan error, 1)
	go func() {
		_, err := ac.SendCommand("some.command", nil)
		done <- err
	}()
	select {
	case err := <-done:
		if err == nil {
			t.Fatal("write on closed conn should fail")
		}
	case <-time.After(3 * time.Second):
		t.Fatal("SendCommand did not return on write failure")
	}
}

// readLoop defer：连接断开时未决命令应收到 "connection closed" 错误。
func TestReadLoopFailsPendingOnDisconnect(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "pending-agent")
	ac := findAgentConn(t, listener, "pending-agent")

	done := make(chan error, 1)
	go func() {
		_, err := ac.SendCommand("never.responds", nil)
		done <- err
	}()

	// 等命令挂起后断开客户端连接
	time.Sleep(150 * time.Millisecond)
	conn.Close()

	select {
	case err := <-done:
		if err == nil || !strings.Contains(err.Error(), "connection closed") {
			t.Fatalf("expected connection closed error, got %v", err)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("pending command did not fail on disconnect")
	}
}

// readLoop 的 body 读取失败分支：header 声明的长度大于实际发送。
func TestReadLoopTruncatedBody(t *testing.T) {
	registry := newRecordingRegistry()
	_, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "truncate-agent")

	// 声明 16 字节 body，只写 4 字节即关闭
	writeMessageHeader(t, conn, 16, Notify, 0)
	if _, err := conn.Write([]byte("abcd")); err != nil {
		t.Fatalf("partial write: %v", err)
	}
	conn.Close()
	time.Sleep(150 * time.Millisecond) // 服务器侧 ReadFull 报错并清理
}

// handleInboxReport 的失败日志分支：上报未知消息 ID。
func TestInboxReportUnknownMessageLogsError(t *testing.T) {
	registry := newRecordingRegistry()
	listener, addr := startTestListener(t, registry, &recordingBroadcaster{})
	conn := dialAndRegister(t, addr, "report-err-agent")
	tm := listener.GetTeamManager()

	sendMessage(t, conn, Notify, 0, map[string]any{
		"type": "inbox.report", "agentId": "report-err-agent", "msgId": "msg_nonexistent",
		"result": map[string]any{"ok": true},
	})
	waitForCond(t, time.Second, func() bool {
		return len(tm.GetMessages("report-err-agent", 10)) == 0
	}, "unknown report should not create messages")
}

// GetVoteInfo：含已投票 responses 的快照路径。
func TestGetVoteInfoWithResponses(t *testing.T) {
	tm := NewTeamManager()
	team := tm.CreateTeam("leader", "agent-leader")
	if err := tm.JoinTeam(team.TeamID, "m1", "agent-m1"); err != nil {
		t.Fatal(err)
	}
	vote, err := tm.CreateVote(team.TeamID, "leader", "partial?", time.Minute)
	if err != nil {
		t.Fatal(err)
	}
	// 只投一票（未全员）→ vote 仍存活
	if err := tm.CastVote(vote.VoteID, "leader", "yes"); err != nil {
		t.Fatal(err)
	}

	info, err := tm.GetVoteInfo(vote.VoteID)
	if err != nil {
		t.Fatal(err)
	}
	responses, _ := info["responses"].(map[string]string)
	if responses["leader"] != "yes" {
		t.Errorf("expected leader response in snapshot, got %v", responses)
	}
}
