package workflow

import (
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// ---------- 纯函数：条件求值 ----------

func TestEvaluateConditionExpression(t *testing.T) {
	ok, msg, err := evaluateCondition(map[string]any{"expression": true})
	if err != nil || !ok || msg != "condition matched" {
		t.Errorf("expression true: ok=%v msg=%q err=%v", ok, msg, err)
	}
	ok, msg, err = evaluateCondition(map[string]any{"expression": "false"})
	if err != nil || ok || msg != "condition expression is false" {
		t.Errorf("expression false: ok=%v msg=%q err=%v", ok, msg, err)
	}
	if _, _, err := evaluateCondition(map[string]any{"expression": "not-bool"}); err == nil {
		t.Error("non-boolean expression should error")
	}
}

func TestEvaluateConditionTruthyFalsy(t *testing.T) {
	ok, _, err := evaluateCondition(map[string]any{"value": "yes"})
	if err != nil || !ok {
		t.Errorf("truthy: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": ""})
	if err != nil || ok {
		t.Errorf("empty string is falsy: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"actual": 0})
	if err != nil || ok {
		t.Errorf("zero is falsy: ok=%v err=%v", ok, err)
	}
	if _, _, err := evaluateCondition(map[string]any{"value": struct{}{}}); err == nil {
		t.Error("non-boolean-compatible value should error")
	}
	ok, _, err = evaluateCondition(map[string]any{"value": "off", "operator": "falsy"})
	if err != nil || !ok {
		t.Errorf("falsy of off: ok=%v err=%v", ok, err)
	}
	if _, _, err := evaluateCondition(map[string]any{"value": struct{}{}, "operator": "falsy"}); err == nil {
		t.Error("falsy of non-boolean value should error")
	}
}

func TestEvaluateConditionEquality(t *testing.T) {
	ok, _, err := evaluateCondition(map[string]any{"value": 5, "operator": "eq", "expected": 5})
	if err != nil || !ok {
		t.Errorf("eq numbers: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": "5", "operator": "=", "expected": 5})
	if err != nil || !ok {
		t.Errorf("eq mixed types should coerce: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": true, "operator": "==", "expected": true})
	if err != nil || !ok {
		t.Errorf("eq bools: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": "a", "operator": "ne", "expected": "b"})
	if err != nil || !ok {
		t.Errorf("ne: ok=%v err=%v", ok, err)
	}
	if _, _, err := evaluateCondition(map[string]any{"value": 1, "operator": "eq"}); err == nil {
		t.Error("eq without expected should error")
	}
	if _, _, err := evaluateCondition(map[string]any{"value": 1, "operator": "ne"}); err == nil {
		t.Error("ne without expected should error")
	}
}

func TestEvaluateConditionComparisons(t *testing.T) {
	cases := []struct {
		op       string
		value    any
		expected any
		want     bool
	}{
		{"gt", 5, 4, true},
		{"gt", 5, 5, false},
		{"gte", 5, 5, true},
		{"gte", 4, 5, false},
		{"lt", 4, 5, true},
		{"lt", 5, 4, false},
		{"lte", 4, 4, true},
		{"lte", 5, 4, false},
	}
	for _, tc := range cases {
		ok, _, err := evaluateCondition(map[string]any{"value": tc.value, "operator": tc.op, "expected": tc.expected})
		if err != nil {
			t.Errorf("%s: err %v", tc.op, err)
			continue
		}
		if ok != tc.want {
			t.Errorf("%s %v %v: got %v want %v", tc.op, tc.value, tc.expected, ok, tc.want)
		}
	}
	if _, _, err := evaluateCondition(map[string]any{"value": "x", "operator": "gt", "expected": 1}); err == nil {
		t.Error("non-numeric actual should error")
	}
	if _, _, err := evaluateCondition(map[string]any{"value": 1, "operator": "gt", "expected": "y"}); err == nil {
		t.Error("non-numeric expected should error")
	}
	if _, _, err := evaluateCondition(map[string]any{"value": 1, "operator": "gt"}); err == nil {
		t.Error("gt without expected should error")
	}
}

func TestEvaluateConditionContains(t *testing.T) {
	ok, _, err := evaluateCondition(map[string]any{"value": "hello world", "operator": "contains", "expected": "world"})
	if err != nil || !ok {
		t.Errorf("contains string: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": []any{1.0, 2.0, 3.0}, "operator": "in", "expected": 2})
	if err != nil || !ok {
		t.Errorf("contains slice: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": 42, "operator": "contains", "expected": "4"})
	if err != nil || !ok {
		t.Errorf("contains fmt.Sprint fallback: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": "abc", "operator": "not_contains", "expected": "zzz"})
	if err != nil || !ok {
		t.Errorf("not_contains: ok=%v err=%v", ok, err)
	}
	ok, _, err = evaluateCondition(map[string]any{"value": nil, "operator": "contains", "expected": "x"})
	if err != nil || ok {
		t.Errorf("contains nil: ok=%v err=%v", ok, err)
	}
	if _, _, err := evaluateCondition(map[string]any{"value": "a", "operator": "contains"}); err == nil {
		t.Error("contains without expected should error")
	}
}

func TestEvaluateConditionOperatorAliasesAndUnknown(t *testing.T) {
	// op 别名
	ok, _, err := evaluateCondition(map[string]any{"value": true, "op": "is_true"})
	if err != nil || !ok {
		t.Errorf("op alias is_true: ok=%v err=%v", ok, err)
	}
	// 空白 operator 回退 truthy
	ok, _, err = evaluateCondition(map[string]any{"value": true, "operator": "   "})
	if err != nil || !ok {
		t.Errorf("blank operator: ok=%v err=%v", ok, err)
	}
	// 未知 operator
	if _, _, err := evaluateCondition(map[string]any{"value": 1, "operator": "bogus"}); err == nil {
		t.Error("unknown operator should error")
	}
}

func TestNormalizeConditionOperator(t *testing.T) {
	cases := map[string]string{
		"":               "truthy",
		"TRUE":           "truthy",
		"is_true":        "truthy",
		"False":          "falsy",
		"is_false":       "falsy",
		"=":              "eq",
		"==":             "eq",
		"equals":         "eq",
		"!=":             "ne",
		"<>":             "ne",
		"not_equals":     "ne",
		">":              "gt",
		">=":             "gte",
		"<":              "lt",
		"<=":             "lte",
		"contains":       "contains",
		"in":             "contains",
		"not_contains":   "not_contains",
		"not-contains":   "not_contains",
		"not in":         "not_contains",
		"not_in":         "not_contains",
		"  Weird  Op  ":  "weird  op",
	}
	for in, want := range cases {
		if got := normalizeConditionOperator(in); got != want {
			t.Errorf("normalizeConditionOperator(%q) = %q, want %q", in, got, want)
		}
	}
}

func TestValuesEqual(t *testing.T) {
	if !valuesEqual(1.0, 1) {
		t.Error("numeric equality failed")
	}
	if !valuesEqual(true, "true") {
		t.Error("bool/string coercion failed")
	}
	if !valuesEqual("x", "x") {
		t.Error("string equality failed")
	}
	if valuesEqual("abc", 2) {
		t.Error("string vs number should differ")
	}
	if valuesEqual(1, 2) {
		t.Error("1 and 2 should differ")
	}
}

func TestCompareNumbersUnknownOperator(t *testing.T) {
	if _, err := compareNumbers(1, 2, "weird"); err == nil {
		t.Error("unknown numeric operator should error")
	}
}

func TestContainsValueSliceAndFallback(t *testing.T) {
	if containsValue(nil, "x") {
		t.Error("nil should not contain")
	}
	if !containsValue([]any{"a", "b"}, "b") {
		t.Error("slice should contain element")
	}
	if containsValue([]any{"a"}, "z") {
		t.Error("slice should not contain missing element")
	}
	if !containsValue("hello", "ell") {
		t.Error("string contains failed")
	}
	if !containsValue(12345, "234") {
		t.Error("fmt.Sprint fallback contains failed")
	}
}

func TestToBoolVariants(t *testing.T) {
	truthy := []any{true, "true", "1", "yes", "y", "on", "TRUE", " Yes ", 1.0, float32(2), 3, int64(4), json.Number("5")}
	for _, v := range truthy {
		if b, ok := toBool(v); !ok || !b {
			t.Errorf("toBool(%#v) = %v,%v want true", v, b, ok)
		}
	}
	falsy := []any{false, "false", "0", "no", "n", "off", "", 0.0, float32(0), 0, int64(0), json.Number("0")}
	for _, v := range falsy {
		if b, ok := toBool(v); !ok || b {
			t.Errorf("toBool(%#v) = %v,%v want false", v, b, ok)
		}
	}
	for _, v := range []any{nil, "bogus", struct{}{}, json.Number("abc")} {
		if _, ok := toBool(v); ok {
			t.Errorf("toBool(%#v) should not be compatible", v)
		}
	}
}

func TestToFloatVariants(t *testing.T) {
	valid := []struct {
		in   any
		want float64
	}{
		{1.5, 1.5},
		{float32(2.5), 2.5},
		{3, 3},
		{int64(4), 4},
		{json.Number("5.5"), 5.5},
	}
	for _, tc := range valid {
		got, ok := toFloat(tc.in)
		if !ok || got != tc.want {
			t.Errorf("toFloat(%#v) = %v,%v want %v", tc.in, got, ok, tc.want)
		}
	}
	for _, v := range []any{"5", nil, true} {
		if _, ok := toFloat(v); ok {
			t.Errorf("toFloat(%#v) should fail", v)
		}
	}
}

func TestNumberAsInt(t *testing.T) {
	if n, ok := numberAsInt(3.7); !ok || n != 3 {
		t.Errorf("float: got %d,%v", n, ok)
	}
	if n, ok := numberAsInt(" 42 "); !ok || n != 42 {
		t.Errorf("string: got %d,%v", n, ok)
	}
	if _, ok := numberAsInt("x"); ok {
		t.Error("non-numeric string should fail")
	}
	if _, ok := numberAsInt(nil); ok {
		t.Error("nil should fail")
	}
}

func TestResponseError(t *testing.T) {
	if got := responseError(map[string]any{"error": "boom"}); got != "boom" {
		t.Errorf("error field: %q", got)
	}
	if got := responseError(map[string]any{"message": "oops"}); got != "oops" {
		t.Errorf("message field: %q", got)
	}
	if got := responseError(map[string]any{}); got != "unknown error" {
		t.Errorf("fallback: %q", got)
	}
}

func TestScreenshotPayloadVariants(t *testing.T) {
	if _, err := screenshotPayload(nil); err == nil {
		t.Error("nil resp should error")
	}
	if _, err := screenshotPayload(map[string]any{"success": false, "error": "denied"}); err == nil {
		t.Error("top-level failure should error")
	}
	if _, err := screenshotPayload(map[string]any{"success": true}); err == nil {
		t.Error("missing image should error")
	}
	if _, err := screenshotPayload(map[string]any{
		"success": true,
		"data":    map[string]any{"success": false, "message": "agent failed"},
	}); err == nil {
		t.Error("data-level failure should error")
	}
	payload, err := screenshotPayload(map[string]any{
		"success": true,
		"width":   10,
		"data":    map[string]any{"image": "img", "extra": 1},
	})
	if err != nil {
		t.Fatalf("valid payload: %v", err)
	}
	if payload["image"] != "img" || payload["extra"] != 1 {
		t.Errorf("payload should merge data fields: %+v", payload)
	}
}

func TestScreenshotMessage(t *testing.T) {
	if got := screenshotMessage(map[string]any{"width": 640, "height": 480}); got != "screenshot captured: 640x480" {
		t.Errorf("dimensioned: %q", got)
	}
	if got := screenshotMessage(map[string]any{"width": "x"}); got != "screenshot captured" {
		t.Errorf("no dims: %q", got)
	}
}

// ---------- 引擎辅助：workflowToJSON ----------

func TestWorkflowToJSON(t *testing.T) {
	e, _, _ := newTestEngine(t)
	now := time.Now()
	wf := &models.Workflow{Name: "w", Description: "d", Status: "running", StartTime: &now}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua"}})
	ss := &models.StepStatus{StepID: "s1", Name: "step", Status: "completed", WorkerID: "a1"}

	j := e.workflowToJSON(wf, map[string]*models.StepStatus{"s1": ss})
	if j["name"] != "w" || j["status"] != "running" {
		t.Errorf("unexpected json: %+v", j)
	}
	steps, ok := j["steps"].([]any)
	if !ok || len(steps) != 1 {
		t.Fatalf("steps not serialized: %#v", j["steps"])
	}
	stepStatus, ok := j["stepStatus"].(map[string]any)
	if !ok {
		t.Fatalf("stepStatus missing: %#v", j["stepStatus"])
	}
	entry, ok := stepStatus["s1"].(map[string]any)
	if !ok || entry["workerId"] != "a1" {
		t.Errorf("unexpected stepStatus entry: %#v", entry)
	}
}

// ---------- Cancel / 查询接口 ----------

func TestCancelNotRunning(t *testing.T) {
	e, _, _ := newTestEngine(t)
	if err := e.Cancel(12345); err == nil {
		t.Fatal("cancelling unknown workflow should error")
	}
}

func TestCancelRunningWorkflow(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})

	wf := &models.Workflow{Name: "cancellable"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "wait", Type: "wait", TimeoutSeconds: 30},
		{ID: "after", Type: "wait", DependsOn: []string{"wait"}, TimeoutSeconds: 1},
	})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}

	// 等待运行中
	waitFor(t, 2*time.Second, func() bool {
		_, ok := e.GetExecution(wf.ID)
		return ok
	}, "workflow should be running")

	if err := e.Cancel(wf.ID); err != nil {
		t.Fatalf("cancel: %v", err)
	}

	if _, ok := e.GetExecution(wf.ID); ok {
		t.Error("execution should be removed after cancel")
	}

	var got models.Workflow
	db.First(&got, wf.ID)
	if got.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", got.Status)
	}
	if got.EndTime == nil {
		t.Error("end time should be set")
	}

	// 未执行的 pending 步骤应被标记 skipped
	var skipped []models.StepStatus
	db.Where("workflow_id = ? AND status = ?", wf.ID, "skipped").Find(&skipped)
	if len(skipped) == 0 {
		t.Error("pending steps should be skipped after cancel")
	}
}

func TestListAndGetWorkflow(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "lw"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua"}})
	if err := db.Create(wf).Error; err != nil {
		t.Fatal(err)
	}
	db.Create(&models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "completed"})

	list, err := e.ListWorkflows()
	if err != nil || len(list) != 1 {
		t.Fatalf("list: %v count=%d", err, len(list))
	}

	got, steps, err := e.GetWorkflow(wf.ID)
	if err != nil || got.ID != wf.ID || len(steps) != 1 {
		t.Fatalf("get: %v %+v %+v", err, got, steps)
	}

	if _, _, err := e.GetWorkflow(99999); err == nil {
		t.Error("unknown workflow should error")
	}
}

// ---------- runScriptOnce 失败变体 ----------

func TestRunScriptOnceResponseFailures(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	step := models.WorkflowStep{ID: "s", Script: "a.lua"}

	conn := &mockConn{responses: []map[string]any{{"success": false, "message": "agent says no"}}}
	registerAgent(reg, "a1", conn)
	if err := e.runScriptOnce(context.Background(), conn, step, time.Second); err == nil {
		t.Error("success=false with message should error")
	}

	conn2 := &mockConn{responses: []map[string]any{{"success": false}}}
	registerAgent(reg, "a2", conn2)
	if err := e.runScriptOnce(context.Background(), conn2, step, time.Second); err == nil {
		t.Error("success=false without message should error")
	}

	connErr := &mockConn{errs: []error{errors.New("transport down")}}
	registerAgent(reg, "a3", connErr)
	if err := e.runScriptOnce(context.Background(), connErr, step, time.Second); err == nil {
		t.Error("command error should propagate")
	}
}

func TestRunScriptOnceTimesOut(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	conn := &mockConn{delay: 2 * time.Second}
	registerAgent(reg, "a1", conn)
	step := models.WorkflowStep{ID: "s", Script: "a.lua"}
	start := time.Now()
	err := e.runScriptOnce(context.Background(), conn, step, 200*time.Millisecond)
	if err == nil {
		t.Fatal("expected timeout error")
	}
	if time.Since(start) > time.Second {
		t.Error("timeout should not wait for slow agent")
	}
}

// ---------- executeStep：超时与重试期间取消 ----------

func TestExecuteStepScriptTimesOut(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{delay: 2 * time.Second}
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 1}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err == nil {
		t.Fatal("expected timeout failure")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

func TestExecuteStepCancelledDuringBackoff(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{responses: []map[string]any{{"success": false, "message": "flaky"}}}
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Script: "a.lua", TimeoutSeconds: 5,
		MaxRetries: 3, RetryBackoffSeconds: 30,
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	go func() {
		time.Sleep(200 * time.Millisecond)
		cancel()
	}()
	if err := e.executeStep(ctx, exec, exec.Steps[0]); err == nil {
		t.Fatal("expected cancellation")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled during backoff, got %s", ss.Status)
	}
}

// ---------- 依赖失败 → 下游 skipped ----------

func TestFindReadyStepsSkipsDownstreamOfFailed(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "a", Script: "a.lua"},
		{ID: "b", Script: "b.lua", DependsOn: []string{"a"}},
		{ID: "c", Script: "c.lua"},
	})
	db.Create(wf)
	ssA := &models.StepStatus{WorkflowID: wf.ID, StepID: "a", Status: "failed"}
	db.Create(ssA)
	ssB := &models.StepStatus{WorkflowID: wf.ID, StepID: "b", Status: "pending"}
	db.Create(ssB)
	ssC := &models.StepStatus{WorkflowID: wf.ID, StepID: "c", Status: "pending"}
	db.Create(ssC)
	exec := &Execution{
		Workflow:  wf,
		Steps:     wf.GetSteps(),
		StepState: map[string]*models.StepStatus{"a": ssA, "b": ssB, "c": ssC},
	}

	// a 已完成但失败：b 应被标记 skipped，c 就绪
	ready := e.findReadySteps(exec, map[string]bool{"a": true})
	if len(ready) != 1 || ready[0].ID != "c" {
		t.Fatalf("expected only c ready, got %+v", ready)
	}
	if ssB.Status != "skipped" {
		t.Errorf("downstream of failed dep should be skipped, got %s", ssB.Status)
	}

	var stored models.StepStatus
	db.Where("workflow_id = ? AND step_id = ?", wf.ID, "b").First(&stored)
	if stored.Status != "skipped" {
		t.Errorf("skip should be persisted, got %s", stored.Status)
	}

	// a 未计入 completed 时：a 因 failed 状态被跳过，b 因依赖未完成不就绪
	ssB.Status = "pending"
	ready = e.findReadySteps(exec, map[string]bool{})
	if len(ready) != 1 || ready[0].ID != "c" {
		t.Fatalf("expected only c ready when failed dep uncompleted, got %+v", ready)
	}
}

func TestExecuteWaitsAndExitsWhenNoReadySteps(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "b", Script: "b.lua", DependsOn: []string{"ghost"}},
		{ID: "dead", Script: "d.lua"},
	})
	db.Create(wf)
	ssB := &models.StepStatus{WorkflowID: wf.ID, StepID: "b", Status: "pending"}
	db.Create(ssB)
	ssDead := &models.StepStatus{WorkflowID: wf.ID, StepID: "dead", Status: "failed"}
	db.Create(ssDead)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"b": ssB, "dead": ssDead}}

	// ghost 依赖永不完成：execute 走“等待再检查”路径；用超时 ctx 退出
	ctx, cancel := context.WithTimeout(context.Background(), 700*time.Millisecond)
	defer cancel()
	done := make(chan struct{})
	go func() {
		e.execute(ctx, exec)
		close(done)
	}()
	select {
	case <-done:
	case <-time.After(5 * time.Second):
		t.Fatal("execute did not exit on ctx cancellation")
	}

	// ctx 取消属于提前返回：不写最终状态，步骤保持 pending
	if ssB.Status != "pending" {
		t.Errorf("step should remain pending, got %s", ssB.Status)
	}
	var got models.Workflow
	db.First(&got, wf.ID)
	if got.Status != "running" {
		t.Errorf("workflow should not be finalized on ctx exit, got %s", got.Status)
	}
}

func TestSubmitDependencyFailureWorkflowFails(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{{"success": false, "message": "boom"}}})

	wf := &models.Workflow{Name: "dep-fail"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "bad", Script: "a.lua", TimeoutSeconds: 5},
		{ID: "independent", Type: "wait", TimeoutSeconds: 1},
	})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}

	deadline := time.Now().Add(5 * time.Second)
	var status string
	for time.Now().Before(deadline) {
		var got models.Workflow
		db.First(&got, wf.ID)
		status = got.Status
		if status == "failed" {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if status != "failed" {
		t.Fatalf("expected failed, got %s", status)
	}
}

// ---------- validateConditionStep 直接单测 ----------

func TestValidateConditionStepVariants(t *testing.T) {
	if err := validateConditionStep(models.WorkflowStep{ID: "s", Type: "condition"}); err == nil {
		t.Error("empty parameters should be rejected")
	}
	if err := validateConditionStep(models.WorkflowStep{
		ID: "s", Type: "condition", Parameters: map[string]any{"expression": true},
	}); err != nil {
		t.Errorf("expression parameter should be accepted: %v", err)
	}
	if err := validateConditionStep(models.WorkflowStep{
		ID: "s", Type: "condition", Parameters: map[string]any{"actual": 1, "operator": "gt", "expected": 0},
	}); err != nil {
		t.Errorf("actual parameter should be accepted: %v", err)
	}
	if err := validateConditionStep(models.WorkflowStep{
		ID: "s", Type: "condition", Parameters: map[string]any{"operator": "gt"},
	}); err == nil {
		t.Error("parameters without value/expression should be rejected")
	}
}

// ---------- executeStep 边界 ----------

func TestExecuteStepMissingStepState(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "ghost", Script: "a.lua"}})
	db.Create(wf)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err == nil {
		t.Fatal("missing step state should error")
	}
}

func TestExecuteStepDefaultTimeoutAndClampedRetries(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{responses: []map[string]any{{"success": true}}}
	registerAgent(reg, "a1", conn)

	// 无 TimeoutSeconds → 默认超时；MaxRetries 负数 → 至少 1 次
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", MaxRetries: -2}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("default timeout path should succeed: %v", err)
	}
	if ss.Status != "completed" {
		t.Errorf("expected completed, got %s", ss.Status)
	}
}

func TestExecuteStepPreCancelledContext(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if err := e.executeStep(ctx, exec, exec.Steps[0]); err == nil {
		t.Fatal("pre-cancelled context should error")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", ss.Status)
	}
}

// ---------- screenshot：无 agent 与超时 ----------

func TestExecuteScreenshotStepNoAgent(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeScreenshotStep(context.Background(), exec, exec.Steps[0], ss); err == nil {
		t.Fatal("no agent should fail screenshot step")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

func TestExecuteScreenshotStepTimesOut(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{delay: 3 * time.Second})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot", TimeoutSeconds: 1}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeScreenshotStep(context.Background(), exec, exec.Steps[0], ss); err == nil {
		t.Fatal("expected timeout failure")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

// ---------- Submit 校验错误 ----------

func TestSubmitRejectsEmptySteps(t *testing.T) {
	e, _, _ := newTestEngine(t)
	wf := &models.Workflow{Name: "empty"}
	if err := e.Submit(wf); err == nil {
		t.Fatal("empty workflow should be rejected")
	}
}

func TestSubmitRejectsInvalidScriptPath(t *testing.T) {
	e, _, _ := newTestEngine(t)
	wf := &models.Workflow{Name: "bad-script"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s", Script: "not-lua.txt"}})
	err := e.Submit(wf)
	if err == nil {
		t.Fatal("invalid script should be rejected")
	}
	if !containsStr(err.Error(), "invalid workflow steps") {
		t.Errorf("error should wrap validation: %v", err)
	}
}

// ---------- wait / condition 细分 ----------

func TestExecuteWaitStepUsesParametersSeconds(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Type: "wait",
		Parameters: map[string]any{"seconds": 1},
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("wait with parameters.seconds: %v", err)
	}
	if ss.Status != "completed" {
		t.Errorf("expected completed, got %s", ss.Status)
	}
}

func TestExecuteWaitStepDefaultsToOneSecond(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "wait"}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	start := time.Now()
	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("default wait: %v", err)
	}
	if elapsed := time.Since(start); elapsed < 900*time.Millisecond {
		t.Errorf("default wait should sleep ~1s, elapsed %v", elapsed)
	}
}

func TestExecuteWaitStepIgnoresNonPositiveSeconds(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Type: "wait", TimeoutSeconds: -5,
		Parameters: map[string]any{"seconds": -3},
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	start := time.Now()
	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("negative wait: %v", err)
	}
	if elapsed := time.Since(start); elapsed < 900*time.Millisecond {
		t.Errorf("negative seconds should fall back to 1s, elapsed %v", elapsed)
	}
}

func TestExecuteConditionStepPreCancelled(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "condition", Parameters: map[string]any{"value": true}}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if err := e.executeConditionStep(ctx, exec, exec.Steps[0], ss); err == nil {
		t.Fatal("pre-cancelled ctx should error")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", ss.Status)
	}
}

func TestExecuteConditionStepInvalidExpressionFails(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Type: "condition",
		Parameters: map[string]any{"expression": "not-a-bool"},
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err == nil {
		t.Fatal("invalid expression should fail the step")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

func TestExecuteConditionStepCustomMessage(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID:         "s1", Type: "condition",
		Parameters: map[string]any{"value": false, "operator": "falsy"},
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeConditionStep(context.Background(), exec, exec.Steps[0], ss); err != nil {
		t.Fatalf("falsy of false should pass: %v", err)
	}
	if ss.Status != "completed" || ss.Message == "" {
		t.Errorf("expected completed with message, got %s %q", ss.Status, ss.Message)
	}
}

// ---------- screenshot 步骤失败变体 ----------

func TestExecuteScreenshotStepCancelled(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{delay: 5 * time.Second})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	go func() {
		time.Sleep(150 * time.Millisecond)
		cancel()
	}()
	if err := e.executeScreenshotStep(ctx, exec, exec.Steps[0], ss); err == nil {
		t.Fatal("expected cancellation")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", ss.Status)
	}
}

func TestExecuteScreenshotStepCommandError(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{errs: []error{errors.New("conn broken")}})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeScreenshotStep(context.Background(), exec, exec.Steps[0], ss); err == nil {
		t.Fatal("expected command error")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

func TestExecuteScreenshotStepNilResponse(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{nil}})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeScreenshotStep(context.Background(), exec, exec.Steps[0], ss); err == nil {
		t.Fatal("nil response should fail")
	}
}

func TestExecuteScreenshotStepUsesDefaultTimeout(t *testing.T) {
	e, reg, db := newTestEngine(t)
	// 未设置 TimeoutSeconds → 默认 30s；此处 agent 成功返回
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{{
		"success": true,
		"data":    map[string]any{"image": "img"},
	}}})

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "screenshot"}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeScreenshotStep(context.Background(), exec, exec.Steps[0], ss); err != nil {
		t.Fatalf("default timeout path: %v", err)
	}
	if ss.Status != "completed" || ss.Message != "screenshot captured" {
		t.Errorf("unexpected status/message: %s %q", ss.Status, ss.Message)
	}
}

// ---------- 并发提交（覆盖 execute 的并发批次） ----------

func TestSubmitParallelStepsDistribute(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{{"success": true}}})
	registerAgent(reg, "a2", &mockConn{responses: []map[string]any{{"success": true}}})

	wf := &models.Workflow{Name: "parallel"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "p1", Script: "a.lua", TimeoutSeconds: 5},
		{ID: "p2", Script: "b.lua", TimeoutSeconds: 5},
		{ID: "p3", Script: "c.lua", TimeoutSeconds: 5},
		{ID: "final", Script: "d.lua", DependsOn: []string{"p1", "p2", "p3"}, TimeoutSeconds: 5},
	})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}

	deadline := time.Now().Add(5 * time.Second)
	var status string
	for time.Now().Before(deadline) {
		var got models.Workflow
		e.db.First(&got, wf.ID)
		status = got.Status
		if status == "completed" || status == "failed" {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if status != "completed" {
		t.Fatalf("expected completed, got %s", status)
	}
}

// ---------- 辅助 ----------

func containsStr(s, sub string) bool {
	return strings.Contains(s, sub)
}

func waitFor(t *testing.T, timeout time.Duration, fn func() bool, message string) {
	t.Helper()
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if fn() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal(message)
}
