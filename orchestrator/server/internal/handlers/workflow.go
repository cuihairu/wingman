package handlers

import (
	"net/http"
	"strconv"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// WorkflowHandler 工作流管理 handler
type WorkflowHandler struct {
	engine *workflow.Engine
	db     *gorm.DB
}

// NewWorkflowHandler 创建工作流 handler
func NewWorkflowHandler(engine *workflow.Engine, db *gorm.DB) *WorkflowHandler {
	return &WorkflowHandler{
		engine: engine,
		db:     db,
	}
}

// HandleList 获取工作流列表
// GET /api/workflows
func (h *WorkflowHandler) HandleList(c *gin.Context) {
	workflows, err := h.engine.ListWorkflows()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    workflows,
	})
}

// HandleGet 获取工作流详情（含步骤状态）
// GET /api/workflows/:id
func (h *WorkflowHandler) HandleGet(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid id"})
		return
	}

	wf, stepStatuses, err := h.engine.GetWorkflow(uint(id))
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "workflow not found"})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"id":          wf.ID,
			"name":        wf.Name,
			"description": wf.Description,
			"status":      wf.Status,
			"steps":       wf.GetSteps(),
			"sharedContext": wf.GetContext(),
			"stepStatus":  stepStatuses,
			"createdTime": wf.CreatedAt.UnixMilli(),
			"startTime":   wf.StartTime,
			"endTime":     wf.EndTime,
		},
	})
}

// HandleCreate 创建并提交工作流
// POST /api/workflows
func (h *WorkflowHandler) HandleCreate(c *gin.Context) {
	var req struct {
		Name         string                   `json:"name" binding:"required"`
		Description  string                   `json:"description"`
		Steps        []models.WorkflowStep    `json:"steps" binding:"required"`
		SharedContext map[string]any           `json:"sharedContext"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	wf := &models.Workflow{
		Name:        req.Name,
		Description: req.Description,
		Status:      "pending",
	}
	if err := wf.SetSteps(req.Steps); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}
	if req.SharedContext != nil {
		if err := wf.SetContext(req.SharedContext); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
			return
		}
	}

	if err := h.db.Create(wf).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": err.Error()})
		return
	}

	// 异步提交执行
	go func() {
		if err := h.engine.Submit(wf); err != nil {
			_ = err // Submit 内部会更新状态
		}
	}()

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"workflowId": wf.ID,
		},
	})
}

// HandleCancel 取消工作流
// POST /api/workflows/:id/cancel
func (h *WorkflowHandler) HandleCancel(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid id"})
		return
	}

	if err := h.engine.Cancel(uint(id)); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// HandleGetWorkers 获取工作流的 worker 状态
// GET /api/workflows/:id/workers
func (h *WorkflowHandler) HandleGetWorkers(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid id"})
		return
	}

	var stepStatuses []models.StepStatus
	h.db.Where("workflow_id = ?", uint(id)).Find(&stepStatuses)

	workers := make([]gin.H, 0)
	seen := make(map[string]bool)
	for _, ss := range stepStatuses {
		if ss.WorkerID == "" || seen[ss.WorkerID] {
			continue
		}
		seen[ss.WorkerID] = true
		workers = append(workers, gin.H{
			"workerId":  ss.WorkerID,
			"stepId":    ss.StepID,
			"status":    ss.Status,
			"startTime": ss.StartTime,
			"endTime":   ss.EndTime,
		})
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": workers})
}

// HandleGetStepStatus 获取步骤状态
// GET /api/workflows/:id/steps/:stepId/status
func (h *WorkflowHandler) HandleGetStepStatus(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid id"})
		return
	}
	stepID := c.Param("stepId")

	var ss models.StepStatus
	if err := h.db.Where("workflow_id = ? AND step_id = ?", uint(id), stepID).First(&ss).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "step not found"})
		return
	}

	// 检查运行中的执行状态
	if exec, ok := h.engine.GetExecution(uint(id)); ok {
		if execSS, ok := exec.StepState[stepID]; ok {
			ss.Status = execSS.Status
			ss.Message = execSS.Message
			ss.WorkerID = execSS.WorkerID
			if execSS.StartTime != nil {
				ss.StartTime = execSS.StartTime
			}
			if execSS.EndTime != nil {
				ss.EndTime = execSS.EndTime
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": ss})
}

