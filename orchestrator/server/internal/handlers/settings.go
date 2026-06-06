package handlers

import (
	"net/http"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// SettingsHandler 配置 handler
type SettingsHandler struct {
	db *gorm.DB
}

// NewSettingsHandler 创建配置 handler
func NewSettingsHandler(db *gorm.DB) *SettingsHandler {
	return &SettingsHandler{db: db}
}

// HandleGetSettings 获取配置
func (h *SettingsHandler) HandleGetSettings(c *gin.Context) {
	var settings []models.Settings
	h.db.Find(&settings)

	data := gin.H{}
	for _, s := range settings {
		data[s.Key] = s.Value
	}

	// 填充默认值
	defaults := gin.H{
		"serverPort": "9527",
		"logLevel":   "info",
		"maxScripts": "10",
	}
	for k, v := range defaults {
		if _, exists := data[k]; !exists {
			data[k] = v
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    data,
	})
}

// HandleUpdateSettings 更新配置
func (h *SettingsHandler) HandleUpdateSettings(c *gin.Context) {
	var req map[string]string
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	for key, value := range req {
		var setting models.Settings
		result := h.db.Where("key = ?", key).First(&setting)
		if result.Error == gorm.ErrRecordNotFound {
			h.db.Create(&models.Settings{Key: key, Value: value})
		} else {
			h.db.Model(&setting).Update("value", value)
		}
	}

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// Keep legacy functions for backward compatibility
// HandleGetSettingsLegacy 兼容 v1 路由的静态配置（无 DB）
func HandleGetSettings(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"serverPort": 8888,
			"httpPort":   9527,
			"logLevel":   "info",
			"maxScripts": 10,
		},
	})
}

// HandleUpdateSettings 兼容 v1 路由的静态更新
func HandleUpdateSettings(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
	})
}
