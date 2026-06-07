package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// ScriptHandler 脚本处理器
type ScriptHandler struct {
	db       *gorm.DB
	registry *agent.Registry
	store    scripts.Store
}

// NewScriptHandler 创建脚本处理器
func NewScriptHandler(db *gorm.DB, scriptsDir string, registry *agent.Registry) *ScriptHandler {
	return &ScriptHandler{
		db:       db,
		registry: registry,
		store:    scripts.NewStore(scriptsDir),
	}
}

// selectAgent selects an agent based on the provided agentId.
// If agentId is empty, returns the first online agent (for backward compatibility).
// If agentId is provided, returns that specific agent if online.
// Returns nil if no suitable agent is found.
func selectAgent(registry *agent.Registry, agentId string) *agent.AgentInfo {
	agents := registry.List()

	// If agentId is specified, find that specific agent
	if agentId != "" {
		for _, a := range agents {
			if a.AgentID == agentId && a.Status == agent.StatusOnline && a.Client != nil {
				return a
			}
		}
		// Specific agent requested but not found or not online
		return nil
	}

	// No agentId specified: return first online agent
	// TODO: Implement smarter selection (round-robin, least loaded, affinity)
	for _, a := range agents {
		if a.Status == agent.StatusOnline && a.Client != nil {
			return a
		}
	}

	return nil
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
		Path    string `json:"path" binding:"required"`
		AgentID string `json:"agentId"` // Optional: specific agent to run script on
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

	// Select agent based on agentId (or first online if not specified)
	onlineAgent := selectAgent(h.registry, req.AgentID)
	if onlineAgent == nil {
		if req.AgentID != "" {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "specified agent not found or not online"})
		} else {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "no available agent"})
		}
		return
	}

	client := onlineAgent.Client
	resp, err := client.SendCommand("run_script", map[string]any{
		"path": scriptPath,
	})
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}
	if success, ok := resp["success"].(bool); ok && !success {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": resp["message"]})
		return
	}

	scriptName := strings.TrimSuffix(filepath.Base(scriptPath), filepath.Ext(scriptPath))
	h.db.Model(&models.Script{}).Where("path = ?", scripts.DisplayPath(h.store.Root(), scriptPath)).Updates(map[string]any{
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
		AgentID     string `json:"agentId"` // Optional: specific agent to stop script on
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	// Select agent based on agentId (or first online if not specified)
	onlineAgent := selectAgent(h.registry, req.AgentID)
	if onlineAgent == nil {
		if req.AgentID != "" {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "specified agent not found or not online"})
		} else {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "no available agent"})
		}
		return
	}

	client := onlineAgent.Client
	if _, err := client.SendCommand("stop_script", map[string]any{
		"script_id": req.ExecutionID,
	}); err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}

	h.db.Model(&models.Script{}).Where("name = ?", req.ExecutionID).Updates(map[string]any{
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
