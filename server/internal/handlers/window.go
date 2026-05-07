package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// WindowHandler 窗口处理器
type WindowHandler struct{}

// NewWindowHandler 创建窗口处理器
func NewWindowHandler() *WindowHandler {
	return &WindowHandler{}
}

// HandleList 获取窗口列表
func (h *WindowHandler) HandleList(c *gin.Context) {
	// TODO: 调用 C++ Agent API
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    []interface{}{},
	})
}
