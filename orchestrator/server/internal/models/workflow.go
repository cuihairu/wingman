package models

import (
	"encoding/json"
	"time"

	"gorm.io/gorm"
)

// Workflow 工作流模型
type Workflow struct {
	gorm.Model
	Name         string     `gorm:"not null" json:"name"`
	Description  string     `json:"description"`
	Status       string     `gorm:"default:pending" json:"status"` // pending/running/completed/failed/cancelled
	StepsJSON    string     `gorm:"type:text;column:steps_json" json:"-"`
	ContextJSON  string     `gorm:"type:text;column:context_json" json:"-"`
	StartTime    *time.Time `json:"startTime"`
	EndTime      *time.Time `json:"endTime"`
}

// TableName 表名
func (Workflow) TableName() string { return "workflows" }

// GetSteps 解析步骤
func (w *Workflow) GetSteps() []WorkflowStep {
	if w.StepsJSON == "" {
		return nil
	}
	var steps []WorkflowStep
	json.Unmarshal([]byte(w.StepsJSON), &steps)
	return steps
}

// SetSteps 序列化步骤
func (w *Workflow) SetSteps(steps []WorkflowStep) error {
	data, err := json.Marshal(steps)
	if err != nil {
		return err
	}
	w.StepsJSON = string(data)
	return nil
}

// GetContext 解析共享上下文
func (w *Workflow) GetContext() map[string]interface{} {
	if w.ContextJSON == "" {
		return nil
	}
	var ctx map[string]interface{}
	json.Unmarshal([]byte(w.ContextJSON), &ctx)
	return ctx
}

// SetContext 序列化共享上下文
func (w *Workflow) SetContext(ctx map[string]interface{}) error {
	data, err := json.Marshal(ctx)
	if err != nil {
		return err
	}
	w.ContextJSON = string(data)
	return nil
}

// WorkflowStep 工作流步骤
type WorkflowStep struct {
	ID                  string                 `json:"id"`
	Name                string                 `json:"name"`
	Type                string                 `json:"type"` // 步骤类型：script(默认)/wait
	Script              string                 `json:"script"`
	Workers             []string               `json:"workers"`
	DependsOn           []string               `json:"dependsOn"`
	TimeoutSeconds      int                    `json:"timeoutSeconds"`
	Parameters          map[string]interface{} `json:"parameters"`
	MaxRetries          int                    `json:"maxRetries"`          // 失败重试次数（0=不重试）
	RetryBackoffSeconds int                    `json:"retryBackoffSeconds"` // 重试退避基数（秒，默认 2，指数翻倍）
}

// StepStatus 步骤执行状态
type StepStatus struct {
	gorm.Model
	WorkflowID uint       `gorm:"index" json:"workflowId"`
	StepID     string     `gorm:"index" json:"stepId"`
	Name       string     `json:"name"`
	Status     string     `gorm:"default:pending" json:"status"` // pending/running/completed/failed/skipped
	WorkerID   string     `json:"workerId"`
	Message    string     `json:"message"`
	StartTime  *time.Time `json:"startTime"`
	EndTime    *time.Time `json:"endTime"`
}

// TableName 表名
func (StepStatus) TableName() string { return "step_statuses" }
