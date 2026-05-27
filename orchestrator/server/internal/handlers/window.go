package handlers

import (
	"net/http"

	"github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	"github.com/gin-gonic/gin"
)

// WindowHandler 窗口处理器
type WindowHandler struct {
	pool      *agent.Pool
	agentAddr string
}

// NewWindowHandler 创建窗口处理器
func NewWindowHandler(agentAddr string) *WindowHandler {
	return &WindowHandler{pool: agent.NewPool(), agentAddr: agentAddr}
}

// HandleList 获取窗口列表
func (h *WindowHandler) HandleList(c *gin.Context) {
	client, err := h.pool.Get(h.agentAddr)
	if err != nil {
		c.JSON(http.StatusServiceUnavailable, gin.H{
			"success": false,
			"error":   err.Error(),
		})
		return
	}

	resp, err := client.ListWindows()
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
