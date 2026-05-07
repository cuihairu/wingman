package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// HandleGetSettings 获取配置
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

// HandleUpdateSettings 更新配置
func HandleUpdateSettings(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
	})
}
