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

// CreateScriptRequest 创建脚本请求体
type CreateScriptRequest struct {
	// 脚本名（不含扩展名时自动补 .lua；禁止路径穿越字符）
	Name        string `json:"name" binding:"required" example:"demo"`
	Description string `json:"description" example:"血量监控示例脚本"`
}

// ScriptPathRequest 脚本路径请求体（删除脚本 / 读取内容共用）
type ScriptPathRequest struct {
	// 相对 scripts 目录的脚本路径
	Path string `json:"path" binding:"required" example:"demo.lua"`
}

// SaveScriptRequest 保存脚本内容请求体
type SaveScriptRequest struct {
	// 相对 scripts 目录的脚本路径
	Path string `json:"path" binding:"required" example:"demo.lua"`
	// 脚本全文（Lua 源码）
	Content string `json:"content" binding:"required" example:"function main() print(\"hi\") end"`
}

// RunScriptRequest 运行脚本请求体
type RunScriptRequest struct {
	// 相对 scripts 目录的脚本路径
	Path string `json:"path" binding:"required" example:"demo.lua"`
	// 目标 Agent ID（缺省自动选择首个在线 agent）
	AgentID string `json:"agentId" example:"agent-001"`
}

// StopScriptRequest 停止脚本请求体
type StopScriptRequest struct {
	// 执行 ID（脚本名，run 响应中的 executionId）
	ExecutionID string `json:"executionId" binding:"required" example:"demo"`
	// 目标 Agent ID（缺省自动选择首个在线 agent）
	AgentID string `json:"agentId" example:"agent-001"`
}

// QueryLogsRequest 查询执行日志请求体
type QueryLogsRequest struct {
	// 执行 ID（脚本名）
	ExecutionID string `json:"executionId" binding:"required" example:"demo"`
	// 分页偏移（0 起）
	Offset int `json:"offset" example:"0"`
	// 每页条数（1-500，缺省 100）
	Limit int `json:"limit" example:"100"`
}

// LogEntry 单条执行日志
type LogEntry struct {
	// 毫秒时间戳
	Timestamp int64 `json:"timestamp" example:"1712937600000"`
	// 日志级别（info/warn/error）
	Level string `json:"level" example:"info"`
	// 日志内容
	Message string `json:"message" example:"script started"`
}

// ListScriptsResponse 脚本列表响应
type ListScriptsResponse struct {
	Success bool            `json:"success" example:"true"`
	Data    []models.Script `json:"data"`
}

// CreateScriptResponse 创建脚本响应
type CreateScriptResponse struct {
	Success bool          `json:"success" example:"true"`
	Data    models.Script `json:"data"`
}

// RunScriptData 运行脚本响应数据
type RunScriptData struct {
	// 本次执行 ID（等于脚本名，停止时使用）
	ExecutionID string `json:"executionId" example:"demo"`
	// agent 通道的原始响应（command result data）
	Agent map[string]any `json:"agent"`
}

// RunScriptResponse 运行脚本响应
type RunScriptResponse struct {
	Success bool          `json:"success" example:"true"`
	Data    RunScriptData `json:"data"`
}

// LogListResponse 执行日志响应
type LogListResponse struct {
	Success bool       `json:"success" example:"true"`
	Data    []LogEntry `json:"data"`
}

// SuccessResponse 通用成功响应（无数据负载）
type SuccessResponse struct {
	Success bool `json:"success" example:"true"`
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
// @Description  返回 DB 中登记的全部脚本（名称/路径/描述/运行状态）；同时在 /api/v1/scripts 注册。任何登录用户可读（/api 路径），/api/v1 路径需 admin 角色
// @Tags         scripts
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  ListScriptsResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /scripts [get]
// @Router       /v1/scripts [get]
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
// @Description  按 name 生成 Lua 模板文件并写入 DB 记录；名称做路径穿越/空字节/长度校验；需要 scripts:edit 权限（/api/v1 路径需 admin 角色）
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  CreateScriptRequest  true  "脚本元信息"
// @Success      200  {object}  CreateScriptResponse
// @Failure      400  {object}  ErrorResponse "非法名称（路径穿越/空字节/超长）或请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "模板文件写入失败"
// @Router       /scripts [post]
// @Router       /v1/scripts [post]
func (h *ScriptHandler) HandleCreate(c *gin.Context) {
	var req CreateScriptRequest
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
// @Description  按路径删除脚本文件与 DB 记录；入口有 DELETE /scripts、POST /scripts/delete（/api/v1 路径为 DELETE /api/v1/scripts）；需要 scripts:edit 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  ScriptPathRequest  true  "脚本路径"
// @Success      200  {object}  SuccessResponse
// @Failure      400  {object}  ErrorResponse "路径非法或穿越 scripts 目录"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "文件删除失败"
// @Router       /scripts [delete]
// @Router       /v1/scripts [delete]
// @Router       /scripts/delete [post]
func (h *ScriptHandler) HandleDelete(c *gin.Context) {
	var req ScriptPathRequest
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
// @Description  按路径返回脚本全文（路径经 store 安全校验，禁止穿越 scripts 目录）；任何登录用户可读
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  ScriptPathRequest  true  "脚本路径"
// @Success      200  {object}  map[string]interface{} "data 为脚本全文字符串"
// @Failure      400  {object}  ErrorResponse "路径非法或穿越 scripts 目录"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "文件读取失败"
// @Router       /scripts/content [post]
// @Router       /v1/scripts/content [post]
func (h *ScriptHandler) HandleGetContent(c *gin.Context) {
	var req ScriptPathRequest
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
// @Description  覆盖写入脚本文件（路径经 store 安全校验）；需要 scripts:edit 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  SaveScriptRequest  true  "脚本路径与内容"
// @Success      200  {object}  SuccessResponse
// @Failure      400  {object}  ErrorResponse "路径非法或请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "文件写入失败"
// @Router       /scripts/save [post]
// @Router       /v1/scripts/save [post]
func (h *ScriptHandler) HandleSave(c *gin.Context) {
	var req SaveScriptRequest
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
// @Description  经 agent 通道下发 run_script（30s 超时）并将脚本标记为 running，写审计日志；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  RunScriptRequest  true  "脚本与目标 agent"
// @Success      200  {object}  RunScriptResponse
// @Failure      400  {object}  ErrorResponse "路径非法或请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse "agent 不在线或命令执行失败"
// @Router       /scripts/run [post]
// @Router       /v1/scripts/run [post]
func (h *ScriptHandler) HandleRun(c *gin.Context) {
	var req RunScriptRequest
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
// @Description  经 agent 通道下发 stop_script（10s 超时）并将脚本标记为 stopped，写审计日志；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  StopScriptRequest  true  "执行 ID 与目标 agent"
// @Success      200  {object}  SuccessResponse
// @Failure      400  {object}  ErrorResponse "请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse "agent 不在线或命令执行失败"
// @Router       /scripts/stop [post]
// @Router       /v1/scripts/stop [post]
func (h *ScriptHandler) HandleStop(c *gin.Context) {
	var req StopScriptRequest
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
// @Description  分页读取某次执行的持久化日志（offset/limit，limit 1-500 缺省 100，按 id 升序）；需要 scripts:run 权限
// @Tags         scripts
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  QueryLogsRequest  true  "执行 ID 与分页"
// @Success      200  {object}  LogListResponse
// @Failure      400  {object}  ErrorResponse "请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /scripts/logs [post]
// @Router       /v1/scripts/logs [post]
func (h *ScriptHandler) HandleLogs(c *gin.Context) {
	var req QueryLogsRequest
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
