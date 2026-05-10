package handlers

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// HandleDebuggerConnect 调试器连接
func HandleDebuggerConnect(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"message": "Debugger connected",
		"version": "0.1.0",
	})
}

// HandleDebuggerCommand 调试命令
func HandleDebuggerCommand(c *gin.Context) {
	var req struct {
		Command string `json:"command"`
	}

	c.ShouldBindJSON(&req)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"state":   "stopped",
	})
}

// HandleDebuggerGetBreakpoints 获取断点
func HandleDebuggerGetBreakpoints(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"success":     true,
		"breakpoints": []interface{}{},
	})
}

// HandleDebuggerSetBreakpoints 设置断点
func HandleDebuggerSetBreakpoints(c *gin.Context) {
	var req struct {
		File        string `json:"file"`
		Breakpoints []struct {
			Line int `json:"line"`
		} `json:"breakpoints"`
	}

	c.ShouldBindJSON(&req)

	c.JSON(http.StatusOK, gin.H{
		"success":     true,
		"breakpoints": []interface{}{},
	})
}
