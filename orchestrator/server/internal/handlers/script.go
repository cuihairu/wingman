package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/agent"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// ScriptHandler 脚本处理器
type ScriptHandler struct {
	db        *gorm.DB
	pool      *agent.Pool
	agentAddr string
	store     scripts.Store
}

// NewScriptHandler 创建脚本处理器
func NewScriptHandler(db *gorm.DB, scriptsDir, agentAddr string) *ScriptHandler {
	return &ScriptHandler{
		db:        db,
		pool:      agent.NewPool(),
		agentAddr: agentAddr,
		store:     scripts.NewStore(scriptsDir),
	}
}

// HandleList 获取脚本列表
func (h *ScriptHandler) HandleList(c *gin.Context) {
	var list []models.Script
	h.db.Find(&list)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    list,
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

	name := filepath.Base(strings.TrimSpace(req.Name))
	if name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid script name"})
		return
	}
	if filepath.Ext(name) == "" {
		name += ".lua"
	}

	scriptPath, err := h.store.Resolve(name)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	content := "-- " + name + "\n-- " + req.Description + "\n\nfunction main()\n\tprint(\"Hello, Wingman!\")\nend\n\nmain()\n"
	if err := os.MkdirAll(filepath.Dir(scriptPath), 0755); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}
	if err := os.WriteFile(scriptPath, []byte(content), 0644); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	script := models.Script{
		Name:        strings.TrimSuffix(name, filepath.Ext(name)),
		Path:        scripts.DisplayPath(h.store.Root(), scriptPath),
		Description: req.Description,
	}
	h.db.Create(&script)

	c.JSON(http.StatusOK, gin.H{"success": true, "data": script})
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

	scriptPath, err := h.store.Resolve(req.Path)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if err := os.Remove(scriptPath); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	h.db.Where("path = ?", scripts.DisplayPath(h.store.Root(), scriptPath)).Delete(&models.Script{})
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

	scriptPath, err := h.store.Resolve(req.Path)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	content, err := os.ReadFile(scriptPath)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": string(content)})
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

	scriptPath, err := h.store.Resolve(req.Path)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if err := os.WriteFile(scriptPath, []byte(req.Content), 0644); err != nil {
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

	scriptPath, err := h.store.Resolve(req.Path)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	client, err := h.pool.Get(h.agentAddr)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}
	resp, err := client.RunScript(scriptPath)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}
	if success, ok := resp["success"].(bool); ok && !success {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": resp["message"]})
		return
	}

	scriptName := strings.TrimSuffix(filepath.Base(scriptPath), filepath.Ext(scriptPath))
	h.db.Model(&models.Script{}).Where("path = ?", scripts.DisplayPath(h.store.Root(), scriptPath)).Updates(map[string]interface{}{
		"is_running": true,
		"status":     "running",
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"executionId": scriptName,
			"agent":       resp,
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

	client, err := h.pool.Get(h.agentAddr)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}
	if _, err := client.StopScript(req.ExecutionID); err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}

	h.db.Model(&models.Script{}).Where("name = ?", req.ExecutionID).Updates(map[string]interface{}{
		"is_running": false,
		"status":     "stopped",
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleLogs 获取脚本日志
func (h *ScriptHandler) HandleLogs(c *gin.Context) {
	var req struct {
		ExecutionID string `json:"executionId" binding:"required"`
		Offset      int    `json:"offset"`
		Limit       int    `json:"limit"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	if req.Limit <= 0 || req.Limit > 500 {
		req.Limit = 100
	}

	var logs []models.ExecutionLog
	query := h.db.Where("script_id = ?", req.ExecutionID).Order("id ASC")
	if req.Offset > 0 {
		query = query.Offset(req.Offset)
	}
	query.Limit(req.Limit).Find(&logs)

	data := make([]gin.H, 0, len(logs))
	for _, l := range logs {
		data = append(data, gin.H{
			"timestamp": l.CreatedAt.UnixMilli(),
			"level":     l.Level,
			"message":   l.Output,
		})
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": data})
}
