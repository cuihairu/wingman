package models

import (
	"encoding/json"
	"time"

	"gorm.io/gorm"
)

// ExecutionStatus 执行状态机（ADR: Execution as the Platform Core Object）：
// pending → queued → running → {succeeded | failed | cancelled | timeout | lost}
// v1 服务端同步路径不落 pending/queued 中间态，直接 running 起步。
type ExecutionStatus string

const (
	ExecutionPending   ExecutionStatus = "pending"
	ExecutionQueued    ExecutionStatus = "queued"
	ExecutionRunning   ExecutionStatus = "running"
	ExecutionSucceeded ExecutionStatus = "succeeded"
	ExecutionFailed    ExecutionStatus = "failed"
	ExecutionCancelled ExecutionStatus = "cancelled"
	ExecutionTimeout   ExecutionStatus = "timeout"
	ExecutionLost      ExecutionStatus = "lost"
)

// Execution 平台核心执行对象：把 run_script / batch / workflow 步骤统一到
// 一个以 executionId 寻址的记录上，成为 Logs / Artifacts / Audit 的挂载点。
// 服务端是唯一事实源；runtime 经 command.result / event.script_log 报回。
//
// v1 接线范围：单发 run_script（handlers.ScriptHandler.HandleRun）在
// 下发前创建、命令收尾后落终态；workflow 引擎的 StepStatus 与内部
// Execution 是既有的并行记录，收敛到本模型前不得再新增执行形态功能。
type Execution struct {
	gorm.Model
	ExecutionID string          `gorm:"uniqueIndex;not null" json:"executionId"`
	WorkflowID  uint            `json:"workflowId"` // 0 = 直接下发，非 workflow 步骤
	StepID      string          `json:"stepId"`
	AgentID     string          `gorm:"index" json:"agentId"`
	ScriptPath  string          `json:"scriptPath"`
	Status      ExecutionStatus `gorm:"default:queued;index" json:"status"`
	StartedAt   *time.Time      `json:"startedAt"`
	FinishedAt  *time.Time      `json:"finishedAt"`
	TimeoutSec  int             `json:"timeoutSec"`
	// Result JSON 文本：成功为 command result data 摘要，失败为错误信息。
	Result string `gorm:"type:text" json:"-"`
	// Artifacts JSON 数组文本（执行产物引用，v1 预留，恒空数组）。
	Artifacts string `gorm:"type:text" json:"-"`
}

// TableName 表名
func (Execution) TableName() string { return "executions" }

// ExecutionView 对外视图（展开 Result/Artifacts JSON，隐藏原始文本列）。
type ExecutionView struct {
	ID          uint            `json:"id"`
	ExecutionID string          `json:"executionId"`
	WorkflowID  uint            `json:"workflowId"`
	StepID      string          `json:"stepId"`
	AgentID     string          `json:"agentId"`
	ScriptPath  string          `json:"scriptPath"`
	Status      ExecutionStatus `json:"status"`
	StartedAt   string          `json:"startedAt"`
	FinishedAt  string          `json:"finishedAt"`
	TimeoutSec  int             `json:"timeoutSec"`
	Result      any             `json:"result"`
	Artifacts   []string        `json:"artifacts"`
	CreatedAt   string          `json:"createdAt"`
}

// ToView 产出脱敏后视图。
func (e Execution) ToView() ExecutionView {
	fmtTime := func(t *time.Time) string {
		if t == nil || t.IsZero() {
			return ""
		}
		return t.Format("2006-01-02 15:04:05")
	}
	v := ExecutionView{
		ID:          e.ID,
		ExecutionID: e.ExecutionID,
		WorkflowID:  e.WorkflowID,
		StepID:      e.StepID,
		AgentID:     e.AgentID,
		ScriptPath:  e.ScriptPath,
		Status:      e.Status,
		StartedAt:   fmtTime(e.StartedAt),
		FinishedAt:  fmtTime(e.FinishedAt),
		TimeoutSec:  e.TimeoutSec,
		Artifacts:   []string{},
		CreatedAt:   fmtTime(&e.CreatedAt),
	}
	if e.Result != "" {
		var r any
		if json.Unmarshal([]byte(e.Result), &r) == nil {
			v.Result = r
		} else {
			v.Result = e.Result
		}
	}
	if e.Artifacts != "" {
		var arts []string
		if json.Unmarshal([]byte(e.Artifacts), &arts) == nil {
			v.Artifacts = arts
		}
	}
	return v
}
