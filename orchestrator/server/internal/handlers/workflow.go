package handlers

import (
	"net/http"
	"strconv"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/workflow"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

type WorkflowHandler struct {
	engine *workflow.Engine
	db     *gorm.DB
}

func NewWorkflowHandler(engine *workflow.Engine, db *gorm.DB) *WorkflowHandler {
	return &WorkflowHandler{
		engine: engine,
		db:     db,
	}
}

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

// HandleListTemplates 返回内置工作流模板目录（只读，所有登录用户可访问）
func (h *WorkflowHandler) HandleListTemplates(c *gin.Context) {
	templates := workflow.BuiltinTemplates()
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    templates,
	})
}

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
			"id":            wf.ID,
			"name":          wf.Name,
			"description":   wf.Description,
			"status":        wf.Status,
			"steps":         wf.GetSteps(),
			"sharedContext": wf.GetContext(),
			"stepStatus":    stepStatuses,
			"createdTime":   wf.CreatedAt.UnixMilli(),
			"startTime":     wf.StartTime,
			"endTime":       wf.EndTime,
		},
	})
}

func (h *WorkflowHandler) HandleCreate(c *gin.Context) {
	var req struct {
		Name          string                `json:"name" binding:"required"`
		Description   string                `json:"description"`
		Steps         []models.WorkflowStep `json:"steps" binding:"required"`
		SharedContext map[string]any        `json:"sharedContext"`
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

	go func() {
		if err := h.engine.Submit(wf); err != nil {
			_ = err
		}
	}()

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "workflow.create", wf.Name, map[string]any{
		"workflow_id": wf.ID,
		"workflow":    wf.Name,
		"ip":          c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"workflowId": wf.ID,
		},
	})
}

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

	_, actor, _ := middleware.GetCurrentUser(c)
	WriteAuditLog(h.db, actor, "workflow.cancel", c.Param("id"), map[string]any{
		"workflow_id": id,
		"ip":          c.ClientIP(),
	})

	c.JSON(http.StatusOK, gin.H{"success": true})
}

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
	for _, status := range stepStatuses {
		if status.WorkerID == "" || seen[status.WorkerID] {
			continue
		}
		seen[status.WorkerID] = true
		workers = append(workers, gin.H{
			"workerId":  status.WorkerID,
			"stepId":    status.StepID,
			"status":    status.Status,
			"startTime": status.StartTime,
			"endTime":   status.EndTime,
		})
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": workers})
}

func (h *WorkflowHandler) HandleGetStepStatus(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid id"})
		return
	}
	stepID := c.Param("stepId")

	var status models.StepStatus
	if err := h.db.Where("workflow_id = ? AND step_id = ?", uint(id), stepID).First(&status).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "step not found"})
		return
	}

	if execution, ok := h.engine.GetExecution(uint(id)); ok {
		if state, ok := execution.StepState[stepID]; ok {
			status.Status = state.Status
			status.Message = state.Message
			status.WorkerID = state.WorkerID
			if state.StartTime != nil {
				status.StartTime = state.StartTime
			}
			if state.EndTime != nil {
				status.EndTime = state.EndTime
			}
		}
	}

	c.JSON(http.StatusOK, gin.H{"success": true, "data": status})
}
