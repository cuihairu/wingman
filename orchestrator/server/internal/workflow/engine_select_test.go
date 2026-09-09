package workflow

import (
	"testing"
)

// selectAgent 的负载比较分支依赖 registry.List() 的 map 迭代顺序，
// 单次执行可能命中不了 "load < bestLoad"。多次迭代确保 candidates[0]
// 负载更高、后续候选负载更低的排列出现，从而覆盖比较命中分支。
func TestSelectAgentLoadComparisonBranch(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "load-a", &mockConn{})
	registerAgent(reg, "load-b", &mockConn{})

	for i := 0; i < 32; i++ {
		e.mu.Lock()
		e.inflight = map[string]int{"load-a": 1, "load-b": 0}
		e.mu.Unlock()

		_, id, err := e.selectAgent(nil)
		if err != nil {
			t.Fatalf("selectAgent: %v", err)
		}
		if id != "load-b" {
			t.Fatalf("expected least-busy agent load-b, got %s", id)
		}
	}

	// 清空 inflight，避免影响其他用例
	e.mu.Lock()
	e.inflight = map[string]int{}
	e.mu.Unlock()
}
