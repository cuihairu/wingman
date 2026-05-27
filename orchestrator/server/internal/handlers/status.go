package handlers

import (
	"net/http"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// StatusHandler 状态处理器
type StatusHandler struct {
	db        *gorm.DB
	pool      *agent.Pool
	agentAddr string
}

// NewStatusHandler 创建状态处理器
func NewStatusHandler(db *gorm.DB, agentAddr string) *StatusHandler {
	return &StatusHandler{db: db, pool: agent.NewPool(), agentAddr: agentAddr}
}

// HandleStatus 获取系统状态
func (h *StatusHandler) HandleStatus(c *gin.Context) {
	var totalScripts int64
	var runningScripts int64

	h.db.Model(&models.Script{}).Count(&totalScripts)
	h.db.Model(&models.Script{}).Where("is_running = ?", true).Count(&runningScripts)

	totalWindows := int64(0)
	cpuUsage := 0.0
	memoryUsage := 0.0
	if client, err := h.pool.Get(h.agentAddr); err == nil {
		if resp, err := client.GetStatus(); err == nil {
			if data, ok := resp["data"].(map[string]interface{}); ok {
				if v, ok := data["totalScripts"].(float64); ok {
					totalScripts = int64(v)
				}
				if v, ok := data["runningScripts"].(float64); ok {
					runningScripts = int64(v)
				}
				if v, ok := data["totalWindows"].(float64); ok {
					totalWindows = int64(v)
				}
				if v, ok := data["cpuUsage"].(float64); ok {
					cpuUsage = v
				}
				if v, ok := data["memoryUsage"].(float64); ok {
					memoryUsage = v
				}
			}
		} else if resp, err := client.ListWindows(); err == nil {
			if data, ok := resp["data"].(map[string]interface{}); ok {
				if windows, ok := data["windows"].([]interface{}); ok {
					totalWindows = int64(len(windows))
				}
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"totalScripts":   totalScripts,
			"runningScripts": runningScripts,
			"totalWindows":   totalWindows,
			"cpuUsage":       cpuUsage,
			"memoryUsage":    memoryUsage,
		},
	})
}

// HandleHealth 健康检查
func (h *StatusHandler) HandleHealth(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status": "ok",
	})
}
