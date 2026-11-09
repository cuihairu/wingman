package agent

import (
	"log"
	"time"
)

// ========== Inbox & Team Handlers ==========

// handleInboxRegister handles inbox registration.
func (ac *agentConn) handleInboxRegister(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	if agentID == "" {
		agentID = ac.getAgentID()
	}

	// Send registration acknowledgment
	ac.sendNotify("inbox.register_ack", map[string]any{
		"success": true,
		"agentId": agentID,
	})

	log.Printf("[Inbox] Agent %s registered", agentID)
}

// handleInboxHeartbeat handles inbox heartbeat.
func (ac *agentConn) handleInboxHeartbeat(msg map[string]any) {
	agentID, _ := msg["agentId"].(string)
	if agentID != "" {
		ac.listener.registry.UpdateHeartbeat(agentID)
	}

	// Send heartbeat acknowledgment
	ac.sendNotify("inbox.heartbeat_ack", map[string]any{
		"timestamp": time.Now().UnixMilli(),
	})
}

// handleInboxAck handles message acknowledgment.
func (ac *agentConn) handleInboxAck(msg map[string]any) {
	msgID, _ := msg["msgId"].(string)
	agentID, _ := msg["agentId"].(string)
	if agentID == "" {
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.AckMessage(agentID, msgID); err != nil {
			log.Printf("[Inbox] Ack failed: %v", err)
		}
	}
}

// handleInboxReport handles task completion report.
func (ac *agentConn) handleInboxReport(msg map[string]any) {
	msgID, _ := msg["msgId"].(string)
	agentID, _ := msg["agentId"].(string)
	result, _ := msg["result"].(map[string]any)

	if agentID == "" {
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.ReportMessage(agentID, msgID, result); err != nil {
			log.Printf("[Inbox] Report failed: %v", err)
		}
	}
}

// handleTeamJoin handles team join request.
func (ac *agentConn) handleTeamJoin(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	agentID, _ := msg["agentId"].(string)

	if agentID == "" {
		agentID = ac.getAgentID()
	}

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.JoinTeam(teamID, memberID, agentID); err != nil {
			log.Printf("[Team] Join failed: %v", err)
			ac.sendNotify("team.error", map[string]any{
				"error": err.Error(),
			})
		}
	}
}

// handleTeamLeave handles team leave request.
func (ac *agentConn) handleTeamLeave(msg map[string]any) {
	// 快照一次并先做 nil 检查：SetTeamManager(nil)（测试/降级场景）下
	// 下方的 FindMemberByAgent 会空指针；且 readLoop 与 SetTeamManager
	// 并发，裸读 teamMgr 字段构成数据竞争（race detector 报警）。
	tm := ac.listener.GetTeamManager()
	if tm == nil {
		return
	}

	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	agentID, _ := msg["agentId"].(string)

	if agentID == "" {
		agentID = ac.getAgentID()
	}

	// If memberId is empty, resolve it from the member -> agent mapping,
	// falling back to using agentID directly (same-value mapping).
	if memberID == "" {
		if mid, found := tm.FindMemberByAgent(teamID, agentID); found {
			memberID = mid
		} else {
			memberID = agentID
		}
	}

	if err := tm.LeaveTeam(teamID, memberID); err != nil {
		log.Printf("[Team] Leave failed: %v", err)
	}
}

// handleTeamVoteCreate handles vote creation request.
func (ac *agentConn) handleTeamVoteCreate(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	proposerID, _ := msg["proposerId"].(string)
	subject, _ := msg["subject"].(string)
	timeoutVal, _ := msg["timeout"].(float64)

	if tm := ac.listener.GetTeamManager(); tm != nil {
		timeout := time.Duration(timeoutVal) * time.Millisecond
		if timeout == 0 {
			timeout = 30 * time.Second // Default 30 seconds
		}

		vote, err := tm.CreateVote(teamID, proposerID, subject, timeout)
		if err != nil {
			log.Printf("[Team] Vote create failed: %v", err)
			ac.sendNotify("team.error", map[string]any{
				"error": err.Error(),
			})
		} else {
			log.Printf("[Team] Vote created: %s", vote.VoteID)
		}
	}
}

// handleTeamVoteCast handles vote casting.
func (ac *agentConn) handleTeamVoteCast(msg map[string]any) {
	voteID, _ := msg["voteId"].(string)
	memberID, _ := msg["memberId"].(string)
	response, _ := msg["response"].(string)

	if tm := ac.listener.GetTeamManager(); tm != nil {
		if err := tm.CastVote(voteID, memberID, response); err != nil {
			log.Printf("[Team] Vote cast failed: %v", err)
		}
	}
}

// handleTeamStatusReport 处理成员状态上报，并把状态转发给团队其他成员。
// 转发以收件箱消息承载（type=team.status_report，payload 保留 teamId/memberId/status
// 原始字段）：runtime team 模块（team_module.cpp handleServerMessage）未定义独立的
// status 推送类型，收件箱通道按 messageType 分发给脚本。
func (ac *agentConn) handleTeamStatusReport(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	memberID, _ := msg["memberId"].(string)
	status, _ := msg["status"].(map[string]any)

	log.Printf("[Team] Status report from %s in team %s: %v", memberID, teamID, status)

	tm := ac.listener.GetTeamManager()
	if tm == nil {
		return
	}

	// 经 memberID -> agentID 映射转发给除上报者外的全部在线/离线成员
	memberAgents, err := tm.GetMemberAgents(teamID)
	if err != nil {
		return
	}
	for mid, agentID := range memberAgents {
		if mid == memberID {
			continue // 跳过上报者自己
		}
		tm.SendMessageToAgent(agentID, "team.status_report", map[string]any{
			"type":     "team.status_report",
			"teamId":   teamID,
			"memberId": memberID,
			"status":   status,
		})
	}
}

// handleTeamBroadcast handles broadcast message to all team members.
func (ac *agentConn) handleTeamBroadcast(msg map[string]any) {
	teamID, _ := msg["teamId"].(string)
	senderMemberID, _ := msg["memberId"].(string)
	message, _ := msg["message"].(map[string]any)

	log.Printf("[Team] Broadcast from %s in team %s: %v", senderMemberID, teamID, message)

	if tm := ac.listener.GetTeamManager(); tm != nil {
		// Get member -> agent mapping to resolve inbox targets
		if memberAgents, err := tm.GetMemberAgents(teamID); err == nil {
			// Send message to all members via inbox
			for memberID, agentID := range memberAgents {
				// Skip the sender
				if memberID == senderMemberID {
					continue
				}

				// Create broadcast message
				broadcastMsg := map[string]any{
					"type":     "team.broadcast_received",
					"teamId":   teamID,
					"senderId": senderMemberID,
					"message":  message,
				}

				tm.SendMessageToAgent(agentID, "team.broadcast_received", broadcastMsg)
			}
		}
	}
}
