package handlers

import (
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// TriggerHandler 触发器处理器：把 runtime 本地 IPC 已有的 trigger.* 能力
// 经 agent 通道透传给 Dashboard（Dispatcher Reuse）。
type TriggerHandler struct {
	registry *agent.Registry
	db       *gorm.DB
}

// NewTriggerHandler 创建触发器处理器
func NewTriggerHandler(registry *agent.Registry, db *gorm.DB) *TriggerHandler {
	return &TriggerHandler{
		registry: registry,
		db:       db,
	}
}

// dispatch 向指定 agent 发送 trigger 命令并统一错误语义：
// agent 不在线 → 502 agent not connected；命令失败/超时 → 502 透传错误详情；
// runtime 明确报告 not found（触发器不存在）→ 404。
// 返回 (runtime 响应, 是否继续后续处理)；false 时响应已写出。
func (h *TriggerHandler) dispatch(c *gin.Context, method string, payload map[string]any, timeout time.Duration) (map[string]any, bool) {
	agentID := c.Param("agentId")

	onlineAgent := selectAgent(h.registry, agentID)
	if onlineAgent == nil {
		if agentID != "" {
			c.JSON(http.StatusBadGateway, gin.H{
				"success": false,
				"error":   "agent not connected: " + agentID,
			})
		} else {
			c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "no available agent"})
		}
		return nil, false
	}

	resp, err := onlineAgent.Client.SendCommandWithTimeout(method, payload, timeout)
	if err != nil {
		c.JSON(http.StatusBadGateway, gin.H{
			"success": false,
			"error":   "trigger command failed: " + err.Error(),
		})
		return nil, false
	}
	if success, ok := resp["success"].(bool); ok && !success {
		errText, _ := resp["error"].(string)
		if errText == "" {
			errText, _ = resp["message"].(string)
		}
		if errText == "" {
			errText = "trigger command failed"
		}
		status := http.StatusBadGateway
		if strings.Contains(strings.ToLower(errText), "not found") {
			status = http.StatusNotFound
		}
		c.JSON(status, gin.H{"success": false, "error": errText})
		return nil, false
	}
	return resp, true
}

// HandleList 获取指定 agent 的触发器列表
// @Summary      Agent 触发器列表
// @Description  经 agent 通道读取 runtime TriggerManager 的 trigger.list
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Success      200  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /agents/{agentId}/triggers [get]
func (h *TriggerHandler) HandleList(c *gin.Context) {
	agentID := c.Param("agentId")

	resp, ok := h.dispatch(c, "trigger.list", nil, 10*time.Second)
	if !ok {
		return
	}

	data, _ := resp["data"].(map[string]any)
	triggers, _ := data["triggers"].([]any)
	if triggers == nil {
		triggers = []any{}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    triggers,
		"agentId": agentID,
	})
}

// HandleToggle 切换触发器启用状态
// @Summary      切换 Agent 触发器启用状态
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Param        request body object true "触发器 ID" example({"id":"1"})
// @Success      200  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /agents/{agentId}/triggers/toggle [post]
func (h *TriggerHandler) HandleToggle(c *gin.Context) {
	agentID := c.Param("agentId")

	var req struct {
		ID string `json:"id" binding:"required"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger id required"})
		return
	}

	resp, ok := h.dispatch(c, "trigger.toggle", map[string]any{
		"id": req.ID,
	}, 10*time.Second)
	if !ok {
		return
	}

	data, _ := resp["data"].(map[string]any)
	enabled, _ := data["enabled"].(bool)

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_toggle", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": req.ID,
		"enabled":    enabled,
		"ip":         c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"id":      req.ID,
			"enabled": enabled,
		},
	})
}

// HandleCreate 在指定 agent 上新增触发器
// @Summary      新增 Agent 触发器
// @Description  经 agent 通道下发 trigger.add（Dispatcher Reuse，配置透传 runtime TriggerManager）；需要 agents:manage 权限
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"
// @Param        request  body  object  true  "触发器配置"  example({"name":"hp-watch","enabled":true,"oneShot":false,"cooldown":3000,"condition":{"type":"ColorFound","value":"#ff0000","tolerance":10,"interval":1000,"region":{"x":0,"y":0,"width":100,"height":100}},"actions":[{"type":"RunScript","value":"heal.lua"}]})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /agents/{agentId}/triggers [post]
func (h *TriggerHandler) HandleCreate(c *gin.Context) {
	agentID := c.Param("agentId")

	var req map[string]any
	if err := c.ShouldBindJSON(&req); err != nil || req == nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger config required"})
		return
	}
	name, _ := req["name"].(string)
	if strings.TrimSpace(name) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger name is required"})
		return
	}

	resp, ok := h.dispatch(c, "trigger.add", map[string]any{"config": req}, 10*time.Second)
	if !ok {
		return
	}

	data, _ := resp["data"].(map[string]any)
	triggerID, _ := data["id"].(string)

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_create", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": triggerID,
		"name":       name,
		"ip":         c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    gin.H{"id": triggerID, "name": name},
	})
}

// HandleUpdate 更新指定 agent 上的触发器配置
// @Summary      更新 Agent 触发器
// @Description  经 agent 通道下发 trigger.update（部分字段更新，缺省保持原值）；需要 agents:manage 权限
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId    path  string  true  "Agent ID"
// @Param        triggerId  path  string  true  "触发器 ID（数字）"
// @Param        request    body  object  true  "触发器配置（部分字段）"  example({"name":"hp-watch-v2","cooldown":5000})
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      404  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /agents/{agentId}/triggers/{triggerId} [put]
func (h *TriggerHandler) HandleUpdate(c *gin.Context) {
	agentID := c.Param("agentId")
	triggerID := c.Param("triggerId")

	if _, err := strconv.ParseUint(triggerID, 10, 64); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid trigger id"})
		return
	}

	var req map[string]any
	if err := c.ShouldBindJSON(&req); err != nil || req == nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger config required"})
		return
	}

	_, ok := h.dispatch(c, "trigger.update", map[string]any{
		"id":     triggerID,
		"config": req,
	}, 10*time.Second)
	if !ok {
		return
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_update", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": triggerID,
		"changes":    req,
		"ip":         c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    gin.H{"id": triggerID},
	})
}

// HandleRemove 删除指定 agent 上的触发器
// @Summary      删除 Agent 触发器
// @Description  经 agent 通道下发 trigger.remove；需要 agents:manage 权限
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        agentId    path  string  true  "Agent ID"
// @Param        triggerId  path  string  true  "触发器 ID（数字）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  map[string]interface{}
// @Failure      404  {object}  map[string]interface{}
// @Failure      502  {object}  map[string]interface{}
// @Router       /agents/{agentId}/triggers/{triggerId} [delete]
func (h *TriggerHandler) HandleRemove(c *gin.Context) {
	agentID := c.Param("agentId")
	triggerID := c.Param("triggerId")

	if _, err := strconv.ParseUint(triggerID, 10, 64); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid trigger id"})
		return
	}

	_, ok := h.dispatch(c, "trigger.remove", map[string]any{"id": triggerID}, 10*time.Second)
	if !ok {
		return
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_remove", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": triggerID,
		"ip":         c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    gin.H{"id": triggerID},
	})
}
