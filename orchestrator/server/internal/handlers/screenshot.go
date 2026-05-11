package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// ScreenshotHandler 截图处理器
type ScreenshotHandler struct {
	hub interface {
		BroadcastEvent(eventType string, data interface{})
	}
}

// NewScreenshotHandler 创建截图处理器
func NewScreenshotHandler(hub interface {
	BroadcastEvent(eventType string, data interface{})
}) *ScreenshotHandler {
	return &ScreenshotHandler{hub: hub}
}

// ScreenshotRequest 截图请求
type ScreenshotRequest struct {
	Image     string `json:"image"`
	Width     int    `json:"width"`
	Height    int    `json:"height"`
	Timestamp int64  `json:"timestamp"`
}

// HandleScreenshot 处理截图上报
func (h *ScreenshotHandler) HandleScreenshot(c *gin.Context) {
	var req ScreenshotRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"error":   "Invalid request: " + err.Error(),
		})
		return
	}

	// 验证必填字段
	if req.Image == "" {
		c.JSON(http.StatusBadRequest, gin.H{
			"success": false,
			"error":   "Missing image data",
		})
		return
	}

	// 通过 WebSocket 广播截图到所有连接的客户端
	h.hub.BroadcastEvent("screenshot", gin.H{
		"image":     req.Image,
		"width":     req.Width,
		"height":    req.Height,
		"timestamp": req.Timestamp,
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"message": "Screenshot received and broadcasted",
		},
	})
}
