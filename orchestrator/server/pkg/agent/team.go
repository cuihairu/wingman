package agent

import (
	"fmt"
	"log"
	"sort"
	"sync"
	"time"
)

// ========== Team Management ==========

// TeamInfo 团队信息
type TeamInfo struct {
	TeamID      string
	Name        string
	Description string
	LeaderID    string
	Members     map[string]string // memberID -> agentID
	State       string           // "idle", "voting", "working"
	CreatedAt   time.Time
	UpdatedAt   time.Time
	mu          sync.RWMutex
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
	MsgID string
	// Seq 全局单调递增序号，GetMessages 按它保序输出（收件箱 FIFO 语义；
	// 底层 map 遍历顺序随机，必须显式排序）
	Seq       int64
	AgentID   string
	Type      string
	Payload   map[string]any
	Timestamp time.Time
	Acked     bool
}

// MessageNotifier 收件箱消息通知回调：消息入队后同步调用，用于把消息实时下发给在线 agent。
// 回调在 TeamManager 内部锁（tm.mu）持有期间被调用，实现不得重入任何会加 tm.mu 的
// TeamManager 方法（FrameListener 的实现锁序为 tm.mu → FrameListener.mu → agentConn.mu）。
type MessageNotifier func(agentID string, msg *InboxMessage)

// TeamManager 团队管理器
type TeamManager struct {
	teams      map[string]*TeamInfo
	votes      map[string]*VoteInfo
	inboxes    map[string]map[string]*InboxMessage // agentID -> msgID -> message
	notifier   MessageNotifier
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

// CreateTeamNamed 创建带名称/描述的团队（Dashboard HTTP 端点使用），创建者即 leader。
// runtime agent 随后经 team.join（teamId + memberId + agentId）加入该团队。
func (tm *TeamManager) CreateTeamNamed(name, description, leaderID, leaderAgentID string) *TeamInfo {
	team := tm.CreateTeam(leaderID, leaderAgentID)

	tm.mu.Lock()
	team.mu.Lock()
	team.Name = name
	team.Description = description
	team.UpdatedAt = time.Now()
	team.mu.Unlock()
	tm.mu.Unlock()

	return team
}

// SetMessageNotifier 注册收件箱消息通知回调（FrameListener 装配时调用，
// 用于把新入队的消息实时下发给在线 agent；传 nil 可取消）。
func (tm *TeamManager) SetMessageNotifier(n MessageNotifier) {
	tm.mu.Lock()
	defer tm.mu.Unlock()
	tm.notifier = n
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
				"members":   tm.getMemberListLocked(team),
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

	if !vote.Active {
		vote.mu.Unlock()
		return fmt.Errorf("vote is not active")
	}

	if time.Now().After(vote.Deadline) {
		vote.Active = false
		vote.mu.Unlock()
		return fmt.Errorf("vote has expired")
	}

	vote.Responses[memberID] = response
	responseCount := len(vote.Responses)
	vote.mu.Unlock()

	log.Printf("[Team] Member %s cast vote for %s: %s", memberID, voteID, response)

	// 检查是否所有人都已投票
	team, ok := tm.teams[vote.TeamID]
	if !ok {
		return nil
	}

	team.mu.RLock()
	memberCount := len(team.Members)
	team.mu.RUnlock()

	if responseCount >= memberCount {
		return tm.endVoteLocked(voteID)
	}

	return nil
}

// endVote 结束投票
func (tm *TeamManager) endVote(voteID string) error {
	tm.mu.Lock()
	defer tm.mu.Unlock()
	return tm.endVoteLocked(voteID)
}

// endVoteLocked 结束投票（调用方必须已持有 tm.mu）。
// CastVote 等持锁路径必须使用此变体，避免重复加锁导致死锁。
func (tm *TeamManager) endVoteLocked(voteID string) error {
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
	var members map[string]string
	if team, ok := tm.teams[vote.TeamID]; ok {
		team.mu.Lock()
		team.State = "idle"
		team.UpdatedAt = time.Now()
		team.mu.Unlock()

		// 通知所有成员投票结果
		team.mu.RLock()
		members = make(map[string]string)
		for k, v := range team.Members {
			members[k] = v
		}
		team.mu.RUnlock()
	}

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

	return tm.getMemberListLocked(team)
}

// getMemberListLocked 获取成员列表（调用方必须已持有 team.mu）。
// JoinTeam 等持锁路径必须使用此变体，避免重复加锁导致死锁。
func (tm *TeamManager) getMemberListLocked(team *TeamInfo) []string {
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
	return tm.sendMessageToAgentLocked(agentID, msgType, payload)
}

// sendMessageToAgentLocked 在调用方已持有 tm.mu 时写入收件箱。
// JoinTeam/LeaveTeam/CreateVote/endVote 等持锁路径必须使用此变体，
// 避免经 sendToInbox → SendMessageToAgent 重复加锁导致死锁。
func (tm *TeamManager) sendMessageToAgentLocked(agentID, msgType string, payload map[string]any) string {
	seq := tm.nextMsgID
	msgID := fmt.Sprintf("msg_%d", seq)
	tm.nextMsgID++

	msg := &InboxMessage{
		MsgID:     msgID,
		Seq:       seq,
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

	// 通知监听者（FrameListener）实时下发 inbox.message；agent 离线时由其保持内存缓冲。
	// 回调在 tm.mu 持有期间同步执行，实现不得重入 TeamManager 的加锁方法。
	if tm.notifier != nil {
		tm.notifier(agentID, msg)
	}

	return msgID
}

// sendToInbox 内部辅助函数（调用方必须已持有 tm.mu）
func (tm *TeamManager) sendToInbox(agentID string, data map[string]any) {
	msgID := tm.sendMessageToAgentLocked(agentID, data["type"].(string), data)

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

	// 按 Seq（入队序）升序收集未确认消息：底层 map 遍历无序，
	// 收件箱必须保持 FIFO 语义供 agent 按序消费。
	pending := make([]*InboxMessage, 0, len(tm.inboxes[agentID]))
	for _, msg := range tm.inboxes[agentID] {
		if !msg.Acked {
			pending = append(pending, msg)
		}
	}
	sort.Slice(pending, func(i, j int) bool { return pending[i].Seq < pending[j].Seq })

	messages := make([]map[string]any, 0, len(pending))
	for _, msg := range pending {
		if len(messages) >= limit {
			break
		}
		messages = append(messages, map[string]any{
			"msgId":     msg.MsgID,
			"type":      msg.Type,
			"payload":   msg.Payload,
			"timestamp": msg.Timestamp.UnixMilli(),
		})
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
	team, ok := tm.teams[teamID]
	tm.mu.RUnlock()

	if !ok {
		return nil, fmt.Errorf("team not found")
	}

	return tm.InfoOf(team), nil
}

// InfoOf 将团队对象格式化为对外响应视图（字段结构与 GetTeamInfo 一致）。
// 独立成方法供 HTTP handler 直接格式化刚创建的团队对象——此时按 teamID
// 回查只会徒增一个不可达的错误分支。
func (tm *TeamManager) InfoOf(team *TeamInfo) map[string]any {
	team.mu.RLock()
	defer team.mu.RUnlock()

	members := make([]string, 0, len(team.Members))
	for memberID := range team.Members {
		members = append(members, memberID)
	}

	return map[string]any{
		"teamId":      team.TeamID,
		"name":        team.Name,
		"description": team.Description,
		"leaderId":    team.LeaderID,
		"members":     members,
		"state":       team.State,
		"createdAt":   team.CreatedAt.UnixMilli(),
		"updatedAt":   team.UpdatedAt.UnixMilli(),
	}
}

// GetMemberAgents 获取团队成员的 memberID -> agentID 映射
func (tm *TeamManager) GetMemberAgents(teamID string) (map[string]string, error) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return nil, fmt.Errorf("team not found")
	}

	team.mu.RLock()
	defer team.mu.RUnlock()

	agents := make(map[string]string, len(team.Members))
	for memberID, agentID := range team.Members {
		agents[memberID] = agentID
	}
	return agents, nil
}

// RemoveAgent 清理指定 agent 的全部团队状态：清空其收件箱缓冲消息，并从所有团队
// 移除其成员关系；清空后无成员的团队随之解散。其余剩余成员会收到 team.member_left
// 收件箱通知（锁外发送，避免与 tm.mu 死锁）。agent 断连时由 FrameListener 调用。
func (tm *TeamManager) RemoveAgent(agentID string) {
	type leaveNotice struct {
		teamID   string
		memberID string
		remains  []string // 剩余成员的 agentID
	}
	var notices []leaveNotice

	tm.mu.Lock()

	delete(tm.inboxes, agentID)

	for teamID, team := range tm.teams {
		team.mu.Lock()
		leavingMember := ""
		for memberID, aid := range team.Members {
			if aid == agentID {
				delete(team.Members, memberID)
				leavingMember = memberID
			}
		}
		if leavingMember != "" {
			team.UpdatedAt = time.Now()
			notice := leaveNotice{teamID: teamID, memberID: leavingMember}
			for _, aid := range team.Members {
				notice.remains = append(notice.remains, aid)
			}
			notices = append(notices, notice)
		}
		team.mu.Unlock()

		if leavingMember != "" && len(team.Members) == 0 {
			delete(tm.teams, teamID)
			log.Printf("[Team] Disbanded empty team %s after agent %s disconnected", teamID, agentID)
		}
	}

	tm.mu.Unlock()

	// 锁外发送成员离开通知（SendMessageToAgent 会重新加锁）
	for _, n := range notices {
		for _, aid := range n.remains {
			tm.SendMessageToAgent(aid, "team.member_left", map[string]any{
				"type":     "team.member_left",
				"teamId":   n.teamID,
				"memberId": n.memberID,
			})
		}
	}
}

// FindMemberByAgent 通过 agentID 反查团队中的 memberID
func (tm *TeamManager) FindMemberByAgent(teamID, agentID string) (string, bool) {
	tm.mu.RLock()
	defer tm.mu.RUnlock()

	team, ok := tm.teams[teamID]
	if !ok {
		return "", false
	}

	team.mu.RLock()
	defer team.mu.RUnlock()

	for memberID, aid := range team.Members {
		if aid == agentID {
			return memberID, true
		}
	}
	return "", false
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
