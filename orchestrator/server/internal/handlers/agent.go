package handlers

import (
	"net/http"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type AgentHandler struct {
	registry *agent.Registry
	db       *gorm.DB
}

func NewAgentHandler(registry *agent.Registry, db *gorm.DB) *AgentHandler {
	return &AgentHandler{
		registry: registry,
		db:       db,
	}
}

// HandleList Agent 列表
// @Summary      Agent 列表（状态/标签/负载）
// @Description  返回所有已注册 agent 的状态、标签与负载数据（不含已断开条目按 registry 当前视图）
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Router       /agents [get]
func (h *AgentHandler) HandleList(c *gin.Context) {
	agents := h.registry.List()

	data := make([]map[string]any, 0, len(agents))
	for _, current := range agents {
		data = append(data, current.ToJSON())
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    data,
	})
}

// HandleGet Agent 详情
// @Summary      Agent 详情
// @Description  返回单个 agent 的状态/标签/负载信息
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Router       /agents/{agentId} [get]
func (h *AgentHandler) HandleGet(c *gin.Context) {
	agentID := c.Param("agentId")

	info, ok := h.registry.Get(agentID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{
			"success": false,
			"error":   "agent not found",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    info.ToJSON(),
	})
}

// HandleShutdown 远程关闭 Agent
// @Summary      关闭 Agent
// @Description  下发 system.shutdown 并将 agent 标记为 offline；需要 agents:manage 权限
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
// @Router       /agents/{agentId}/shutdown [post]
func (h *AgentHandler) HandleShutdown(c *gin.Context) {
	agentID := c.Param("agentId")

	conn, ok := h.registry.GetClient(agentID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{
			"success": false,
			"error":   "agent not connected",
		})
		return
	}

	if _, err := conn.SendCommandWithTimeout("system.shutdown", nil, 10*time.Second); err != nil {
		c.JSON(http.StatusBadGateway, gin.H{
			"success": false,
			"error":   "failed to send shutdown command: " + err.Error(),
		})
		return
	}

	h.registry.UpdateStatus(agentID, string(agent.StatusOffline), nil)

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.shutdown", agentID, map[string]any{
		"agent_id": agentID,
		"ip":       c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
	})
}

// HandleSetTags 设置 Agent 标签（分组）
// @Summary      设置 Agent 标签
// @Description  覆盖式更新 agent 标签；需要 agents:manage 权限
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Param        request  body  object  true  "标签列表"  example({"tags":["prod"]})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Router       /agents/{agentId}/tags [put]
func (h *AgentHandler) HandleSetTags(c *gin.Context) {
	agentID := c.Param("agentId")

	var req struct {
		Tags []string `json:"tags"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request"})
		return
	}

	if !h.registry.SetTags(agentID, req.Tags) {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "agent not found"})
		return
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.set_tags", agentID, map[string]any{
		"agent_id": agentID,
		"tags":     req.Tags,
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}
