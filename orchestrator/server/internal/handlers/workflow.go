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

// CreateWorkflowRequest 创建工作流请求体
type CreateWorkflowRequest struct {
	// 工作流名称
	Name string `json:"name" binding:"required" example:"monitor"`
	// 工作流描述
	Description string `json:"description" example:"单步监控流水线"`
	// 步骤列表（type: script/wait/condition/screenshot；通过 dependsOn 声明依赖）
	Steps []models.WorkflowStep `json:"steps" binding:"required"`
	// 跨步骤共享上下文（步骤间传参）
	SharedContext map[string]any `json:"sharedContext"`
}

// ListWorkflowsResponse 工作流列表响应
type ListWorkflowsResponse struct {
	Success bool              `json:"success" example:"true"`
	Data    []models.Workflow `json:"data"`
}

// WorkflowDetailData 工作流详情数据
type WorkflowDetailData struct {
	// 工作流 ID
	ID uint `json:"id" example:"1"`
	// 名称
	Name string `json:"name" example:"monitor"`
	// 描述
	Description string `json:"description" example:"单步监控流水线"`
	// 状态：pending/running/completed/failed/cancelled
	Status string `json:"status" example:"completed"`
	// 步骤定义列表
	Steps []models.WorkflowStep `json:"steps"`
	// 共享上下文快照
	SharedContext map[string]any `json:"sharedContext"`
	// 各步骤实时状态（stepId → 状态）
	StepStatus map[string]any `json:"stepStatus"`
	// 创建时间（毫秒）
	CreatedTime int64 `json:"createdTime" example:"1712937600000"`
	// 开始时间（毫秒，未开始为 0）
	StartTime int64 `json:"startTime" example:"1712937610000"`
	// 结束时间（毫秒，未结束为 0）
	EndTime int64 `json:"endTime" example:"1712937620000"`
}

// WorkflowDetailResponse 工作流详情响应
type WorkflowDetailResponse struct {
	Success bool               `json:"success" example:"true"`
	Data    WorkflowDetailData `json:"data"`
}

// WorkerEntry 工作流 worker（去重后的 agent）
type WorkerEntry struct {
	// Agent ID
	WorkerID string `json:"workerId" example:"agent-001"`
	// 该 worker 承担的首个步骤 ID
	StepID string `json:"stepId" example:"s1"`
	// 步骤状态
	Status string `json:"status" example:"completed"`
	// 开始时间（毫秒）
	StartTime int64 `json:"startTime" example:"1712937610000"`
	// 结束时间（毫秒）
	EndTime int64 `json:"endTime" example:"1712937620000"`
}

// WorkerListResponse 工作流 worker 列表响应
type WorkerListResponse struct {
	Success bool          `json:"success" example:"true"`
	Data    []WorkerEntry `json:"data"`
}

// StepStatusResponse 单步状态响应（DB 记录与引擎内存快照合并）
type StepStatusResponse struct {
	Success bool              `json:"success" example:"true"`
	Data    models.StepStatus `json:"data"`
}

// HandleList 工作流列表
// @Summary      工作流列表
// @Description  返回所有工作流记录（含状态：pending/running/completed/failed/cancelled）；任何登录用户可读
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  ListWorkflowsResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "数据库查询失败"
// @Router       /workflows [get]
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
// @Summary      工作流模板库
// @Description  内置模板目录：单步监控/并行采集/串行流水线/fan-out/独立步骤类型 wait-condition-screenshot；可直接作为创建工作流的 steps 骨架
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{} "data 为模板数组（name/description/steps）"
// @Failure      401  {object}  ErrorResponse
// @Router       /workflow-templates [get]
func (h *WorkflowHandler) HandleListTemplates(c *gin.Context) {
	templates := workflow.BuiltinTemplates()
	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data":    templates,
	})
}

// HandleGet 工作流详情
// @Summary      工作流详情
// @Description  返回工作流定义、步骤状态与共享上下文快照（运行中与已结束均可查询）
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Param        id  path  int  true  "工作流 ID"  example(1)
// @Success      200  {object}  WorkflowDetailResponse
// @Failure      400  {object}  ErrorResponse "id 非数字"
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse "工作流不存在"
// @Router       /workflows/{id} [get]
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

// HandleCreate 创建并提交工作流
// @Summary      创建工作流
// @Description  校验步骤定义（依赖关系/worker 存在性）后提交引擎调度，调度失败时回滚 DB 记录；需要 workflows:run 权限
// @Tags         workflows
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  CreateWorkflowRequest  true  "工作流定义"
// @Success      200  {object}  map[string]interface{} "data.workflowId 为新建工作流 ID"
// @Failure      400  {object}  ErrorResponse "步骤校验失败或请求体格式错误"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse "序列化或 DB 写入失败"
// @Router       /workflows [post]
func (h *WorkflowHandler) HandleCreate(c *gin.Context) {
	var req CreateWorkflowRequest
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

	if err := h.engine.Submit(wf); err != nil {
		// 回滚刚创建的无效工作流，避免数据库残留永远 pending 的坏记录。
		h.db.Unscoped().Delete(&models.StepStatus{}, "workflow_id = ?", wf.ID)
		h.db.Unscoped().Delete(wf)
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": err.Error()})
		return
	}

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

// HandleCancel 取消工作流
// @Summary      取消工作流
// @Description  取消运行中的工作流（通知引擎中止后续步骤）并写审计日志；需要 workflows:run 权限
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Param        id  path  int  true  "工作流 ID"  example(1)
// @Success      200  {object}  SuccessResponse
// @Failure      400  {object}  ErrorResponse "id 非数字或工作流不可取消"
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /workflows/{id}/cancel [post]
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

// HandleGetWorkers 工作流 worker 列表
// @Summary      工作流 worker 列表
// @Description  汇总步骤状态中按 workerId 去重的 worker（agent）信息；任何登录用户可读
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Param        id  path  int  true  "工作流 ID"  example(1)
// @Success      200  {object}  WorkerListResponse
// @Failure      400  {object}  ErrorResponse "id 非数字"
// @Failure      401  {object}  ErrorResponse
// @Router       /workflows/{id}/workers [get]
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

// HandleGetStepStatus 单步实时状态
// @Summary      工作流步骤状态
// @Description  合并 DB 记录与引擎内存快照，返回单步的实时执行状态（status/message/workerId/起止时间）；任何登录用户可读
// @Tags         workflows
// @Produce      json
// @Security     BearerAuth
// @Param        id      path  int     true  "工作流 ID"   example(1)
// @Param        stepId  path  string  true  "步骤 ID"     example(s1)
// @Success      200  {object}  StepStatusResponse
// @Failure      400  {object}  ErrorResponse "id 非数字"
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse "步骤状态记录不存在"
// @Router       /workflows/{id}/steps/{stepId}/status [get]
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
		if state, ok := execution.StepSnapshot(stepID); ok {
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
