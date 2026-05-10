package handlers

import (
	"net/http"
	"os"
	"path/filepath"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
	"github.com/cuihaitao/wingman/server/internal/models"
)

// ScriptHandler 脚本处理器
type ScriptHandler struct {
	db *gorm.DB
}

// NewScriptHandler 创建脚本处理器
func NewScriptHandler(db *gorm.DB) *ScriptHandler {
	return &ScriptHandler{db: db}
}

// HandleList 获取脚本列表
func (h *ScriptHandler) HandleList(c *gin.Context) {
	var scripts []models.Script
	h.db.Find(&scripts)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    scripts,
	})
}

// HandleCreate 创建脚本
func (h *ScriptHandler) HandleCreate(c *gin.Context) {
	var req struct {
		Name        string `json:"name" binding:"required"`
		Description string `json:"description"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	scriptPath := "scripts/" + req.Name
	content := "-- " + req.Name + "\n-- " + req.Description + "\n\nfunction main()\n\tprint(\"Hello, Wingman!\")\nend\n\nmain()\n"

	if err := os.MkdirAll(filepath.Dir(scriptPath), 0755); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	if err := os.WriteFile(scriptPath, []byte(content), 0644); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	script := models.Script{
		Name: req.Name,
		Path: scriptPath,
		Description: req.Description,
	}
	h.db.Create(&script)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    script,
	})
}

// HandleDelete 删除脚本
func (h *ScriptHandler) HandleDelete(c *gin.Context) {
	var req struct {
		Path string `json:"path" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	if err := os.Remove(req.Path); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	h.db.Where("path = ?", req.Path).Delete(&models.Script{})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleGetContent 获取脚本内容
func (h *ScriptHandler) HandleGetContent(c *gin.Context) {
	var req struct {
		Path string `json:"path" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	content, err := os.ReadFile(req.Path)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    string(content),
	})
}

// HandleSave 保存脚本
func (h *ScriptHandler) HandleSave(c *gin.Context) {
	var req struct {
		Path    string `json:"path" binding:"required"`
		Content string `json:"content" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	if err := os.WriteFile(req.Path, []byte(req.Content), 0644); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleRun 运行脚本
func (h *ScriptHandler) HandleRun(c *gin.Context) {
	var req struct {
		Path string `json:"path" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	// TODO: 调用 C++ Client API
	scriptName := filepath.Base(req.Path)
	scriptName = scriptName[:len(scriptName)-4] // 去掉 .lua

	h.db.Model(&models.Script{}).Where("path = ?", req.Path).Updates(map[string]interface{}{
		"is_running": true,
		"status":     "running",
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"executionId": scriptName,
		},
	})
}

// HandleStop 停止脚本
func (h *ScriptHandler) HandleStop(c *gin.Context) {
	var req struct {
		ExecutionID string `json:"executionId" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	// TODO: 调用 C++ Client API

	h.db.Model(&models.Script{}).Where("path LIKE ?", "%"+req.ExecutionID+"%").Updates(map[string]interface{}{
		"is_running": false,
		"status":     "stopped",
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleLogs 获取脚本日志
func (h *ScriptHandler) HandleLogs(c *gin.Context) {
	var req struct {
		ExecutionID string `json:"executionId" binding:"required"`
	}

	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    []interface{}{},
	})
}
