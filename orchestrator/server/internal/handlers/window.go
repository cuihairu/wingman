package handlers

import (
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
)

// WindowHandler 窗口处理器
type WindowHandler struct {
	pool *agent.Pool
}

// NewWindowHandler 创建窗口处理器
func NewWindowHandler() *WindowHandler {
	return &WindowHandler{pool: agent.NewPool()}
}

// HandleList 获取窗口列表
func (h *WindowHandler) HandleList(c *gin.Context) {
	address := os.Getenv("WINGMAN_AGENT_ADDR")
	if address == "" {
		address = "localhost:8888"
	}

	client, err := h.pool.Get(address)
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
