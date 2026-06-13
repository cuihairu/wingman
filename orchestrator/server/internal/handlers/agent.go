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
