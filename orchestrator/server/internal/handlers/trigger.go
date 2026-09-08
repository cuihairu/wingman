package handlers

import (
	"net/http"
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
// agent 不在线 → 502 agent not connected；命令失败/超时 → 502 透传错误详情。
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
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": errText})
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
