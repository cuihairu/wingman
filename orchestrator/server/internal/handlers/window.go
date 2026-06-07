package handlers

import (
	"log"
	"net/http"

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
	// 从 registry 获取第一个在线的 agent
	agents := h.registry.List()
	var onlineAgent *agent.AgentInfo
	for _, a := range agents {
		if a.Status == agent.StatusOnline && a.Client != nil {
			onlineAgent = a
			break
		}
	}

	if onlineAgent == nil {
		if len(agents) > 0 {
			log.Printf("[WindowHandler] No online agents available (total: %d)", len(agents))
		}
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"success": false,
			"error":   "no available agent",
		})
		return
	}

	client := onlineAgent.Client
	resp, err := client.SendCommand("list_windows", nil)
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
