package workflow

import (
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

func TestBuiltinTemplatesNonEmpty(t *testing.T) {
	templates := BuiltinTemplates()
	if len(templates) == 0 {
		t.Fatal("expected non-empty builtin templates")
	}
	seen := map[string]bool{}
	for _, tpl := range templates {
		if tpl.ID == "" {
			t.Errorf("template has empty ID: %+v", tpl)
		}
		if seen[tpl.ID] {
			t.Errorf("duplicate template ID: %s", tpl.ID)
		}
		seen[tpl.ID] = true
		if len(tpl.Steps) == 0 {
			t.Errorf("template %s has no steps", tpl.ID)
		}
	}
}

// 模板应可作为合法工作流提交（DAG 无环、步骤 ID 唯一）。
func TestBuiltinTemplatesAreValidDAG(t *testing.T) {
	e, _, _ := newTestEngine(t) // 复用 engine_test.go 的辅助

	for _, tpl := range BuiltinTemplates() {
		t.Run(tpl.ID, func(t *testing.T) {
			if err := e.detectCycle(tpl.Steps); err != nil {
				t.Errorf("template %s has cycle: %v", tpl.ID, err)
			}
			// 验证依赖指向存在
			ids := map[string]bool{}
			for _, s := range tpl.Steps {
				ids[s.ID] = true
			}
			for _, s := range tpl.Steps {
				for _, dep := range s.DependsOn {
					if !ids[dep] {
						t.Errorf("template %s step %s depends on missing %s", tpl.ID, s.ID, dep)
					}
				}
			}
		})
	}
}

func TestTemplateFanOutReduceDeps(t *testing.T) {
	var fanout *WorkflowTemplate
	for _, tpl := range BuiltinTemplates() {
		if tpl.ID == "fanout-reduce" {
			fanout = &tpl
			break
		}
	}
	if fanout == nil {
		t.Fatal("fanout-reduce template missing")
	}
	var reduce *models.WorkflowStep
	for i := range fanout.Steps {
		if fanout.Steps[i].ID == "reduce" {
			reduce = &fanout.Steps[i]
		}
	}
	if reduce == nil {
		t.Fatal("reduce step missing")
	}
	if len(reduce.DependsOn) != 3 {
		t.Errorf("reduce should depend on 3 workers, got %d", len(reduce.DependsOn))
	}
}
