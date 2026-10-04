package workflow

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// 步骤级 Execution 记录（ADR: Execution as the Platform Core Object）。
// 与 StepStatus 平行记录并存（收敛前保留），Execution 行带 WorkflowID/StepID/
// AgentID 挂载，是日志/审计/Artifact 的挂载点。DB IO 由调用方（步骤执行
// goroutine）串行执行，无需额外加锁。

// newExecutionID 生成全局唯一执行 ID（16 字节随机数 hex，碰撞概率可忽略）。
func newExecutionID() string {
	buf := make([]byte, 16)
	if _, err := rand.Read(buf); err != nil {
		return fmt.Sprintf("exec_%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
}

// finishStepExecution 落步骤级 Execution 终态（succeeded/failed/cancelled/timeout），
// Result 为 JSON 摘要。记录不存在（Create 失败留空 id）时 Where 静默跳过。
func (e *Engine) finishStepExecution(id uint, status models.ExecutionStatus, errText string) {
	if id == 0 {
		return
	}
	now := time.Now()
	result := map[string]any{"status": string(status)}
	if errText != "" {
		result["error"] = errText
	}
	buf, _ := json.Marshal(result)
	e.db.Model(&models.Execution{}).Where("id = ?", id).Updates(map[string]any{
		"status":      string(status),
		"finished_at": &now,
		"result":      string(buf),
	})
}
