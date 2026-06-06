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
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/gorm"
)

// Engine 工作流引擎
type Engine struct {
	db       *gorm.DB
	registry *agent.Registry
	hub      *ws.Hub
	running  map[uint]*Execution
	mu       sync.RWMutex
}

// Execution 正在执行的工作流
type Execution struct {
	Workflow  *models.Workflow
	Steps     []models.WorkflowStep
	StepState map[string]*models.StepStatus
	cancel    context.CancelFunc
}

// NewEngine 创建工作流引擎
func NewEngine(db *gorm.DB, registry *agent.Registry, hub *ws.Hub) *Engine {
	return &Engine{
		db:       db,
		registry: registry,
		hub:      hub,
		running:  make(map[uint]*Execution),
	}
}

// Submit 提交工作流执行
func (e *Engine) Submit(workflow *models.Workflow) error {
	steps := workflow.GetSteps()
	if len(steps) == 0 {
		return fmt.Errorf("workflow has no steps")
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
		exec.cancel()
		delete(e.running, workflowID)
	}
	e.mu.Unlock()

	if !ok {
		return fmt.Errorf("workflow not running")
	}

	// 更新状态
	now := time.Now()
	e.db.Model(&models.Workflow{}).Where("id = ?", workflowID).Updates(map[string]any{
		"status":   "cancelled",
		"end_time": now,
	})
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

// execute 执行工作流（DAG 调度）
func (e *Engine) execute(ctx context.Context, exec *Execution) {
	defer func() {
		e.mu.Lock()
		delete(e.running, exec.Workflow.ID)
		e.mu.Unlock()
	}()

	// 拓扑排序执行
	completed := make(map[string]bool)
	failed := false

	for {
		if failed {
			break
		}

		select {
		case <-ctx.Done():
			return
		default:
		}

		// 找到可执行的步骤（所有依赖已完成）
		ready := e.findReadySteps(exec, completed)
		if len(ready) == 0 {
			// 检查是否全部完成
			if len(completed) == len(exec.Steps) {
				break
			}
			// 还有步骤未完成但无可执行步骤 → 死锁或等待
			allDone := true
			for _, step := range exec.Steps {
				if !completed[step.ID] {
					allDone = false
					// 检查是否为失败步骤的下游
					ss := exec.StepState[step.ID]
					if ss != nil && ss.Status == "failed" {
						// 已标记失败，跳过
						continue
					}
				}
			}
			if allDone {
				break
			}
			// 等待一会再检查
			time.Sleep(500 * time.Millisecond)
			continue
		}

		// 并发执行就绪步骤
		var wg sync.WaitGroup
		var mu sync.Mutex
		for _, step := range ready {
			wg.Add(1)
			go func(s models.WorkflowStep) {
				defer wg.Done()
				err := e.executeStep(ctx, exec, s)
				mu.Lock()
				if err != nil {
					failed = true
				}
				completed[s.ID] = true
				mu.Unlock()
			}(step)
		}
		wg.Wait()
	}

	// 确定最终状态
	finalStatus := "completed"
	for _, ss := range exec.StepState {
		if ss.Status == "failed" {
			finalStatus = "failed"
			break
		}
	}

	now := time.Now()
	e.db.Model(&models.Workflow{}).Where("id = ?", exec.Workflow.ID).Updates(map[string]any{
		"status":   finalStatus,
		"end_time": now,
	})

	e.hub.BroadcastEvent("workflow", map[string]any{
		"event": "status_changed",
		"data": map[string]any{
			"workflowId": exec.Workflow.ID,
			"status":     finalStatus,
		},
	})

	log.Printf("[WorkflowEngine] Workflow %d completed with status: %s", exec.Workflow.ID, finalStatus)
}

// findReadySteps 找到所有依赖已满足的就绪步骤
func (e *Engine) findReadySteps(exec *Execution, completed map[string]bool) []models.WorkflowStep {
	var ready []models.WorkflowStep
	for _, step := range exec.Steps {
		if completed[step.ID] {
			continue
		}
		ss := exec.StepState[step.ID]
		if ss != nil && (ss.Status == "running" || ss.Status == "completed" || ss.Status == "failed") {
			continue
		}

		allDepsDone := true
		for _, dep := range step.DependsOn {
			if !completed[dep] {
				allDepsDone = false
				break
			}
			// 检查依赖步骤是否失败
			if depSS, ok := exec.StepState[dep]; ok && depSS.Status == "failed" {
				// 依赖失败 → 跳过此步骤
				e.db.Model(ss).Updates(map[string]any{"status": "skipped", "message": "dependency failed"})
				if ss != nil {
					ss.Status = "skipped"
				}
				completed[step.ID] = true
				allDepsDone = false
				break
			}
		}
		if allDepsDone {
			ready = append(ready, step)
		}
	}
	return ready
}

// executeStep 执行单个步骤
func (e *Engine) executeStep(ctx context.Context, exec *Execution, step models.WorkflowStep) error {
	ss := exec.StepState[step.ID]
	if ss == nil {
		return fmt.Errorf("step state not found: %s", step.ID)
	}

	// 标记为运行中
	now := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "running", "start_time": now})
	ss.Status = "running"

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "running", "")

	// 选择 worker
	workerID := ""
	if len(step.Workers) > 0 {
		workerID = step.Workers[0]
	}

	// 获取 agent 连接
	var conn agent.AgentConn
	if workerID != "" {
		var ok bool
		conn, ok = e.registry.GetClient(workerID)
		if !ok {
			return e.failStep(ss, exec, step, "agent not connected: "+workerID)
		}
	} else {
		// 选择第一个可用的 agent
		agents := e.registry.List()
		for _, a := range agents {
			if a.Client != nil && a.Status != agent.StatusOffline {
				conn = a.Client
				workerID = a.AgentID
				break
			}
		}
		if conn == nil {
			return e.failStep(ss, exec, step, "no available agent")
		}
	}

	// 设置超时
	timeout := time.Duration(step.TimeoutSeconds) * time.Second
	if timeout == 0 {
		timeout = 5 * time.Minute
	}

	done := make(chan error, 1)
	go func() {
		// 合并参数到脚本路径
		data := map[string]any{
			"path": step.Script,
		}
		for k, v := range step.Parameters {
			data[k] = v
		}

		resp, err := conn.SendCommand("run_script", data)
		if err != nil {
			done <- err
			return
		}
		if success, ok := resp["success"].(bool); ok && !success {
			if msg, ok := resp["message"].(string); ok {
				done <- fmt.Errorf("script failed: %s", msg)
			} else {
				done <- fmt.Errorf("script failed")
			}
			return
		}
		done <- nil
	}()

	select {
	case <-ctx.Done():
		e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
		ss.Status = "cancelled"
		return ctx.Err()
	case err := <-done:
		if err != nil {
			return e.failStep(ss, exec, step, err.Error())
		}
	}

	// 标记完成
	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "completed", "end_time": endTime, "worker_id": workerID})
	ss.Status = "completed"
	ss.WorkerID = workerID

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", "")
	return nil
}

// failStep 标记步骤失败
func (e *Engine) failStep(ss *models.StepStatus, exec *Execution, step models.WorkflowStep, msg string) error {
	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "failed", "end_time": endTime, "message": msg})
	ss.Status = "failed"
	ss.Message = msg

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
