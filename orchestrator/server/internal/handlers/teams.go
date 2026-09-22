package handlers

import (
	"net/http"
	"strings"

	agentPkg "github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
)

// CreateTeamRequest 创建团队请求体
type CreateTeamRequest struct {
	// 团队名称（必填）
	Name string `json:"name" binding:"required" example:"night-farm"`
	// 团队描述
	Description string `json:"description" example:"夜间挂机协同小队"`
}

// TeamHandler 团队处理器：为多 agent 协作（runtime team.join）提供团队创建入口。
// 团队运行时状态由 FrameListener 内的 TeamManager 持有（agent 通道），本 handler
// 仅暴露 Dashboard 侧的创建能力，补齐 JoinTeam 前置的建团通路。
type TeamHandler struct {
	teamMgr *agentPkg.TeamManager
}

// NewTeamHandler 创建团队处理器
func NewTeamHandler(teamMgr *agentPkg.TeamManager) *TeamHandler {
	return &TeamHandler{teamMgr: teamMgr}
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

	// 直接格式化刚创建的团队对象（InfoOf）；按 teamID 回查在此处不可能失败，
	// 不引入不可达的错误分支。
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    h.teamMgr.InfoOf(team),
	})
}
