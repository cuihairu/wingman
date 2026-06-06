package handlers

import (
	"net/http"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// AgentHandler Agent 管理 handler
type AgentHandler struct {
	registry *agent.Registry
	db       *gorm.DB
}

// NewAgentHandler 创建 Agent handler
func NewAgentHandler(registry *agent.Registry, db *gorm.DB) *AgentHandler {
	return &AgentHandler{
		registry: registry,
		db:       db,
	}
}

// HandleList 获取 Agent 列表
// GET /api/agents
func (h *AgentHandler) HandleList(c *gin.Context) {
	agents := h.registry.List()

	data := make([]map[string]interface{}, 0, len(agents))
	for _, a := range agents {
		data = append(data, a.ToJSON())
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    data,
	})
}

// HandleGet 获取指定 Agent 详情
// GET /api/agents/:agentId
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

// HandleShutdown 关闭 Agent
// POST /api/agents/:agentId/shutdown
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

	// 向 agent 发送关闭命令
	_, err := conn.SendCommand("system.shutdown", nil)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{
			"success": false,
			"error":   "failed to send shutdown command: " + err.Error(),
		})
		return
	}

	// 标记为 offline
	h.registry.UpdateStatus(agentID, string(agent.StatusOffline), nil)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
	})
}
