package handlers

import (
	"encoding/json"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// TriggerConfigRequest 触发器配置请求体。
// 字段与 runtime TriggerConfig 契约对齐
// （lib/wingman/src/rpc/handlers/trigger_handler.cpp applyTriggerConfigJson），
// 指针字段用于区分「未提供」与「显式 false/0」（更新为部分字段语义）。
type TriggerConfigRequest struct {
	// 触发器名称（新增时必填，runtime 缺省 Unnamed Trigger）
	Name     string `json:"name" example:"hp-watch"`
	Enabled  *bool  `json:"enabled,omitempty" example:"true"`
	OneShot  *bool  `json:"oneShot,omitempty" example:"false"`
	Cooldown *int   `json:"cooldown,omitempty" example:"3000"`
	// condition 触发条件（11 种类型：ColorFound/ColorLost/ImageFound/ImageLost/
	// WindowOpened/WindowClosed/ProcessStarted/ProcessStopped/TimeElapsed/
	// HotkeyPressed/PixelChanged）
	Condition *TriggerConditionRequest `json:"condition,omitempty"`
	// actions 触发动作序列（RunScript/Click/KeyPress/Type/StopScript/
	// PauseScript/ShowMessage/PlayAudio/Log/Delay）
	Actions []TriggerActionRequest `json:"actions,omitempty"`
}

// TriggerConditionRequest 触发条件（value 语义随 type 变化：
// 颜色 #rrggbb / 图片路径 / 窗口标题 / 进程名 / 毫秒数 / 键名）
type TriggerConditionRequest struct {
	Type      string                `json:"type" example:"ColorFound"`
	Value     string                `json:"value,omitempty" example:"#ff0000"`
	Tolerance *int                  `json:"tolerance,omitempty" example:"10"`
	Interval  *int                  `json:"interval,omitempty" example:"1000"`
	Enabled   *bool                 `json:"enabled,omitempty" example:"true"`
	Region    *TriggerRegionRequest `json:"region,omitempty"`
}

// TriggerRegionRequest 像素检测区域（屏幕绝对坐标）
type TriggerRegionRequest struct {
	X      int `json:"x,omitempty" example:"100"`
	Y      int `json:"y,omitempty" example:"200"`
	Width  int `json:"width,omitempty" example:"50"`
	Height int `json:"height,omitempty" example:"50"`
}

// TriggerActionRequest 触发动作（value 语义随 type 变化：脚本路径/按键/文本/消息/音频路径）
type TriggerActionRequest struct {
	Type  string `json:"type" example:"RunScript"`
	Value string `json:"value,omitempty" example:"heal.lua"`
	X     int    `json:"x,omitempty" example:"0"`
	Y     int    `json:"y,omitempty" example:"0"`
	Delay int    `json:"delay,omitempty" example:"500"`
}

// TriggerToggleRequest 切换触发器启用状态请求体
type TriggerToggleRequest struct {
	// runtime 分配的触发器 ID（trigger.list 返回的字符串数字）
	ID string `json:"id" binding:"required" example:"42"`
}

// toMap 将请求体转为通用 map（经 JSON 往返，保留 omitempty 语义，
// 未提供的指针字段不出现在透传给 runtime 的 config 中）。
func (r *TriggerConfigRequest) toMap() map[string]any {
	raw, err := json.Marshal(r)
	if err != nil {
		return map[string]any{}
	}
	var out map[string]any
	if err := json.Unmarshal(raw, &out); err != nil {
		return map[string]any{}
	}
	return out
}

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
// @Description  经 agent 通道读取 runtime TriggerManager 的 trigger.list；任何登录用户可读
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"  example(agent-001)
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
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
// @Description  经 agent 通道下发 trigger.toggle（启用↔停用翻转）；需要 agents:manage 权限
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId  path  string  true  "Agent ID"  example(agent-001)
// @Param        request  body  TriggerToggleRequest  true  "触发器 ID"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
// @Router       /agents/{agentId}/triggers/toggle [post]
func (h *TriggerHandler) HandleToggle(c *gin.Context) {
	agentID := c.Param("agentId")

	var req TriggerToggleRequest
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
// @Param        agentId  path  string  true  "Agent ID"  example(agent-001)
// @Param        request  body  TriggerConfigRequest  true  "触发器配置（name 必填）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
// @Router       /agents/{agentId}/triggers [post]
func (h *TriggerHandler) HandleCreate(c *gin.Context) {
	agentID := c.Param("agentId")

	var req TriggerConfigRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger config required"})
		return
	}
	if strings.TrimSpace(req.Name) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger name is required"})
		return
	}

	resp, ok := h.dispatch(c, "trigger.add", map[string]any{"config": req.toMap()}, 10*time.Second)
	if !ok {
		return
	}

	data, _ := resp["data"].(map[string]any)
	triggerID, _ := data["id"].(string)

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_create", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": triggerID,
		"name":       req.Name,
		"ip":         c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    gin.H{"id": triggerID, "name": req.Name},
	})
}

// HandleUpdate 更新指定 agent 上的触发器配置
// @Summary      更新 Agent 触发器
// @Description  经 agent 通道下发 trigger.update（部分字段更新，未提供的字段保持原值）；需要 agents:manage 权限
// @Tags         agents
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        agentId    path  string  true  "Agent ID"  example(agent-001)
// @Param        triggerId  path  int     true  "触发器 ID"  example(42)
// @Param        request    body  TriggerConfigRequest  true  "触发器配置（部分字段）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
// @Router       /agents/{agentId}/triggers/{triggerId} [put]
func (h *TriggerHandler) HandleUpdate(c *gin.Context) {
	agentID := c.Param("agentId")
	triggerID := c.Param("triggerId")

	if _, err := strconv.ParseUint(triggerID, 10, 64); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid trigger id"})
		return
	}

	var req TriggerConfigRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger config required"})
		return
	}

	_, ok := h.dispatch(c, "trigger.update", map[string]any{
		"id":     triggerID,
		"config": req.toMap(),
	}, 10*time.Second)
	if !ok {
		return
	}

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.trigger_update", agentID, map[string]any{
		"agent_id":   agentID,
		"trigger_id": triggerID,
		"changes":    req.toMap(),
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
// @Param        agentId    path  string  true  "Agent ID"  example(agent-001)
// @Param        triggerId  path  int     true  "触发器 ID"  example(42)
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      502  {object}  ErrorResponse
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
