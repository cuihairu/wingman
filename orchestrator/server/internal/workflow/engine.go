package workflow

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"maps"
	"reflect"
	"strconv"
	"strings"
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

// reserveAgent 增加 agent 在执行步骤计数
func (e *Engine) reserveAgent(id string) {
	e.mu.Lock()
	e.inflight[id]++
	e.mu.Unlock()
}

// releaseAgent 减少 agent 在执行步骤计数
func (e *Engine) releaseAgent(id string) {
	e.mu.Lock()
	if e.inflight[id] > 0 {
		e.inflight[id]--
	}
	e.mu.Unlock()
}

func (e *Engine) inflightFor(id string) int {
	e.mu.RLock()
	defer e.mu.RUnlock()
	return e.inflight[id]
}

// selectAgent 选择执行步骤的 agent。
// 显式指定 workers 时，在可用 worker 中选负载最低的；否则在所有在线 agent 中选负载最低的。
// 返回 (conn, agentID)。无可用 agent 时返回错误。
func (e *Engine) selectAgent(preferred []string) (agent.AgentConn, string, error) {
	agents := e.registry.List()

	var candidates []*agent.AgentInfo
	if len(preferred) > 0 {
		want := make(map[string]bool, len(preferred))
		for _, w := range preferred {
			want[w] = true
		}
		for _, a := range agents {
			if want[a.AgentID] && a.Client != nil && a.Status != agent.StatusOffline {
				candidates = append(candidates, a)
			}
		}
	} else {
		for _, a := range agents {
			if a.Client != nil && a.Status != agent.StatusOffline {
				candidates = append(candidates, a)
			}
		}
	}
	if len(candidates) == 0 {
		if len(preferred) > 0 {
			return nil, "", fmt.Errorf("agent not connected: %v", preferred)
		}
		return nil, "", fmt.Errorf("no available agent")
	}

	// 选负载最低的；并列时取列表顺序靠前的（稳定）
	best := candidates[0]
	bestLoad := e.inflightFor(best.AgentID)
	for _, a := range candidates[1:] {
		load := e.inflightFor(a.AgentID)
		if load < bestLoad {
			best = a
			bestLoad = load
		}
	}
	return best.Client, best.AgentID, nil
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

		// 规范化步骤类型（默认 script）
		stepType := steps[i].Type
		if stepType == "" {
			stepType = "script"
			steps[i].Type = stepType
		}
		switch stepType {
		case "script":
			// 验证脚本路径有效且在 scripts 目录内
			scriptPath, err := e.store.Resolve(steps[i].Script)
			if err != nil {
				return fmt.Errorf("step %s: invalid script path %q: %w", steps[i].ID, steps[i].Script, err)
			}
			// 将解析后的绝对路径存回 slice 元素（供 executeStep 使用）
			steps[i].Script = scriptPath
		case "wait":
			// wait 步骤无需脚本；时长取自 TimeoutSeconds 或 parameters.seconds
		case "condition":
			if err := validateConditionStep(steps[i]); err != nil {
				return err
			}
		case "screenshot":
			// screenshot 步骤无需脚本；通过 runtime 远程命令 screenshot.capture 执行。
		default:
			return fmt.Errorf("step %s: unknown step type %q (supported: script, wait, condition, screenshot)", steps[i].ID, stepType)
		}

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

	// 按步骤类型分发
	stepType := step.Type
	if stepType == "" {
		stepType = "script"
	}
	if stepType == "wait" {
		return e.executeWaitStep(ctx, exec, step, ss)
	}
	if stepType == "condition" {
		return e.executeConditionStep(ctx, exec, step, ss)
	}
	if stepType == "screenshot" {
		return e.executeScreenshotStep(ctx, exec, step, ss)
	}

	// script 步骤：选择 worker（负载均衡）
	conn, workerID, err := e.selectAgent(step.Workers)
	if err != nil {
		return e.failStep(ss, exec, step, err.Error())
	}
	e.reserveAgent(workerID)
	defer e.releaseAgent(workerID)

	// 超时
	timeout := time.Duration(step.TimeoutSeconds) * time.Second
	if timeout == 0 {
		timeout = 5 * time.Minute
	}

	// 重试策略
	maxAttempts := step.MaxRetries + 1
	if maxAttempts < 1 {
		maxAttempts = 1
	}
	backoff := time.Duration(step.RetryBackoffSeconds) * time.Second
	if backoff <= 0 {
		backoff = 2 * time.Second
	}

	var lastErr error
	for attempt := 1; attempt <= maxAttempts; attempt++ {
		// 进入新一轮前若已取消，标记 cancelled 并返回（区别于失败）
		if err := ctx.Err(); err != nil {
			e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
			ss.Status = "cancelled"
			return err
		}

		runErr := e.runScriptOnce(ctx, conn, step, timeout)
		if runErr == nil {
			lastErr = nil
			break
		}
		lastErr = runErr

		// 执行期间被取消：不重试，标记 cancelled
		if ctx.Err() != nil {
			e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
			ss.Status = "cancelled"
			return ctx.Err()
		}

		// 超时或脚本失败时尝试重试
		if attempt < maxAttempts {
			msg := fmt.Sprintf("attempt %d/%d failed: %s; retrying", attempt, maxAttempts, runErr.Error())
			e.db.Model(ss).Updates(map[string]any{"message": msg})
			ss.Message = msg
			e.broadcastStepProgress(exec.Workflow.ID, step.ID, "retry", msg)
			log.Printf("[WorkflowEngine] Step %s %s", step.ID, msg)

			select {
			case <-ctx.Done():
				e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
				ss.Status = "cancelled"
				return ctx.Err()
			case <-time.After(backoff):
			}
			backoff *= 2 // 指数退避
		}
	}

	if lastErr != nil {
		return e.failStep(ss, exec, step, lastErr.Error())
	}

	// 标记完成
	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "completed", "end_time": endTime, "worker_id": workerID})
	ss.Status = "completed"
	ss.WorkerID = workerID

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", "")
	return nil
}

func validateConditionStep(step models.WorkflowStep) error {
	if len(step.Parameters) == 0 {
		return fmt.Errorf("step %s: condition step requires parameters", step.ID)
	}
	if _, ok := step.Parameters["expression"]; ok {
		return nil
	}
	if _, ok := step.Parameters["value"]; ok {
		return nil
	}
	if _, ok := step.Parameters["actual"]; ok {
		return nil
	}
	return fmt.Errorf("step %s: condition step requires parameters.value (or actual/expression)", step.ID)
}

// runScriptOnce 向 agent 发起一次脚本执行，遵循步骤超时与工作流取消。
func (e *Engine) runScriptOnce(ctx context.Context, conn agent.AgentConn, step models.WorkflowStep, timeout time.Duration) error {
	done := make(chan error, 1)
	go func() {
		data := map[string]any{
			"path":    step.Script,
			"timeout": int(timeout.Milliseconds()),
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
				return
			}
			done <- fmt.Errorf("script failed")
			return
		}
		done <- nil
	}()

	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-time.After(timeout):
		return fmt.Errorf("step %s timed out after %v", step.ID, timeout)
	case err := <-done:
		return err
	}
}

// executeWaitStep 执行 wait 步骤：休眠指定时长，不需要 agent。
// 时长取自 TimeoutSeconds；为 0 时取 parameters.seconds；均无则默认 1 秒。
func (e *Engine) executeWaitStep(ctx context.Context, exec *Execution, step models.WorkflowStep, ss *models.StepStatus) error {
	seconds := step.TimeoutSeconds
	if seconds <= 0 {
		if v, ok := step.Parameters["seconds"]; ok {
			if fv, ok := toFloat(v); ok && fv > 0 {
				seconds = int(fv)
			}
		}
	}
	if seconds <= 0 {
		seconds = 1
	}

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "running", fmt.Sprintf("waiting %ds", seconds))

	select {
	case <-ctx.Done():
		e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
		ss.Status = "cancelled"
		return ctx.Err()
	case <-time.After(time.Duration(seconds) * time.Second):
	}

	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "completed", "end_time": endTime})
	ss.Status = "completed"
	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", "")
	return nil
}

// executeConditionStep evaluates a small declarative condition without using an agent.
// Parameters:
//   - value or actual: left-hand value
//   - operator/op: truthy, falsy, eq, ne, gt, gte, lt, lte, contains, not_contains
//   - expected: right-hand value for binary operators
//   - expression: optional bool shortcut
func (e *Engine) executeConditionStep(ctx context.Context, exec *Execution, step models.WorkflowStep, ss *models.StepStatus) error {
	if err := ctx.Err(); err != nil {
		e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
		ss.Status = "cancelled"
		return err
	}

	ok, message, err := evaluateCondition(step.Parameters)
	if err != nil {
		return e.failStep(ss, exec, step, err.Error())
	}
	if !ok {
		if message == "" {
			message = "condition evaluated false"
		}
		return e.failStep(ss, exec, step, message)
	}

	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{"status": "completed", "end_time": endTime, "message": message})
	ss.Status = "completed"
	ss.Message = message
	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", message)
	return nil
}

func evaluateCondition(params map[string]any) (bool, string, error) {
	if v, ok := params["expression"]; ok {
		result, ok := toBool(v)
		if !ok {
			return false, "", fmt.Errorf("condition expression must be boolean-compatible")
		}
		if result {
			return true, "condition matched", nil
		}
		return false, "condition expression is false", nil
	}

	actual, ok := params["value"]
	if !ok {
		actual = params["actual"]
	}
	operator := "truthy"
	if op, ok := params["operator"].(string); ok && strings.TrimSpace(op) != "" {
		operator = op
	} else if op, ok := params["op"].(string); ok && strings.TrimSpace(op) != "" {
		operator = op
	}
	operator = normalizeConditionOperator(operator)
	expected, hasExpected := params["expected"]

	var matched bool
	var err error
	switch operator {
	case "truthy":
		matched, ok = toBool(actual)
		if !ok {
			return false, "", fmt.Errorf("condition value is not boolean-compatible")
		}
	case "falsy":
		matched, ok = toBool(actual)
		if !ok {
			return false, "", fmt.Errorf("condition value is not boolean-compatible")
		}
		matched = !matched
	case "eq":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator eq requires expected")
		}
		matched = valuesEqual(actual, expected)
	case "ne":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator ne requires expected")
		}
		matched = !valuesEqual(actual, expected)
	case "gt", "gte", "lt", "lte":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator %s requires expected", operator)
		}
		matched, err = compareNumbers(actual, expected, operator)
		if err != nil {
			return false, "", err
		}
	case "contains", "not_contains":
		if !hasExpected {
			return false, "", fmt.Errorf("condition operator %s requires expected", operator)
		}
		matched = containsValue(actual, expected)
		if operator == "not_contains" {
			matched = !matched
		}
	default:
		return false, "", fmt.Errorf("unknown condition operator %q", operator)
	}

	if matched {
		return true, fmt.Sprintf("condition matched: %s", operator), nil
	}
	return false, fmt.Sprintf("condition failed: %s", operator), nil
}

func normalizeConditionOperator(op string) string {
	switch strings.ToLower(strings.TrimSpace(op)) {
	case "", "true", "truthy", "is_true":
		return "truthy"
	case "false", "falsy", "is_false":
		return "falsy"
	case "=", "==", "eq", "equals":
		return "eq"
	case "!=", "<>", "ne", "not_equals":
		return "ne"
	case ">", "gt":
		return "gt"
	case ">=", "gte":
		return "gte"
	case "<", "lt":
		return "lt"
	case "<=", "lte":
		return "lte"
	case "contains", "in":
		return "contains"
	case "not_contains", "not-contains", "not in", "not_in":
		return "not_contains"
	default:
		return strings.ToLower(strings.TrimSpace(op))
	}
}

func valuesEqual(a, b any) bool {
	if af, ok := toFloat(a); ok {
		if bf, ok := toFloat(b); ok {
			return af == bf
		}
	}
	if ab, ok := toBool(a); ok {
		if bb, ok := toBool(b); ok {
			return ab == bb
		}
	}
	return fmt.Sprint(a) == fmt.Sprint(b)
}

func compareNumbers(actual, expected any, operator string) (bool, error) {
	left, ok := toFloat(actual)
	if !ok {
		return false, fmt.Errorf("condition actual value %q is not numeric", fmt.Sprint(actual))
	}
	right, ok := toFloat(expected)
	if !ok {
		return false, fmt.Errorf("condition expected value %q is not numeric", fmt.Sprint(expected))
	}
	switch operator {
	case "gt":
		return left > right, nil
	case "gte":
		return left >= right, nil
	case "lt":
		return left < right, nil
	case "lte":
		return left <= right, nil
	default:
		return false, fmt.Errorf("unknown numeric operator %q", operator)
	}
}

func containsValue(actual, expected any) bool {
	if actual == nil {
		return false
	}
	if s, ok := actual.(string); ok {
		return strings.Contains(s, fmt.Sprint(expected))
	}
	value := reflect.ValueOf(actual)
	if value.Kind() == reflect.Slice || value.Kind() == reflect.Array {
		for i := 0; i < value.Len(); i++ {
			if valuesEqual(value.Index(i).Interface(), expected) {
				return true
			}
		}
		return false
	}
	return strings.Contains(fmt.Sprint(actual), fmt.Sprint(expected))
}

func toBool(v any) (bool, bool) {
	switch b := v.(type) {
	case bool:
		return b, true
	case string:
		switch strings.ToLower(strings.TrimSpace(b)) {
		case "true", "1", "yes", "y", "on":
			return true, true
		case "false", "0", "no", "n", "off", "":
			return false, true
		}
	case float64:
		return b != 0, true
	case float32:
		return b != 0, true
	case int:
		return b != 0, true
	case int64:
		return b != 0, true
	case json.Number:
		f, err := b.Float64()
		return f != 0, err == nil
	}
	return false, false
}

// executeScreenshotStep asks a selected agent to capture a screenshot and broadcasts
// the returned image through the existing Dashboard screenshot event channel.
func (e *Engine) executeScreenshotStep(ctx context.Context, exec *Execution, step models.WorkflowStep, ss *models.StepStatus) error {
	conn, workerID, err := e.selectAgent(step.Workers)
	if err != nil {
		return e.failStep(ss, exec, step, err.Error())
	}
	e.reserveAgent(workerID)
	defer e.releaseAgent(workerID)

	timeout := time.Duration(step.TimeoutSeconds) * time.Second
	if timeout == 0 {
		timeout = 30 * time.Second
	}

	data := map[string]any{"timeout": int(timeout.Milliseconds())}
	maps.Copy(data, step.Parameters)

	done := make(chan map[string]any, 1)
	errCh := make(chan error, 1)
	go func() {
		resp, err := conn.SendCommandWithTimeout("screenshot.capture", data, timeout)
		if err != nil {
			errCh <- err
			return
		}
		done <- resp
	}()

	var resp map[string]any
	select {
	case <-ctx.Done():
		e.db.Model(ss).Updates(map[string]any{"status": "cancelled"})
		ss.Status = "cancelled"
		return ctx.Err()
	case <-time.After(timeout):
		return e.failStep(ss, exec, step, fmt.Sprintf("screenshot step timed out after %v", timeout))
	case err := <-errCh:
		return e.failStep(ss, exec, step, err.Error())
	case resp = <-done:
	}

	payload, err := screenshotPayload(resp)
	if err != nil {
		return e.failStep(ss, exec, step, err.Error())
	}
	payload["workflowId"] = exec.Workflow.ID
	payload["stepId"] = step.ID
	payload["workerId"] = workerID
	e.hub.BroadcastEvent("screenshot", payload)

	message := screenshotMessage(payload)
	endTime := time.Now()
	e.db.Model(ss).Updates(map[string]any{
		"status":    "completed",
		"end_time":  endTime,
		"worker_id": workerID,
		"message":   message,
	})
	ss.Status = "completed"
	ss.WorkerID = workerID
	ss.Message = message
	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", message)
	return nil
}

func screenshotPayload(resp map[string]any) (map[string]any, error) {
	if resp == nil {
		return nil, fmt.Errorf("empty screenshot response")
	}
	if success, ok := resp["success"].(bool); ok && !success {
		return nil, fmt.Errorf("screenshot failed: %s", responseError(resp))
	}
	data, _ := resp["data"].(map[string]any)
	if data == nil {
		data = resp
	}
	if success, ok := data["success"].(bool); ok && !success {
		return nil, fmt.Errorf("screenshot failed: %s", responseError(data))
	}
	image, ok := data["image"].(string)
	if !ok || image == "" {
		return nil, fmt.Errorf("screenshot response missing image")
	}
	payload := make(map[string]any, len(data))
	maps.Copy(payload, data)
	return payload, nil
}

func responseError(resp map[string]any) string {
	if msg, ok := resp["error"].(string); ok && msg != "" {
		return msg
	}
	if msg, ok := resp["message"].(string); ok && msg != "" {
		return msg
	}
	return "unknown error"
}

func screenshotMessage(payload map[string]any) string {
	width, _ := numberAsInt(payload["width"])
	height, _ := numberAsInt(payload["height"])
	if width > 0 && height > 0 {
		return fmt.Sprintf("screenshot captured: %dx%d", width, height)
	}
	return "screenshot captured"
}

func numberAsInt(v any) (int, bool) {
	if f, ok := toFloat(v); ok {
		return int(f), true
	}
	if s, ok := v.(string); ok {
		n, err := strconv.Atoi(strings.TrimSpace(s))
		return n, err == nil
	}
	return 0, false
}

// toFloat 容错地把任意 JSON 数值转为 float64
func toFloat(v any) (float64, bool) {
	switch n := v.(type) {
	case float64:
		return n, true
	case float32:
		return float64(n), true
	case int:
		return float64(n), true
	case int64:
		return float64(n), true
	case json.Number:
		f, err := n.Float64()
		return f, err == nil
	}
	return 0, false
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
