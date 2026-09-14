package handlers

import (
	"errors"
	"fmt"
	"net/http"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// fan-out 并发上限：单次批量下发的最大并行 agent 命令数
const maxBatchConcurrency = 8

// 选择器条目上限：防止误传超大列表拖垮请求
const maxBatchSelectorItems = 500

// BatchAgentSelector 批量目标选择器
// @Description 批量操作的目标选择；agentIds 与 tags 任一非空即可，
// @Description 同时提供取并集（OR）。匹配 0 台时返回 total=0 而非错误。
type BatchAgentSelector struct {
	// agentIds 目标 agent ID 列表
	AgentIDs []string `json:"agentIds,omitempty" example:"agent-001"`
	// tags 按标签匹配（命中任一标签即入选）
	Tags []string `json:"tags,omitempty" example:"prod"`
}

// BatchRunScriptRequest 批量运行脚本请求
type BatchRunScriptRequest struct {
	BatchAgentSelector
	// Path 相对 scripts 目录的脚本路径（每台 agent 下发同一路径）
	Path string `json:"path" binding:"required" example:"demo.lua"`
}

// BatchStopScriptRequest 批量停止脚本请求
type BatchStopScriptRequest struct {
	BatchAgentSelector
	// ExecutionID 执行 ID（等于脚本名，与单 agent stop_script 一致）
	ExecutionID string `json:"executionId" binding:"required" example:"demo"`
}

// BatchTriggerRequest 批量下发触发器请求（trigger.add 语义，
// 配置字段与单 agent trigger.add 一致）
type BatchTriggerRequest struct {
	BatchAgentSelector
	TriggerConfigRequest
}

// BatchAgentResult 单台 agent 的批量操作结果
type BatchAgentResult struct {
	AgentID string `json:"agentId" example:"agent-001"`
	Success bool   `json:"success" example:"true"`
	// Error 失败原因（"agent offline" / 命令错误详情等）
	Error string `json:"error,omitempty" example:"agent offline"`
}

// BatchSummary 批量操作汇总
// @Description 部分失败不算整体失败：HTTP 200，逐台在 results 标注。
type BatchSummary struct {
	// Total 匹配的目标 agent 数
	Total int `json:"total" example:"5"`
	// Succeeded 成功台数
	Succeeded int `json:"succeeded" example:"3"`
	// Failed 失败台数（含离线）
	Failed int `json:"failed" example:"2"`
	// Results 逐台结果（与目标顺序一致）
	Results []BatchAgentResult `json:"results"`
}

// BatchHandler 批量操作处理器：按选择器 fan-out 既有 agent 命令
// （run_script / stop_script / trigger.add），不引入新命令与传输通道。
type BatchHandler struct {
	db       *gorm.DB
	registry *agent.Registry
	store    scripts.Store
}

// NewBatchHandler 创建批量操作处理器
func NewBatchHandler(db *gorm.DB, scriptsDir string, registry *agent.Registry) *BatchHandler {
	return &BatchHandler{
		db:       db,
		registry: registry,
		store:    scripts.NewStore(scriptsDir),
	}
}

// 批量选择器校验错误
var (
	errBatchEmptySelector    = errors.New("selector requires agentIds or tags")
	errBatchSelectorTooLarge = fmt.Errorf("selector supports at most %d items", maxBatchSelectorItems)
)

// parseBatchSelector 清洗选择器条目并校验：皆空 → 错误；任一超过上限 → 错误。
// 返回清洗后的 (agentIds, tags)。
func parseBatchSelector(agentIDs, tags []string) ([]string, []string, error) {
	clean := func(items []string) []string {
		out := make([]string, 0, len(items))
		for _, v := range items {
			if v = strings.TrimSpace(v); v != "" {
				out = append(out, v)
			}
		}
		return out
	}
	ids := clean(agentIDs)
	tagList := clean(tags)
	if len(ids) == 0 && len(tagList) == 0 {
		return nil, nil, errBatchEmptySelector
	}
	if len(ids) > maxBatchSelectorItems || len(tagList) > maxBatchSelectorItems {
		return nil, nil, errBatchSelectorTooLarge
	}
	return ids, tagList, nil
}

// resolveTargets 从注册表快照中解析并集目标（agentIds 精确匹配 ∪ tags 命中任一），
// 去重后按 AgentID 排序返回（注册表为 map，遍历顺序随机，排序保证结果顺序确定）。
func (h *BatchHandler) resolveTargets(agentIDs, tags []string) []*agent.AgentInfo {
	idSet := make(map[string]bool, len(agentIDs))
	tagSet := make(map[string]bool, len(tags))
	for _, id := range agentIDs {
		idSet[id] = true
	}
	for _, t := range tags {
		tagSet[t] = true
	}

	seen := map[string]bool{}
	targets := make([]*agent.AgentInfo, 0)
	for _, info := range h.registry.List() {
		matched := idSet[info.AgentID]
		if !matched {
			for _, t := range info.Tags {
				if tagSet[t] {
					matched = true
					break
				}
			}
		}
		if !matched || seen[info.AgentID] {
			continue
		}
		seen[info.AgentID] = true
		targets = append(targets, info)
	}
	sort.Slice(targets, func(i, j int) bool {
		return targets[i].AgentID < targets[j].AgentID
	})
	return targets
}

// runBatch 并发执行 fn（信号量限流），结果按目标顺序写入。
func runBatch(targets []*agent.AgentInfo, fn func(*agent.AgentInfo) (bool, string)) BatchSummary {
	summary := BatchSummary{
		Total:   len(targets),
		Results: make([]BatchAgentResult, len(targets)),
	}
	if len(targets) == 0 {
		return summary
	}

	sem := make(chan struct{}, maxBatchConcurrency)
	var wg sync.WaitGroup
	for i, target := range targets {
		wg.Add(1)
		go func(idx int, info *agent.AgentInfo) {
			defer wg.Done()
			sem <- struct{}{}
			defer func() { <-sem }()
			ok, errMsg := fn(info)
			// 各 goroutine 只写自己的下标，汇总计数在 Wait 之后进行
			summary.Results[idx] = BatchAgentResult{AgentID: info.AgentID, Success: ok, Error: errMsg}
		}(i, target)
	}
	wg.Wait()
	for _, r := range summary.Results {
		if r.Success {
			summary.Succeeded++
		} else {
			summary.Failed++
		}
	}
	return summary
}

// commandErrorText 归一单台失败文案：连接错误 → resp["error"] → resp["message"] → 兜底。
func commandErrorText(resp map[string]any, err error) string {
	if err != nil {
		return err.Error()
	}
	if text, ok := resp["error"].(string); ok && text != "" {
		return text
	}
	if text, ok := resp["message"].(string); ok && text != "" {
		return text
	}
	return "agent command failed"
}

// HandleBatchRunScript 批量运行脚本
// @Summary      批量运行脚本
// @Description  按选择器（agentIds/tags 并集）对多台 agent 并发下发 run_script。
// @Description  脚本路径先经服务端解析（非法路径 400 且不下发）。部分失败不算整体失败：
// @Description  一律 200，逐台结果在 results 标注（离线 agent 记 "agent offline"）。
// @Tags         scripts
// @Produce      json
// @Security     BearerAuth
// @Param        request body BatchRunScriptRequest true "选择器 + 脚本路径"
// @Success      200  {object}  BatchSummaryResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /agents/batch/run-script [post]
func (h *BatchHandler) HandleBatchRunScript(c *gin.Context) {
	var req BatchRunScriptRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	ids, tagList, err := parseBatchSelector(req.AgentIDs, req.Tags)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	// fan-out 之前一次性解析，非法路径零下发
	scriptPath, err := h.store.Resolve(req.Path)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	targets := h.resolveTargets(ids, tagList)
	summary := runBatch(targets, func(info *agent.AgentInfo) (bool, string) {
		if info.Client == nil || info.Status != agent.StatusOnline {
			return false, "agent offline"
		}
		resp, err := info.Client.SendCommandWithTimeout("run_script", map[string]any{
			"path": scriptPath,
		}, 30*time.Second)
		if err != nil {
			return false, err.Error()
		}
		if success, ok := resp["success"].(bool); ok && !success {
			return false, commandErrorText(resp, nil)
		}
		return true, ""
	})

	// DB 写在 fan-out 之后串行执行
	if summary.Succeeded > 0 {
		h.db.Model(&models.Script{}).Where("path = ?", scripts.DisplayPath(h.store.Root(), scriptPath)).Updates(map[string]any{
			"is_running": true,
			"status":     "running",
		})
	}
	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.batch_run", req.Path, batchAuditMeta(ids, tagList, summary))

	c.JSON(http.StatusOK, gin.H{"success": true, "data": summary})
}

// HandleBatchStopScript 批量停止脚本
// @Summary      批量停止脚本
// @Description  按选择器对多台 agent 并发下发 stop_script，语义同批量运行脚本
// @Tags         scripts
// @Produce      json
// @Security     BearerAuth
// @Param        request body BatchStopScriptRequest true "选择器 + 执行 ID"
// @Success      200  {object}  BatchSummaryResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /agents/batch/stop-script [post]
func (h *BatchHandler) HandleBatchStopScript(c *gin.Context) {
	var req BatchStopScriptRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	ids, tagList, err := parseBatchSelector(req.AgentIDs, req.Tags)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	targets := h.resolveTargets(ids, tagList)
	summary := runBatch(targets, func(info *agent.AgentInfo) (bool, string) {
		if info.Client == nil || info.Status != agent.StatusOnline {
			return false, "agent offline"
		}
		_, err := info.Client.SendCommandWithTimeout("stop_script", map[string]any{
			"script_id": req.ExecutionID,
		}, 10*time.Second)
		if err != nil {
			return false, err.Error()
		}
		return true, ""
	})

	if summary.Succeeded > 0 {
		h.db.Model(&models.Script{}).Where("name = ?", req.ExecutionID).Updates(map[string]any{
			"is_running": false,
			"status":     "stopped",
		})
	}
	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "script.batch_stop", req.ExecutionID, batchAuditMeta(ids, tagList, summary))

	c.JSON(http.StatusOK, gin.H{"success": true, "data": summary})
}

// HandleBatchTrigger 批量下发触发器
// @Summary      批量下发触发器
// @Description  按选择器对多台 agent 并发下发 trigger.add，配置字段与单 agent
// @Description  POST /api/agents/:agentId/triggers 完全一致（name 必填）
// @Tags         agents
// @Produce      json
// @Security     BearerAuth
// @Param        request body BatchTriggerRequest true "选择器 + 触发器配置"
// @Success      200  {object}  BatchSummaryResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /agents/batch/trigger [post]
func (h *BatchHandler) HandleBatchTrigger(c *gin.Context) {
	var req BatchTriggerRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	ids, tagList, err := parseBatchSelector(req.AgentIDs, req.Tags)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}
	if strings.TrimSpace(req.Name) == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "trigger name is required"})
		return
	}
	config := req.TriggerConfigRequest.toMap()

	targets := h.resolveTargets(ids, tagList)
	summary := runBatch(targets, func(info *agent.AgentInfo) (bool, string) {
		if info.Client == nil || info.Status != agent.StatusOnline {
			return false, "agent offline"
		}
		resp, err := info.Client.SendCommandWithTimeout("trigger.add", map[string]any{
			"config": config,
		}, 10*time.Second)
		if err != nil {
			return false, err.Error()
		}
		if success, ok := resp["success"].(bool); ok && !success {
			return false, commandErrorText(resp, nil)
		}
		return true, ""
	})

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "agent.batch_trigger_add", req.Name, batchAuditMeta(ids, tagList, summary))

	c.JSON(http.StatusOK, gin.H{"success": true, "data": summary})
}

// batchAuditMeta 批量审计 meta：选择器 + 结果摘要 + 命中目标。
func batchAuditMeta(ids, tags []string, summary BatchSummary) map[string]any {
	matched := make([]string, 0, len(summary.Results))
	for _, r := range summary.Results {
		matched = append(matched, r.AgentID)
	}
	return map[string]any{
		"selector_agent_ids": ids,
		"selector_tags":      tags,
		"total":              summary.Total,
		"succeeded":          summary.Succeeded,
		"failed":             summary.Failed,
		"matched_agent_ids":  matched,
	}
}
