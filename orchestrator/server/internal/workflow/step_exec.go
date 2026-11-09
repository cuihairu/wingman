package workflow

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"maps"
	"strconv"
	"strings"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/scripts"
)

// executeStep 执行单个步骤
func (e *Engine) executeStep(ctx context.Context, exec *Execution, step models.WorkflowStep) error {
	ss := exec.stepStateRef(step.ID)
	if ss == nil {
		return fmt.Errorf("step state not found: %s", step.ID)
	}

	// 标记为运行中
	now := time.Now()
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "running", "start_time": now})
	startTime := now
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "running"
		s.StartTime = &startTime
	})

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

	// script 步骤：选择 worker（先按 requires 过滤，再负载均衡）
	conn, workerID, err := e.selectAgent(step.Workers, step.Requires)
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
			e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
			exec.setStepState(step.ID, func(s *models.StepStatus) {
				s.Status = "cancelled"
			})
			return err
		}

		// Execution 记录（ADR: Execution as the Platform Core Object）：每轮
		// 下发对应一次执行，WorkflowID/StepID 挂载（StepStatus 为既有平行记录，
		// 收敛前保留）；终态由该轮 runScriptOnce 结果驱动。
		now := time.Now()
		stepExec := &models.Execution{
			ExecutionID: newExecutionID(),
			WorkflowID:  exec.Workflow.ID,
			StepID:      step.ID,
			AgentID:     workerID,
			ScriptPath:  step.Script,
			Status:      models.ExecutionRunning,
			StartedAt:   &now,
			TimeoutSec:  int(timeout.Seconds()),
			Artifacts:   "[]",
		}
		if err := e.db.Create(stepExec).Error; err != nil {
			log.Printf("[WorkflowEngine] Step %s: failed to persist execution record: %v", step.ID, err)
		}

		runErr := e.runScriptOnce(ctx, conn, step, timeout)
		// 落本轮执行终态：成功 / 取消 / 超时（私案哨兵） / 其余失败
		switch {
		case runErr == nil:
			e.finishStepExecution(stepExec.ID, models.ExecutionSucceeded, "")
		case ctx.Err() != nil:
			e.finishStepExecution(stepExec.ID, models.ExecutionCancelled, "")
		case errors.Is(runErr, errStepTimeout):
			e.finishStepExecution(stepExec.ID, models.ExecutionTimeout, runErr.Error())
		default:
			e.finishStepExecution(stepExec.ID, models.ExecutionFailed, runErr.Error())
		}

		if runErr == nil {
			lastErr = nil
			break
		}
		lastErr = runErr

		// 执行期间被取消：不重试，标记 cancelled
		if ctx.Err() != nil {
			e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
			exec.setStepState(step.ID, func(s *models.StepStatus) {
				s.Status = "cancelled"
			})
			return ctx.Err()
		}

		// 超时或脚本失败时尝试重试
		if attempt < maxAttempts {
			msg := fmt.Sprintf("attempt %d/%d failed: %s; retrying", attempt, maxAttempts, runErr.Error())
			e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"message": msg})
			exec.setStepState(step.ID, func(s *models.StepStatus) {
				s.Message = msg
			})
			e.broadcastStepProgress(exec.Workflow.ID, step.ID, "retry", msg)
			log.Printf("[WorkflowEngine] Step %s %s", step.ID, msg)

			select {
			case <-ctx.Done():
				e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
				exec.setStepState(step.ID, func(s *models.StepStatus) {
					s.Status = "cancelled"
				})
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
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "completed", "end_time": endTime, "worker_id": workerID})
	completedAt := endTime
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "completed"
		s.WorkerID = workerID
		s.EndTime = &completedAt
	})

	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", "")
	return nil
}

// stepStateRef 返回步骤状态的指针引用（仅用于该步骤执行 goroutine 内部的存在性检查；
// 并发读写状态字段必须使用 setStepState / stepSnapshot）。
func (exec *Execution) stepStateRef(stepID string) *models.StepStatus {
	exec.mu.RLock()
	defer exec.mu.RUnlock()
	return exec.StepState[stepID]
}

// updateStepStatusRow 按 workflow/step 条件更新步骤状态行，避免 GORM 反射读取并发中的结构体。
func (e *Engine) updateStepStatusRow(workflowID uint, stepID string, updates map[string]any) {
	e.db.Model(&models.StepStatus{}).
		Where("workflow_id = ? AND step_id = ?", workflowID, stepID).
		Updates(updates)
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

// errStepTimeout 脚本下发超时哨兵：runScriptOnce 的 select 超时分支 wrap 它，
// 步骤执行层据 errors.Is 区分 Execution 终态 timeout 与普通 failed。
var errStepTimeout = errors.New("script dispatch timed out")

// runScriptOnce 向 agent 发起一次脚本执行，遵循步骤超时与工作流取消。
func (e *Engine) runScriptOnce(ctx context.Context, conn agent.AgentConn, step models.WorkflowStep, timeout time.Duration) error {
	// 内容内联：与 handlers 单发/批量一致（docs/android-agent-design.md §3.2），
	// Android 等无服务器文件系统的 agent 依赖 content 字段；桌面 runtime 忽略
	// content 继续用 path。step.Script 已在 validateSteps 解析为绝对路径；
	// 桌面工作流的既有部署契约是 server 与 agent 共享文件系统，故读失败
	// 直接判步骤失败（此时 runtime 同样读不到，失败只是更早、信息更清晰）。
	content, err := scripts.ReadInline(step.Script)
	if err != nil {
		return fmt.Errorf("step %s: %w", step.ID, err)
	}

	done := make(chan error, 1)
	go func() {
		data := map[string]any{
			"path":          step.Script,
			"content":       string(content),
			"contentSha256": scripts.Sha256Of(content),
			"timeout":       int(timeout.Milliseconds()),
		}
		if lang := scripts.LanguageOf(step.Script); lang != "" {
			data["language"] = lang
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
		return fmt.Errorf("step %s timed out after %v: %w", step.ID, timeout, errStepTimeout)
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
		e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
		exec.setStepState(step.ID, func(s *models.StepStatus) {
			s.Status = "cancelled"
		})
		return ctx.Err()
	case <-time.After(time.Duration(seconds) * time.Second):
	}

	endTime := time.Now()
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "completed", "end_time": endTime})
	waitEndedAt := endTime
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "completed"
		s.EndTime = &waitEndedAt
	})
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
		e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
		exec.setStepState(step.ID, func(s *models.StepStatus) {
			s.Status = "cancelled"
		})
		return err
	}

	ok, message, err := evaluateCondition(step.Parameters)
	if err != nil {
		return e.failStep(ss, exec, step, err.Error())
	}
	// evaluateCondition 的 false 返回均携带非空 message，无需兜底默认值。
	if !ok {
		return e.failStep(ss, exec, step, message)
	}

	endTime := time.Now()
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "completed", "end_time": endTime, "message": message})
	condEndedAt := endTime
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "completed"
		s.Message = message
		s.EndTime = &condEndedAt
	})
	e.broadcastStepProgress(exec.Workflow.ID, step.ID, "completed", message)
	return nil
}

// executeScreenshotStep asks a selected agent to capture a screenshot and broadcasts
// the returned image through the existing Dashboard screenshot event channel.
func (e *Engine) executeScreenshotStep(ctx context.Context, exec *Execution, step models.WorkflowStep, ss *models.StepStatus) error {
	conn, workerID, err := e.selectAgent(step.Workers, step.Requires)
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
		e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{"status": "cancelled"})
		exec.setStepState(step.ID, func(s *models.StepStatus) {
			s.Status = "cancelled"
		})
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
	e.updateStepStatusRow(exec.Workflow.ID, step.ID, map[string]any{
		"status":    "completed",
		"end_time":  endTime,
		"worker_id": workerID,
		"message":   message,
	})
	exec.setStepState(step.ID, func(s *models.StepStatus) {
		s.Status = "completed"
		s.WorkerID = workerID
		s.Message = message
		s.EndTime = &endTime
	})
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
