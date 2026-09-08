package handlers

import (
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type ScriptHandler struct {
	db       *gorm.DB
	registry *agent.Registry
	store    scripts.Store
}

func NewScriptHandler(db *gorm.DB, scriptsDir string, registry *agent.Registry) *ScriptHandler {
	return &ScriptHandler{
		db:       db,
		registry: registry,
		store:    scripts.NewStore(scriptsDir),
	}
}

func selectAgent(registry *agent.Registry, agentID string) *agent.AgentInfo {
	agents := registry.List()

	if agentID != "" {
		for _, current := range agents {
			if current.AgentID == agentID && current.Status == agent.StatusOnline && current.Client != nil {
				return current
			}
		}
		return nil
	}

	for _, current := range agents {
		if current.Status == agent.StatusOnline && current.Client != nil {
			return current
		}
	}

	return nil
}

// HandleList 脚本列表
// @Summary      脚本列表
// @Description  返回已登记的脚本（同时在 /api/v1/scripts 注册）
// @Tags         scripts
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Router       /scripts [get]
func (h *ScriptHandler) HandleList(c *gin.Context) {
	var list []models.Script
	h.db.Find(&list)

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    list,
	})
}

// HandleCreate 创建脚本
// @Summary      创建脚本
// @Description  生成 Lua 模板文件并写入 DB 记录（路径穿越防护）；需要 scripts:edit 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "脚本元信息"  example({"name":"demo","description":"示例脚本"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      500  {object}  map[string]interface{}
// @Router       /scripts [post]
func (h *ScriptHandler) HandleCreate(c *gin.Context) {
	var req struct {
		Name        string `json:"name" binding:"required"`
		Description string `json:"description"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	trimmedName := strings.TrimSpace(req.Name)
	if trimmedName == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid script name"})
		return
	}
	if strings.Contains(trimmedName, "..") || strings.Contains(trimmedName, "/") || strings.Contains(trimmedName, "\\") {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid script name: path traversal characters not allowed"})
		return
	}
	if strings.Contains(trimmedName, "\x00") {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid script name: null bytes not allowed"})
		return
	}
	if len(trimmedName) > 255 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid script name: name too long (max 255 characters)"})
		return
	}

	name := filepath.Base(trimmedName)
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

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.create", script.Path, map[string]any{
		"script_name": script.Name,
		"script_path": script.Path,
		"ip":          c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{"success": true, "data": script})
}

// HandleDelete 删除脚本
// @Summary      删除脚本
// @Description  删除脚本文件与 DB 记录（DELETE /scripts 与 POST /scripts/delete 两个入口）；需要 scripts:edit 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "脚本路径"  example({"path":"demo.lua"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      500  {object}  map[string]interface{}
// @Router       /scripts [delete]
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

	targetPath := scripts.DisplayPath(h.store.Root(), scriptPath)
	h.db.Where("path = ?", targetPath).Delete(&models.Script{})

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.delete", targetPath, map[string]any{
		"script_path": targetPath,
		"ip":          c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleGetContent 读取脚本内容
// @Summary      读取脚本内容
// @Description  按路径返回脚本全文（路径经 store 安全校验）
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "脚本路径"  example({"path":"demo.lua"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      500  {object}  map[string]interface{}
// @Router       /scripts/content [post]
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

// HandleSave 保存脚本内容
// @Summary      保存脚本内容
// @Description  覆盖写入脚本文件；需要 scripts:edit 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "脚本路径与内容"  example({"path":"demo.lua","content":"print(1)"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      500  {object}  map[string]interface{}
// @Router       /scripts/save [post]
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

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.save", req.Path, map[string]any{
		"script_path": req.Path,
		"ip":          c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleRun 运行脚本
// @Summary      运行脚本
// @Description  经 agent 通道下发 run_script，并更新脚本运行状态；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "脚本与目标 agent"  example({"path":"demo.lua","agentId":"agent-1"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /scripts/run [post]
func (h *ScriptHandler) HandleRun(c *gin.Context) {
	var req struct {
		Path    string `json:"path" binding:"required"`
		AgentID string `json:"agentId"`
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

	onlineAgent := selectAgent(h.registry, req.AgentID)
	if onlineAgent == nil {
		if req.AgentID != "" {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "specified agent not found or not online"})
		} else {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "no available agent"})
		}
		return
	}

	resp, err := onlineAgent.Client.SendCommandWithTimeout("run_script", map[string]any{
		"path": scriptPath,
	}, 30*time.Second)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}
	if success, ok := resp["success"].(bool); ok && !success {
		errText, _ := resp["error"].(string)
		if errText == "" {
			errText, _ = resp["message"].(string)
		}
		if errText == "" {
			errText = "agent command failed"
		}
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": errText})
		return
	}

	scriptName := strings.TrimSuffix(filepath.Base(scriptPath), filepath.Ext(scriptPath))
	h.db.Model(&models.Script{}).Where("path = ?", scripts.DisplayPath(h.store.Root(), scriptPath)).Updates(map[string]any{
		"is_running": true,
		"status":     "running",
	})

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.run", req.Path, map[string]any{
		"script_path":  req.Path,
		"agent_id":     onlineAgent.AgentID,
		"execution_id": scriptName,
		"ip":           c.ClientIP(),
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
// @Summary      停止脚本
// @Description  经 agent 通道下发 stop_script，并更新脚本状态；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "执行 ID 与目标 agent"  example({"executionId":"demo","agentId":"agent-1"})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /scripts/stop [post]
func (h *ScriptHandler) HandleStop(c *gin.Context) {
	var req struct {
		ExecutionID string `json:"executionId" binding:"required"`
		AgentID     string `json:"agentId"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	onlineAgent := selectAgent(h.registry, req.AgentID)
	if onlineAgent == nil {
		if req.AgentID != "" {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "specified agent not found or not online"})
		} else {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "no available agent"})
		}
		return
	}

	if _, err := onlineAgent.Client.SendCommandWithTimeout("stop_script", map[string]any{
		"script_id": req.ExecutionID,
	}, 10*time.Second); err != nil {
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": err.Error()})
		return
	}

	h.db.Model(&models.Script{}).Where("name = ?", req.ExecutionID).Updates(map[string]any{
		"is_running": false,
		"status":     "stopped",
	})

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.stop", req.ExecutionID, map[string]any{
		"execution_id": req.ExecutionID,
		"agent_id":     onlineAgent.AgentID,
		"ip":           c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleLogs 查询执行日志
// @Summary      脚本执行日志
// @Description  分页读取某次执行的持久化日志；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  object  true  "执行 ID 与分页"  example({"executionId":"demo","offset":0,"limit":100})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Router       /scripts/logs [post]
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
	for _, item := range logs {
		data = append(data, gin.H{
			"timestamp": item.CreatedAt.UnixMilli(),
			"level":     item.Level,
			"message":   item.Output,
		})
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": data})
}
