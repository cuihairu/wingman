package workflow

import (
	"context"
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

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
			// 还有步骤未完成但无可执行步骤 → 死锁或等待，稍后再查。
			// 注：状态 failed 的步骤必然已完成（completed[s.ID]=true 由步骤 goroutine
			// 兜底写入），因此这里无需再按失败状态提前 break。
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

	// 确定最终状态。ctx 已取消时终态必须是 cancelled——被取消的步骤状态是
	// cancelled 不是 failed，anyStepStatus("failed") 判不出来，会误算成
	// completed（-race -count=50 实测 runner 因此抢先落库错误的 completed）。
	// 就算写入侧的条件守卫会让位，算出来的终态也必须诚实：万一未来出现其他
	// cancel ctx 的路径（引擎停机、超时策略），这里不再依赖「Cancel 先落库」
	// 这个顺序假设。
	finalStatus := "completed"
	switch {
	case ctx.Err() != nil:
		finalStatus = "cancelled"
	case exec.anyStepStatus("failed"):
		finalStatus = "failed"
	}

	// 终态收口：只允许把仍处于 running 的行改成终态（后到者让位，理由同 Cancel
	// 一侧的注释）。正常取消流程里 Cancel 先落库、runner 走到这里必然让位；
	// 本侧自己的回写只在无人取消、或取消落在终态写入之后的窗口里生效。
	now := time.Now()
	res := e.db.Model(&models.Workflow{}).
		Where("id = ? AND status = ?", exec.Workflow.ID, "running").
		Updates(map[string]any{"status": finalStatus, "end_time": now})
	switch {
	case res.Error != nil:
		log.Printf("[WorkflowEngine] Workflow %d 终态回写失败: %v", exec.Workflow.ID, res.Error)
	case res.RowsAffected == 0:
		// Cancel 先落库了：终态以 cancelled 为准，这里丢弃自己的终态与广播，
		// 否则前端会收到一条与库里不一致的 status_changed。
		log.Printf("[WorkflowEngine] Workflow %d 已被取消，终态回写让位（丢弃 %s）",
			exec.Workflow.ID, finalStatus)
		return
	}

	e.hub.BroadcastEvent("workflow", map[string]any{
		"event": "status_changed",
		"data": map[string]any{
			"workflowId": exec.Workflow.ID,
			"status":     finalStatus,
		},
	})

	log.Printf("[WorkflowEngine] Workflow %d 到达终态: %s", exec.Workflow.ID, finalStatus)
}

// findReadySteps 找到所有依赖已满足的就绪步骤
func (e *Engine) findReadySteps(exec *Execution, completed map[string]bool) []models.WorkflowStep {
	var ready []models.WorkflowStep
	for _, step := range exec.Steps {
		if completed[step.ID] {
			continue
		}
		if exec.stepStatusIn(step.ID, "running", "completed", "failed") {
			continue
		}

		allDepsDone := true
		for _, dep := range step.DependsOn {
			if !completed[dep] {
				allDepsDone = false
				break
			}
			// 检查依赖步骤是否失败
			if exec.stepStatusIn(dep, "failed") {
				// 依赖失败 → 跳过此步骤
				e.db.Model(&models.StepStatus{}).Where("workflow_id = ? AND step_id = ?", exec.Workflow.ID, step.ID).
					Updates(map[string]any{"status": "skipped", "message": "dependency failed"})
				exec.setStepState(step.ID, func(ss *models.StepStatus) {
					ss.Status = "skipped"
				})
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
