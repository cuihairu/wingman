package agent

import (
	"fmt"
	"log"
	"sync"
	"time"
)

// ========== Team Management ==========

// TeamInfo 团队信息
type TeamInfo struct {
	TeamID    string
	LeaderID  string
	Members   map[string]string // memberID -> agentID
	State     string           // "idle", "voting", "working"
	CreatedAt time.Time
	UpdatedAt time.Time
	mu        sync.RWMutex
}

// VoteInfo 投票信息
type VoteInfo struct {
	VoteID      string
	TeamID      string
	ProposerID  string
	Subject     string
	Responses   map[string]string // memberID -> response
	CreatedAt   time.Time
	Deadline    time.Time
	Active      bool
	mu          sync.RWMutex
}

// InboxMessage 收件箱消息
type InboxMessage struct {
	MsgID     string
	AgentID   string
	Type      string
	Payload   map[string]any
	Timestamp time.Time
	Acked     bool
}

// TeamManager 团队管理器
type TeamManager struct {
	teams      map[string]*TeamInfo
	votes      map[string]*VoteInfo
	inboxes    map[string]map[string]*InboxMessage // agentID -> msgID -> message
	mu         sync.RWMutex
	nextTeamID int64
	nextVoteID int64
	nextMsgID  int64
}

// NewTeamManager 创建团队管理器
func NewTeamManager() *TeamManager {
	return &TeamManager{
		teams:      make(map[string]*TeamInfo),
		votes:      make(map[string]*VoteInfo),
		inboxes:    make(map[string]map[string]*InboxMessage),
		nextTeamID: 1,
		nextVoteID: 1,
		nextMsgID:  1,
	}
}

// CreateTeam 创建团队
func (tm *TeamManager) CreateTeam(leaderID, leaderAgentID string) *TeamInfo {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	teamID := fmt.Sprintf("team_%d", tm.nextTeamID)
	tm.nextTeamID++

	team := &TeamInfo{
		TeamID:    teamID,
		LeaderID:  leaderID,
		Members:   make(map[string]string),
		State:     "idle",
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}
	team.Members[leaderID] = leaderAgentID

	tm.teams[teamID] = team

	log.Printf("[Team] Created team %s with leader %s", teamID, leaderID)
	return team
}

// JoinTeam 加入团队
func (tm *TeamManager) JoinTeam(teamID, memberID, agentID string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return fmt.Errorf("team not found")
	}

	team.mu.Lock()
	defer team.mu.Unlock()

	if _, exists := team.Members[memberID]; exists {
		return fmt.Errorf("already a member")
	}

	team.Members[memberID] = agentID
	team.UpdatedAt = time.Now()

	log.Printf("[Team] Member %s (agent %s) joined team %s", memberID, agentID, teamID)

	// 通知所有团队成员
	for mid, aid := range team.Members {
		if mid == memberID {
			// 给新成员发送加入确认
			tm.sendToInbox(aid, map[string]any{
				"type":      "team.joined",
				"teamId":    teamID,
				"leaderId":  team.LeaderID,
				"memberId":  memberID,
				"members":   tm.getMemberList(team),
			})
		} else {
			// 通知其他成员有新成员加入
			tm.sendToInbox(aid, map[string]any{
				"type":      "team.member_joined",
				"teamId":    teamID,
				"memberId":  memberID,
			})
		}
	}

	return nil
}

// LeaveTeam 离开团队
func (tm *TeamManager) LeaveTeam(teamID, memberID string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return fmt.Errorf("team not found")
	}

	team.mu.Lock()
	defer team.mu.Unlock()

	agentID, exists := team.Members[memberID]
	if !exists {
		return fmt.Errorf("not a member")
	}

	delete(team.Members, memberID)
	team.UpdatedAt = time.Now()

	// 发送离开确认
	tm.sendToInbox(agentID, map[string]any{
		"type":   "team.left",
		"teamId": teamID,
	})

	// 通知其他成员
	for _, aid := range team.Members {
		tm.sendToInbox(aid, map[string]any{
			"type":      "team.member_left",
			"teamId":    teamID,
			"memberId":  memberID,
		})
	}

	log.Printf("[Team] Member %s left team %s", memberID, teamID)

	// 如果团队为空，删除团队
	if len(team.Members) == 0 {
		delete(tm.teams, teamID)
		log.Printf("[Team] Disbanded empty team %s", teamID)
	}

	return nil
}

// CreateVote 创建投票
func (tm *TeamManager) CreateVote(teamID, proposerID, subject string, timeout time.Duration) (*VoteInfo, error) {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return nil, fmt.Errorf("team not found")
	}

	team.mu.RLock()
	_, isMember := team.Members[proposerID]
	team.mu.RUnlock()

	if !isMember {
		return nil, fmt.Errorf("not a team member")
	}

	voteID := fmt.Sprintf("vote_%d", tm.nextVoteID)
	tm.nextVoteID++

	vote := &VoteInfo{
		VoteID:    voteID,
		TeamID:    teamID,
		ProposerID: proposerID,
		Subject:   subject,
		Responses: make(map[string]string),
		CreatedAt: time.Now(),
		Deadline:  time.Now().Add(timeout),
		Active:    true,
	}

	tm.votes[voteID] = vote

	// 更新团队状态
	team.mu.Lock()
	team.State = "voting"
	team.UpdatedAt = time.Now()
	team.mu.Unlock()

	// 通知所有团队成员
	team.mu.RLock()
	members := make(map[string]string)
	for k, v := range team.Members {
		members[k] = v
	}
	team.mu.RUnlock()

	for _, agentID := range members {
		tm.sendToInbox(agentID, map[string]any{
			"type":      "team.vote_started",
			"voteId":    voteID,
			"teamId":    teamID,
			"proposerId": proposerID,
			"subject":   subject,
			"deadline":  vote.Deadline.UnixMilli(),
		})
	}

	log.Printf("[Team] Created vote %s in team %s: %s", voteID, teamID, subject)

	// 启动超时检查
	go tm.monitorVote(voteID, timeout)

	return vote, nil
}

// CastVote 投票
func (tm *TeamManager) CastVote(voteID, memberID, response string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	vote, ok := tm.votes[voteID]
	if !ok {
		return fmt.Errorf("vote not found")
	}

	vote.mu.Lock()
	defer vote.mu.Unlock()

	if !vote.Active {
		return fmt.Errorf("vote is not active")
	}

	if time.Now().After(vote.Deadline) {
		vote.Active = false
		return fmt.Errorf("vote has expired")
	}

	vote.Responses[memberID] = response

	log.Printf("[Team] Member %s cast vote for %s: %s", memberID, voteID, response)

	// 检查是否所有人都已投票
	team, ok := tm.teams[vote.TeamID]
	if !ok {
		return nil
	}

	team.mu.RLock()
	memberCount := len(team.Members)
	team.mu.RUnlock()

	if len(vote.Responses) >= memberCount {
		return tm.endVote(voteID)
	}

	return nil
}

// endVote 结束投票
func (tm *TeamManager) endVote(voteID string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	vote, ok := tm.votes[voteID]
	if !ok {
		return fmt.Errorf("vote not found")
	}

	vote.mu.Lock()
	vote.Active = false
	responses := make(map[string]string)
	for k, v := range vote.Responses {
		responses[k] = v
	}
	vote.mu.Unlock()

	// 更新团队状态
	team, ok := tm.teams[vote.TeamID]
	if ok {
		team.mu.Lock()
		team.State = "idle"
		team.UpdatedAt = time.Now()
		team.mu.Unlock()
	}

	// 通知所有成员投票结果
	team.mu.RLock()
	members := make(map[string]string)
	for k, v := range team.Members {
		members[k] = v
	}
	team.mu.RUnlock()

	result := map[string]any{
		"voteId":    voteID,
		"teamId":    vote.TeamID,
		"subject":   vote.Subject,
		"result":    responses,
		"active":    false,
		"timestamp": time.Now().UnixMilli(),
	}

	for _, agentID := range members {
		tm.sendToInbox(agentID, map[string]any{
			"type":   "team.vote_ended",
			"voteId": voteID,
			"result": result,
		})
	}

	log.Printf("[Team] Vote %s ended with %d responses", voteID, len(responses))

	// 清理已结束的投票
	delete(tm.votes, voteID)

	return nil
}

// monitorVote 监控投票超时
func (tm *TeamManager) monitorVote(voteID string, timeout time.Duration) {
	time.Sleep(timeout + 100*time.Millisecond) // 稍微延长一点确保所有投票都被处理

	tm.mu.RLock()
	vote, exists := tm.votes[voteID]
	if !exists {
		tm.mu.RUnlock()
		return
	}
	tm.mu.RUnlock()

	vote.mu.RLock()
	active := vote.Active
	vote.mu.RUnlock()

	if active {
		tm.endVote(voteID)
	}
}

// getMemberList 获取成员列表（辅助函数）
func (tm *TeamManager) getMemberList(team *TeamInfo) []string {
	team.mu.RLock()
	defer team.mu.RUnlock()

	members := make([]string, 0, len(team.Members))
	for memberID := range team.Members {
		members = append(members, memberID)
	}
	return members
}

// ========== Inbox Management ==========

// SendMessageToAgent 发送消息到指定 agent 的收件箱
func (tm *TeamManager) SendMessageToAgent(agentID, msgType string, payload map[string]any) string {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	msgID := fmt.Sprintf("msg_%d", tm.nextMsgID)
	tm.nextMsgID++

	msg := &InboxMessage{
		MsgID:     msgID,
		AgentID:   agentID,
		Type:      msgType,
		Payload:   payload,
		Timestamp: time.Now(),
		Acked:     false,
	}

	if _, ok := tm.inboxes[agentID]; !ok {
		tm.inboxes[agentID] = make(map[string]*InboxMessage)
	}

	tm.inboxes[agentID][msgID] = msg

	log.Printf("[Inbox] Sent message %s to agent %s (type: %s)", msgID, agentID, msgType)

	return msgID
}

// sendToInbox 内部辅助函数
func (tm *TeamManager) sendToInbox(agentID string, data map[string]any) {
	msgID := tm.SendMessageToAgent(agentID, data["type"].(string), data)

	// 添加 msgId 到数据中
	data["msgId"] = msgID
	data["timestamp"] = time.Now().UnixMilli()
}

// GetMessages 获取 agent 的待处理消息
func (tm *TeamManager) GetMessages(agentID string, limit int) []map[string]any {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if _, ok := tm.inboxes[agentID]; !ok {
		return []map[string]any{}
	}

	messages := make([]map[string]any, 0)
	count := 0

	for msgID, msg := range tm.inboxes[agentID] {
		if msg.Acked || count >= limit {
			continue
		}

		messages = append(messages, map[string]any{
			"msgId":     msgID,
			"type":      msg.Type,
			"payload":   msg.Payload,
			"timestamp": msg.Timestamp.UnixMilli(),
		})
		count++
	}

	return messages
}

// AckMessage 确认消息
func (tm *TeamManager) AckMessage(agentID, msgID string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if inbox, ok := tm.inboxes[agentID]; ok {
		if msg, ok := inbox[msgID]; ok {
			msg.Acked = true
			log.Printf("[Inbox] Message %s acknowledged by agent %s", msgID, agentID)
			return nil
		}
	}
	return fmt.Errorf("message not found")
}

// ReportMessage 处理任务完成报告
func (tm *TeamManager) ReportMessage(agentID, msgID string, result map[string]any) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	// 删除已处理的消息
	if inbox, ok := tm.inboxes[agentID]; ok {
		if _, ok := inbox[msgID]; ok {
			delete(inbox, msgID)
			log.Printf("[Inbox] Message %s reported complete by agent %s", msgID, agentID)
			return nil
		}
	}
	return fmt.Errorf("message not found")
}

// GetTeamInfo 获取团队信息
func (tm *TeamManager) GetTeamInfo(teamID string) (map[string]any, error) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return nil, fmt.Errorf("team not found")
	}

	team.mu.RLock()
	defer team.mu.RUnlock()

	members := make([]string, 0, len(team.Members))
	for memberID := range team.Members {
		members = append(members, memberID)
	}

	return map[string]any{
		"teamId":     team.TeamID,
		"leaderId":   team.LeaderID,
		"members":    members,
		"state":      team.State,
		"createdAt":  team.CreatedAt.UnixMilli(),
		"updatedAt":  team.UpdatedAt.UnixMilli(),
	}, nil
}

// GetVoteInfo 获取投票信息
func (tm *TeamManager) GetVoteInfo(voteID string) (map[string]any, error) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	vote, ok := tm.votes[voteID]
	if !ok {
		return nil, fmt.Errorf("vote not found")
	}

	vote.mu.RLock()
	defer vote.mu.RUnlock()

	responses := make(map[string]string)
	for k, v := range vote.Responses {
		responses[k] = v
	}

	return map[string]any{
		"voteId":     vote.VoteID,
		"teamId":     vote.TeamID,
		"proposerId": vote.ProposerID,
		"subject":    vote.Subject,
		"responses":  responses,
		"active":     vote.Active,
		"deadline":   vote.Deadline.UnixMilli(),
		"createdAt":  vote.CreatedAt.UnixMilli(),
	}, nil
}
