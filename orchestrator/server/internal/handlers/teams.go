package handlers

import (
	"net/http"
	"sort"
	"strings"

	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// CreateTeamRequest 创建团队请求体
type CreateTeamRequest struct {
	// 团队名称（必填）
	Name string `json:"name" binding:"required" example:"night-farm"`
	// 团队描述
	Description string `json:"description" example:"夜间挂机协同小队"`
}

// TeamMessageRequest 团队范围收件箱推送请求体
// @Description 向团队全部「已绑定设备」成员各入队一条 inbox.message
// @Description 推送由 TeamManager 通知回调实时下发给在线 agent，离线成员消息留在内存缓冲。
type TeamMessageRequest struct {
	// MessageType 消息类型（自由字符串，runtime inbox 模块按此分派回调）
	MessageType string `json:"messageType" binding:"required" example:"system.notice"`
	// Payload 消息体（原样透传给 runtime inbox 队列）
	Payload map[string]any `json:"payload" example:"{\"title\":\"维护窗口\"}"`
}

// TeamMessageResult 单个成员的推送结果
type TeamMessageResult struct {
	// MemberID 团队成员 ID
	MemberID string `json:"memberId"`
	// AgentID 成员绑定的 agent ID（未绑定设备时为空）
	AgentID string `json:"agentId,omitempty"`
	// MsgID 入队后的收件箱消息 ID
	MsgID string `json:"msgId,omitempty"`
	// Skipped 跳过原因（成员未绑定设备时给出）
	Skipped string `json:"skipped,omitempty"`
}

// TeamMessageSummary 团队推送汇总
type TeamMessageSummary struct {
	// TeamID 目标团队
	TeamID string `json:"teamId"`
	// Total 成员总数（含未绑定设备的成员）
	Total int `json:"total"`
	// Delivered 已入队的消息数
	Delivered int `json:"delivered"`
	// Skipped 跳过的成员数（未绑定设备）
	Skipped int `json:"skipped"`
	// Results 逐成员结果（按 memberID 升序）
	Results []TeamMessageResult `json:"results"`
}

// TeamHandler 团队处理器：为多 agent 协作（runtime team.join）提供建团、
// 查询与团队范围收件箱推送入口。团队运行时状态由 FrameListener 内的
// TeamManager 持有（agent 通道），本 handler 只负责 HTTP 面适配。
type TeamHandler struct {
	db      *gorm.DB
	teamMgr *agentPkg.TeamManager
}

// NewTeamHandler 创建团队处理器
func NewTeamHandler(db *gorm.DB, teamMgr *agentPkg.TeamManager) *TeamHandler {
	return &TeamHandler{db: db, teamMgr: teamMgr}
}

// teamView 团队对外视图：TeamManager.InfoOf 基础字段 + 成员到设备的映射。
func (h *TeamHandler) teamView(teamID string) (map[string]any, error) {
	info, err := h.teamMgr.GetTeamInfo(teamID)
	if err != nil {
		return nil, err
	}
	agents, aerr := h.teamMgr.GetMemberAgents(teamID)
	if aerr != nil {
		return nil, aerr
	}
	info["memberAgents"] = agents
	return info, nil
}

// HandleCreate 创建团队
// @Summary      创建团队
// @Description  创建多 agent 协作团队，创建者（当前登录用户）即 leader；runtime agent 随后通过 team.join（teamId + memberId + agentId）加入该团队。需要 agents:manage 权限
// @Tags         teams
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  CreateTeamRequest  true  "团队信息"
// @Success      200  {object}  map[string]interface{} "data.teamId 为新建团队 ID"
// @Failure      400  {object}  ErrorResponse "请求体格式错误或名称为空"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /teams [post]
func (h *TeamHandler) HandleCreate(c *gin.Context) {
	var req CreateTeamRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "team name is required"})
		return
	}

	name := strings.TrimSpace(req.Name)
	if name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "team name is required"})
		return
	}

	// 创建者即 leader：leaderID 取当前登录用户名；leaderAgentID 暂为空，
	// 由 runtime agent 后续经 team.join 以 memberId + agentId 关联。
	_, actor, _ := middleware.GetCurrentUser(c)
	team := h.teamMgr.CreateTeamNamed(name, req.Description, actor, "")
	WriteAuditLog(h.db, actor, "team.create", team.TeamID, map[string]any{"name": name})

	// 直接格式化刚创建的团队对象（InfoOf）；按 teamID 回查在此处不可能失败，
	// 不引入不可达的错误分支。
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    h.teamMgr.InfoOf(team),
	})
}

// HandleList 团队列表
// @Summary      团队列表
// @Description  列出全部团队（按 teamId 升序），含成员-设备映射；团队运行时状态由 agent 通道
// @Description  的 TeamManager 持有，服务重启后丢失，这里只反映当前在线视图。需要 agents:manage 权限
// @Tags         teams
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{} "data 为团队视图数组"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /teams [get]
func (h *TeamHandler) HandleList(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    h.teamMgr.ListTeams(),
	})
}

// HandleGet 团队详情
// @Summary      团队详情
// @Description  单个团队的视图（基础字段 + 成员到设备的映射 memberAgents）。需要 agents:manage 权限
// @Tags         teams
// @Produce      json
// @Security     BearerAuth
// @Param        teamId  path  string  true  "团队 ID"
// @Success      200  {object}  map[string]interface{} "data 为团队视图"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse "团队不存在（或已被解散）"
// @Router       /teams/{teamId} [get]
func (h *TeamHandler) HandleGet(c *gin.Context) {
	teamID := c.Param("teamId")
	view, err := h.teamView(teamID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "team not found"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "data": view})
}

// HandleSendMessage 团队范围收件箱推送
// @Summary      向团队全部成员下发收件箱消息
// @Description  解析团队成员到设备的映射，对每个已绑定设备的成员入队一条 inbox.message
// @Description  （TeamManager 通知回调实时推送给在线 agent，离线成员保留在内存缓冲）；
// @Description  未绑定设备的成员记入 skipped。需要 agents:manage 权限
// @Tags         teams
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        teamId  path  string  true  "团队 ID"
// @Param        request body  TeamMessageRequest true "消息类型与消息体"
// @Success      200  {object}  map[string]interface{} "data 为逐成员推送汇总"
// @Failure      400  {object}  ErrorResponse "请求体格式错误或 messageType 为空"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse "团队不存在（或已被解散）"
// @Router       /teams/{teamId}/messages [post]
func (h *TeamHandler) HandleSendMessage(c *gin.Context) {
	teamID := c.Param("teamId")
	var req TeamMessageRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "messageType is required"})
		return
	}
	if strings.TrimSpace(req.MessageType) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "messageType is required"})
		return
	}

	agents, err := h.teamMgr.GetMemberAgents(teamID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "team not found"})
		return
	}

	// 成员结果按 memberID 升序输出（map 遍历无序）
	members := make([]string, 0, len(agents))
	for memberID := range agents {
		members = append(members, memberID)
	}
	sort.Strings(members)

	payload := req.Payload
	if payload == nil {
		payload = map[string]any{}
	}
	summary := TeamMessageSummary{
		TeamID:  teamID,
		Total:   len(members),
		Results: make([]TeamMessageResult, 0, len(members)),
	}
	for _, memberID := range members {
		agentID := agents[memberID]
		if strings.TrimSpace(agentID) == "" {
			// 未绑定设备的成员（人肉成员）不向空 agentID 入队，避免污染收件箱
			summary.Results = append(summary.Results, TeamMessageResult{
				MemberID: memberID,
				Skipped:  "no device bound",
			})
			summary.Skipped++
			continue
		}
		msgID := h.teamMgr.SendMessageToAgent(agentID, req.MessageType, payload)
		summary.Results = append(summary.Results, TeamMessageResult{
			MemberID: memberID,
			AgentID:  agentID,
			MsgID:    msgID,
		})
		summary.Delivered++
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "team.message_push", teamID, map[string]any{
		"messageType": req.MessageType,
		"total":       summary.Total,
		"delivered":   summary.Delivered,
		"skipped":     summary.Skipped,
	})

	c.JSON(http.StatusOK, gin.H{"success": true, "data": summary})
}
