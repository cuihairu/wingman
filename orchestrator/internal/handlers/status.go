package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
	"github.com/cuihaitao/wingman/server/internal/models"
)

// StatusHandler 状态处理器
type StatusHandler struct {
	db *gorm.DB
}

// NewStatusHandler 创建状态处理器
func NewStatusHandler(db *gorm.DB) *StatusHandler {
	return &StatusHandler{db: db}
}

// HandleStatus 获取系统状态
func (h *StatusHandler) HandleStatus(c *gin.Context) {
	var totalScripts int64
	var runningScripts int64

	h.db.Model(&models.Script{}).Count(&totalScripts)
	h.db.Model(&models.Script{}).Where("is_running = ?", true).Count(&runningScripts)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"totalScripts":   totalScripts,
			"runningScripts": runningScripts,
			"totalWindows":   0,
			"cpuUsage":       0,
			"memoryUsage":    0,
		},
	})
}

// HandleHealth 健康检查
func (h *StatusHandler) HandleHealth(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status": "ok",
	})
}
