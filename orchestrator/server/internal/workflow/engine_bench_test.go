package workflow

import (
	"fmt"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// benchConn 最小可用的 AgentConn，仅满足 selectAgent 注册需要（不实际执行脚本）。
type benchConn struct{}

func (benchConn) SendCommand(string, map[string]any) (map[string]any, error) {
	return map[string]any{"success": true}, nil
}
func (benchConn) SendCommandWithTimeout(string, map[string]any, time.Duration) (map[string]any, error) {
	return map[string]any{"success": true}, nil
}

// BenchmarkDetectCycle 大型 DAG 环检测（50 节点链 + 50 节点分支）。
func BenchmarkDetectCycle(b *testing.B) {
	e, _, _ := newTestEngine(b)
	steps := make([]models.WorkflowStep, 0, 100)
	// 50 节点线性链
	for i := 0; i < 50; i++ {
		s := models.WorkflowStep{ID: fmt.Sprintf("n%d", i), Script: "s.lua"}
		if i > 0 {
			s.DependsOn = []string{fmt.Sprintf("n%d", i-1)}
		}
		steps = append(steps, s)
	}
	// 50 节点分支，全部依赖 n49
	for i := 0; i < 50; i++ {
		steps = append(steps, models.WorkflowStep{
			ID: fmt.Sprintf("b%d", i), Script: "s.lua", DependsOn: []string{"n49"},
		})
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if err := e.detectCycle(steps); err != nil {
			b.Fatalf("detect cycle: %v", err)
		}
	}
}

// BenchmarkSelectAgent 20 个 agent 下的负载均衡选择（选 inflight 最少者）。
func BenchmarkSelectAgent(b *testing.B) {
	e, reg, _ := newTestEngine(b)
	for i := 0; i < 20; i++ {
		id := fmt.Sprintf("agent-%d", i)
		reg.Register(id, "h", "10.0.0.1", benchConn{})
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if _, _, err := e.selectAgent(nil); err != nil {
			b.Fatalf("select agent: %v", err)
		}
	}
}
