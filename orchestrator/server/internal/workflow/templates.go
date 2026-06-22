package workflow

import "github.com/cuihaitao/wingman/orchestrator/server/internal/models"

// WorkflowTemplate 工作流模板（预定义骨架，供 dashboard 一键创建后定制）
type WorkflowTemplate struct {
	ID          string                `json:"id"`
	Name        string                `json:"name"`
	Description string                `json:"description"`
	Category    string                `json:"category"`
	Steps       []models.WorkflowStep `json:"steps"`
}

// BuiltinTemplates 内置模板目录。
//
// 模板中的 Script 为占位路径（用户需替换为实际脚本），Workers 留空表示由负载均衡自动分配。
// 旨在展示典型拓扑：单步、并行、串行流水线、fan-out + 汇总，并演示重试/超时配置。
func BuiltinTemplates() []WorkflowTemplate {
	return []WorkflowTemplate{
		{
			ID:          "screenshot-monitor",
			Name:        "截图监控循环",
			Description: "单 agent 周期性截图与检测，适合简单监控任务",
			Category:    "monitor",
			Steps: []models.WorkflowStep{
				{
					ID: "monitor", Name: "截图监控", Script: "monitor.lua",
					TimeoutSeconds: 300,
				},
			},
		},
		{
			ID:          "parallel-collect",
			Name:        "并行采集",
			Description: "多个独立步骤并行执行，分布到不同 agent，最大化吞吐",
			Category:    "collect",
			Steps: []models.WorkflowStep{
				{ID: "collect-a", Name: "采集 A", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"region": "a"}},
				{ID: "collect-b", Name: "采集 B", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"region": "b"}},
				{ID: "collect-c", Name: "采集 C", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"region": "c"}},
			},
		},
		{
			ID:          "sequential-pipeline",
			Name:        "串行流水线",
			Description: "按依赖顺序执行：准备 → 处理 → 收尾，失败自动重试",
			Category:    "pipeline",
			Steps: []models.WorkflowStep{
				{ID: "prepare", Name: "准备", Script: "prepare.lua", TimeoutSeconds: 60},
				{ID: "process", Name: "处理", Script: "process.lua", DependsOn: []string{"prepare"}, TimeoutSeconds: 180, MaxRetries: 2, RetryBackoffSeconds: 5},
				{ID: "finalize", Name: "收尾", Script: "finalize.lua", DependsOn: []string{"process"}, TimeoutSeconds: 60},
			},
		},
		{
			ID:          "fanout-reduce",
			Name:        "分发-汇总",
			Description: "fan-out 多路并行采集，全部完成后汇总处理",
			Category:    "pipeline",
			Steps: []models.WorkflowStep{
				{ID: "w1", Name: "Worker 1", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"index": 1}},
				{ID: "w2", Name: "Worker 2", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"index": 2}},
				{ID: "w3", Name: "Worker 3", Script: "collect.lua", TimeoutSeconds: 120, Parameters: map[string]any{"index": 3}},
				{ID: "reduce", Name: "汇总", Script: "reduce.lua", DependsOn: []string{"w1", "w2", "w3"}, TimeoutSeconds: 90},
			},
		},
		{
			ID:          "paced-pipeline",
			Name:        "带节拍的流水线",
			Description: "演示 wait 独立步骤类型：采集 → 等待冷却 → 处理，无需脚本即可控制节奏",
			Category:    "pipeline",
			Steps: []models.WorkflowStep{
				{ID: "collect", Name: "采集", Script: "collect.lua", TimeoutSeconds: 120},
				{ID: "cooldown", Name: "冷却等待", Type: "wait", DependsOn: []string{"collect"}, TimeoutSeconds: 30},
				{ID: "process", Name: "处理", Script: "process.lua", DependsOn: []string{"cooldown"}, TimeoutSeconds: 180},
			},
		},
		{
			ID:          "guarded-capture",
			Name:        "条件守卫截图",
			Description: "演示 condition + screenshot：先做前置判断，再由 agent 主动截图并广播到 Dashboard",
			Category:    "monitor",
			Steps: []models.WorkflowStep{
				{
					ID:   "guard",
					Name: "执行条件",
					Type: "condition",
					Parameters: map[string]any{
						"value":    true,
						"operator": "truthy",
					},
				},
				{
					ID:             "capture",
					Name:           "远程截图",
					Type:           "screenshot",
					DependsOn:      []string{"guard"},
					TimeoutSeconds: 30,
					Parameters: map[string]any{
						"displayId": 0,
					},
				},
			},
		},
	}
}
