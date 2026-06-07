package workflow

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"maps"
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
}

// Execution 正在执行的工作流
type Execution struct {
	Workflow  *models.Workflow
	Steps     []models.WorkflowStep
	StepState map[string]*models.StepStatus
	cancel    context.CancelFunc
}

// NewEngine 创建工作流引擎
func NewEngine(db *gorm.DB, registry *agent.Registry, hub *ws.Hub, scriptsDir string) *Engine {
	return &Engine{
		db:       db,
		registry: registry,
		hub:      hub,
		store:    scripts.NewStore(scriptsDir),
		running:  make(map[uint]*Execution),
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

// detectCycle 检测 DAG 是否存在环。返回错误（含环路径）或 nil。
func (e *Engine) detectCycle(steps []models.WorkflowStep) error {
	// 构建邻接表: stepID -> []dependentStepIDs
	graph := make(map[string][]string)
	stepIDs := make(map[string]bool)
	for _, step := range steps {
		stepIDs[step.ID] = true
		for _, dep := range step.DependsOn {
			graph[dep] = append(graph[dep], step.ID)
		}
	}

	// 每个节点访问状态: 0=未访问, 1=访问中(当前递归栈), 2=已完成
	visited := make(map[string]int)
	path := make([]string, 0)

	var dfs func(string) error
	dfs = func(node string) error {
		visited[node] = 1 // 标记为访问中
		path = append(path, node)

		for _, neighbor := range graph[node] {
			switch visited[neighbor] {
			case 0:
				if err := dfs(neighbor); err != nil {
					return err
				}
			case 1:
				// 发现环
				cycleStart := -1
				for i, p := range path {
					if p == neighbor {
						cycleStart = i
						break
					}
				}
				cyclePath := append(path[cycleStart:], neighbor)
				return fmt.Errorf("dependency cycle detected: %v", cyclePath)
			case 2:
				// 已完成其他路径，无需处理
			}
		}

		visited[node] = 2 // 标记为已完成
		path = path[:len(path)-1]
		return nil
	}

	// 从所有节点开始 DFS（处理孤立节点）
	for id := range stepIDs {
		if visited[id] == 0 {
			if err := dfs(id); err != nil {
				return err
			}
		}
	}
	return nil
}

// validateSteps 验证步骤配置
func (e *Engine) validateSteps(steps []models.WorkflowStep) error {
	seenIDs := make(map[string]bool)
	stepIDs := make(map[string]bool)

	for i := range steps {
		// 检查 ID 非空
		if steps[i].ID == "" {
			return fmt.Errorf("step has empty ID")
		}

		// 检查 ID 唯一
		if seenIDs[steps[i].ID] {
			return fmt.Errorf("duplicate step ID: %s", steps[i].ID)
		}
		seenIDs[steps[i].ID] = true
		stepIDs[steps[i].ID] = true

		// 验证脚本路径有效且在 scripts 目录内
		scriptPath, err := e.store.Resolve(steps[i].Script)
		if err != nil {
			return fmt.Errorf("step %s: invalid script path %q: %w", steps[i].ID, steps[i].Script, err)
		}
		// 将解析后的绝对路径存回 slice 元素（供 executeStep 使用）
		steps[i].Script = scriptPath

		// 检查依赖存在
		for _, dep := range steps[i].DependsOn {
			if !seenIDs[dep] {
				// 依赖可能在此步骤之后定义，暂时允许
				// 会在 detectCycle 中捕获环
			}
		}
	}

	// 再次检查依赖是否都指向有效步骤
	for _, step := range steps {
		for _, dep := range step.DependsOn {
			if !stepIDs[dep] {
				return fmt.Errorf("step %s: dependency %q does not exist", step.ID, dep)
			}
		}
	}

	return nil
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
			"path":    step.Script,
			"timeout": int(timeout.Milliseconds()), // Pass timeout to runtime for enforcement
		}
		maps.Copy(data, step.Parameters)

		resp, err := conn.SendCommandWithTimeout("run_script", data, timeout)
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
	case <-time.After(timeout):
		e.db.Model(ss).Updates(map[string]any{"status": "failed", "end_time": time.Now(), "message": "timeout"})
		ss.Status = "failed"
		ss.Message = "timeout"
		return fmt.Errorf("step %s timed out after %v", step.ID, timeout)
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
