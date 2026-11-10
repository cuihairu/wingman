package models

import (
	"reflect"
	"testing"
	"time"
)

// ToView 视图形状测试：时间格式化、Result/Artifacts JSON 展开（合法/非法/
// 空）与字段直传。视图是 Logs/Audit API 的出参契约，形状漂移即测试红。

func TestExecutionToView(t *testing.T) {
	start := time.Date(2026, 11, 11, 8, 30, 0, 0, time.UTC)
	finish := time.Date(2026, 11, 11, 8, 31, 5, 0, time.UTC)

	t.Run("full round-trip with json result and artifacts", func(t *testing.T) {
		e := Execution{
			ExecutionID:  "exec-1",
			WorkflowID:   7,
			StepID:       "step-2",
			AgentID:      "agent-9",
			ScriptPath:   "scripts/deploy.lua",
			ScriptSha256: "ab12",
			Status:       ExecutionSucceeded,
			StartedAt:    &start,
			FinishedAt:   &finish,
			TimeoutSec:   120,
			Result:       `{"exit":0,"message":"ok"}`,
			Artifacts:    `["a.txt","b.log"]`,
		}
		e.CreatedAt = start
		v := e.ToView()

		if v.ID != e.ID || v.ExecutionID != "exec-1" || v.WorkflowID != 7 || v.StepID != "step-2" ||
			v.AgentID != "agent-9" || v.ScriptPath != "scripts/deploy.lua" || v.ScriptSha256 != "ab12" ||
			v.Status != ExecutionSucceeded || v.TimeoutSec != 120 {
			t.Fatalf("scalar fields not passed through: %+v", v)
		}
		if v.StartedAt != "2026-11-11 08:30:00" || v.FinishedAt != "2026-11-11 08:31:05" ||
			v.CreatedAt != "2026-11-11 08:30:00" {
			t.Fatalf("timestamps not formatted: %q %q %q", v.StartedAt, v.FinishedAt, v.CreatedAt)
		}
		want := map[string]any{"exit": float64(0), "message": "ok"}
		if !reflect.DeepEqual(v.Result, want) {
			t.Fatalf("result should be parsed json, got %#v", v.Result)
		}
		if !reflect.DeepEqual(v.Artifacts, []string{"a.txt", "b.log"}) {
			t.Fatalf("artifacts should be parsed list, got %#v", v.Artifacts)
		}
	})

	t.Run("non-json result passes through as string", func(t *testing.T) {
		e := Execution{Result: "plain error text", Status: ExecutionFailed}
		v := e.ToView()
		if s, ok := v.Result.(string); !ok || s != "plain error text" {
			t.Fatalf("non-json result should stay string, got %#v", v.Result)
		}
	})

	t.Run("empty and malformed fields degrade to zero values", func(t *testing.T) {
		e := Execution{Artifacts: "not-json"}
		v := e.ToView()
		if v.Result != nil {
			t.Fatalf("empty result should stay nil, got %#v", v.Result)
		}
		if v.Artifacts == nil || len(v.Artifacts) != 0 {
			t.Fatalf("malformed artifacts should degrade to empty list, got %#v", v.Artifacts)
		}
		if v.StartedAt != "" || v.FinishedAt != "" {
			t.Fatalf("nil timestamps should render empty: %q %q", v.StartedAt, v.FinishedAt)
		}
	})

	t.Run("zero-valued timestamps render empty", func(t *testing.T) {
		zero := time.Time{}
		e := Execution{StartedAt: &zero, FinishedAt: &zero}
		v := e.ToView()
		if v.StartedAt != "" || v.FinishedAt != "" {
			t.Fatalf("zero timestamps should render empty: %q %q", v.StartedAt, v.FinishedAt)
		}
	})
}
