package handlers

import (
	"net/http"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/gin-gonic/gin"
)

// WindowHandler 窗口处理器
type WindowHandler struct {
	registry *agent.Registry
}

// NewWindowHandler 创建窗口处理器
func NewWindowHandler(registry *agent.Registry) *WindowHandler {
	return &WindowHandler{registry: registry}
}

// HandleList 获取窗口列表
func (h *WindowHandler) HandleList(c *gin.Context) {
	// Get agentId from query parameter (optional for window list)
	agentId := c.Query("agentId")

	// Select agent based on agentId (or first online if not specified)
	onlineAgent := selectAgent(h.registry, agentId)
	if onlineAgent == nil {
		if agentId != "" {
			c.JSON(http.StatusServiceUnavailable, gin.H{
				"success": false,
				"error":   "specified agent not found or not online",
			})
		} else {
			c.JSON(http.StatusServiceUnavailable, gin.H{
				"success": false,
				"error":   "no available agent",
			})
		}
		return
	}

	client := onlineAgent.Client
	resp, err := client.SendCommandWithTimeout("list_windows", nil, 10*time.Second)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{
			"success": false,
			"error":   err.Error(),
		})
		return
	}

	if success, ok := resp["success"].(bool); ok && success {
		if data, ok := resp["data"]; ok {
			c.JSON(http.StatusOK, gin.H{
				"success": true,
				"data":    data,
			})
			return
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": false,
		"error":   resp["message"],
	})
}
