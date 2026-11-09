package workflow

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/gorm"
)

// Engine 工作流引擎
type Engine struct {
	db       *gorm.DB
	registry *agent.Registry
	hub      *ws.Hub
	store    scripts.Store
	running  map[uint]*Execution
	mu       sync.RWMutex
	// inflight 跟踪每个 agent 当前在执行的步骤数，用于负载均衡选择。
	inflight map[string]int
}

// Execution 正在执行的工作流
type Execution struct {
	Workflow  *models.Workflow
	Steps     []models.WorkflowStep
	StepState map[string]*models.StepStatus
	cancel    context.CancelFunc

	// mu 保护 StepState 中各步骤状态的并发读写：
	// 调度 goroutine（execute/findReadySteps）与 HTTP 查询（GetStepStateSnapshot）
	// 会与步骤执行 goroutine 的状态更新并发访问。
	mu sync.RWMutex
}

// setStepState 在写锁下更新指定步骤的状态字段。
func (exec *Execution) setStepState(stepID string, fn func(*models.StepStatus)) {
	exec.mu.Lock()
	defer exec.mu.Unlock()
	if ss, ok := exec.StepState[stepID]; ok {
		fn(ss)
	}
}

// stepSnapshot 在读锁下返回指定步骤状态的值拷贝。
func (exec *Execution) StepSnapshot(stepID string) (models.StepStatus, bool) {
	exec.mu.RLock()
	defer exec.mu.RUnlock()
	ss, ok := exec.StepState[stepID]
	if !ok {
		return models.StepStatus{}, false
	}
	return *ss, true
}

// stepStatusIn 判断指定步骤状态是否命中给定集合（读锁）。
func (exec *Execution) stepStatusIn(stepID string, statuses ...string) bool {
	exec.mu.RLock()
	defer exec.mu.RUnlock()
	ss, ok := exec.StepState[stepID]
	if !ok {
		return false
	}
	for _, s := range statuses {
		if ss.Status == s {
			return true
		}
	}
	return false
}

// anyStepStatus 判断是否存在处于指定状态的步骤（读锁）。
func (exec *Execution) anyStepStatus(status string) bool {
	exec.mu.RLock()
	defer exec.mu.RUnlock()
	for _, ss := range exec.StepState {
		if ss.Status == status {
			return true
		}
	}
	return false
}

// NewEngine 创建工作流引擎
func NewEngine(db *gorm.DB, registry *agent.Registry, hub *ws.Hub, scriptsDir string) *Engine {
	return &Engine{
		db:       db,
		registry: registry,
		hub:      hub,
		store:    scripts.NewStore(scriptsDir),
		running:  make(map[uint]*Execution),
		inflight: make(map[string]int),
	}
}

// Submit 提交工作流执行
func (e *Engine) Submit(workflow *models.Workflow) error {
	steps := workflow.GetSteps()
	if len(steps) == 0 {
		return fmt.Errorf("workflow has no steps")
	}

	// 验证步骤
	if err := e.validateSteps(steps); err != nil {
		return fmt.Errorf("invalid workflow steps: %w", err)
	}

	// 检测 DAG 环
	if err := e.detectCycle(steps); err != nil {
		return err
	}

	// 更新状态为 running
	now := time.Now()
	workflow.Status = "running"
	workflow.StartTime = &now
	e.db.Save(workflow)

	// 创建步骤状态
	stepState := make(map[string]*models.StepStatus)
	for _, step := range steps {
		ss := &models.StepStatus{
			WorkflowID: workflow.ID,
			StepID:     step.ID,
			Name:       step.Name,
			Status:     "pending",
		}
		e.db.Create(ss)
		stepState[step.ID] = ss
	}

	ctx, cancel := context.WithCancel(context.Background())
	exec := &Execution{
		Workflow:  workflow,
		Steps:     steps,
		StepState: stepState,
		cancel:    cancel,
	}

	e.mu.Lock()
	e.running[workflow.ID] = exec
	e.mu.Unlock()

	// 广播工作流提交事件
	e.hub.BroadcastEvent("workflow", map[string]any{
		"event": "submitted",
		"data":  e.workflowToJSON(workflow, stepState),
	})

	// 异步执行
	go e.execute(ctx, exec)

	return nil
}

// Cancel 取消工作流
func (e *Engine) Cancel(workflowID uint) error {
	e.mu.Lock()
	exec, ok := e.running[workflowID]
	if ok {
		delete(e.running, workflowID)
	}
	e.mu.Unlock()

	if !ok {
		return fmt.Errorf("workflow not running")
	}

	// 终态收口（条件写 + 先落库后惊醒）：只有仍处于 running 的行才允许改写为
	// cancelled，防止覆盖 execute 已写的 completed/failed；并且必须在
	// exec.cancel() 之前落库——runner 被 ctx 惊醒后会立刻冲向自己的终态回写，
	// 先惊醒再落库就把两侧变成纯赛跑（-race -count=50 实测 runner 会带着误算的
	// completed 抢先落库，Cancel 反而向错误终态让位、对用户回 400）。先落库则
	// 取消必然先安装终态，runner 的回写只会让位，两种交错都不再有输赢问题。
	now := time.Now()
	res := e.db.Model(&models.Workflow{}).
		Where("id = ? AND status = ?", workflowID, "running").
		Updates(map[string]any{"status": "cancelled", "end_time": now})
	if res.Error != nil {
		return res.Error
	}
	if res.RowsAffected == 0 {
		// 极窄窗口：落库之前 execute 已先写终态——工作流已经跑完，无可取消。
		// 按「不在运行」对齐既有契约（map 里还挂着只是因为 execute 的 defer
		// 摘除晚于终态回写）。此时不 cancel ctx、不标 skipped、不广播：那三步
		// 的前提「取消生效」都不成立，且 runner 已在收尾、无需再停。
		log.Printf("[WorkflowEngine] Cancel %d: 工作流已先到达终态，取消让位", workflowID)
		return fmt.Errorf("workflow not running")
	}

	// 终态已安装，此刻才停 runner：步骤内的 select 醒来把 running 步骤标
	// cancelled，随后 runner 的终态回写向已落库的 cancelled 让位。
	exec.cancel()

	e.db.Model(&models.StepStatus{}).Where("workflow_id = ? AND status = ?", workflowID, "pending").
		Update("status", "skipped")

	e.hub.BroadcastEvent("workflow", map[string]any{
		"event": "status_changed",
		"data": map[string]any{
			"workflowId": workflowID,
			"status":     "cancelled",
		},
	})

	return nil
}

// GetExecution 获取执行中的工作流
func (e *Engine) GetExecution(workflowID uint) (*Execution, bool) {
	e.mu.RLock()
	defer e.mu.RUnlock()
	exec, ok := e.running[workflowID]
	return exec, ok
}

// ListWorkflows 列出所有工作流
func (e *Engine) ListWorkflows() ([]models.Workflow, error) {
	var workflows []models.Workflow
	err := e.db.Order("created_at DESC").Find(&workflows).Error
	return workflows, err
}

// GetWorkflow 获取单个工作流（含步骤状态）
func (e *Engine) GetWorkflow(id uint) (*models.Workflow, []models.StepStatus, error) {
	var workflow models.Workflow
	if err := e.db.First(&workflow, id).Error; err != nil {
		return nil, nil, err
	}

	var stepStatuses []models.StepStatus
	e.db.Where("workflow_id = ?", id).Order("step_id").Find(&stepStatuses)

	return &workflow, stepStatuses, nil
}

// failStep 标记步骤失败
func (e *Engine) failStep(ss *models.StepStatus, exec *Execution, step models.WorkflowStep, msg string) error {
	endTime := time.Now()
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "failed", "end_time": endTime, "message": msg})
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "failed"
		s.Message = msg
		s.EndTime = &endTime
	})

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "failed", msg)

	log.Printf("[WorkflowEngine] Step %s failed: %s", step.ID, msg)
	return fmt.Errorf("step %s failed: %s", step.ID, msg)
}

// broadcastStepProgress 广播步骤进度
func (e *Engine) broadcastStepProgress(workflowID uint, stepID, status, message string) {
	e.hub.BroadcastEvent("workflow", map[string]any{
		"event": "progress",
		"data": map[string]any{
			"workflowId": workflowID,
			"stepId":     stepID,
			"status":     status,
			"message":    message,
		},
	})
}

// workflowToJSON 工作流转前端格式
func (e *Engine) workflowToJSON(w *models.Workflow, stepState map[string]*models.StepStatus) map[string]any {
	steps := w.GetSteps()
	stepsJSON, _ := json.Marshal(steps)
	stepsData := []any{}
	json.Unmarshal(stepsJSON, &stepsData)

	stepStatusMap := map[string]any{}
	for id, ss := range stepState {
		stepStatusMap[id] = map[string]any{
			"stepId":    ss.StepID,
			"name":      ss.Name,
			"status":    ss.Status,
			"workerId":  ss.WorkerID,
			"message":   ss.Message,
			"startTime": ss.StartTime,
			"endTime":   ss.EndTime,
		}
	}

	return map[string]any{
		"id":          w.ID,
		"name":        w.Name,
		"description": w.Description,
		"status":      w.Status,
		"steps":       stepsData,
		"stepStatus":  stepStatusMap,
		"createdTime": w.CreatedAt.UnixMilli(),
		"startTime":   w.StartTime,
		"endTime":     w.EndTime,
	}
}
