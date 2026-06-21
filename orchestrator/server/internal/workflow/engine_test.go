package workflow

import (
	"context"
	"sync"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	ws "github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
)

// mockConn 可配置返回结果与调用计数的假 AgentConn。
type mockConn struct {
	mu       sync.Mutex
	responses []map[string]any   // 依次返回；不足时重复最后一个
	errs     []error
	calls    int
	delay    time.Duration
}

func (m *mockConn) next() (map[string]any, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls++
	idx := m.calls - 1
	var err error
	if idx < len(m.errs) {
		err = m.errs[idx]
	}
	var resp map[string]any
	if idx < len(m.responses) {
		resp = m.responses[idx]
	} else if len(m.responses) > 0 {
		resp = m.responses[len(m.responses)-1]
	}
	if resp == nil {
		resp = map[string]any{"success": true}
	}
	if m.delay > 0 {
		time.Sleep(m.delay)
	}
	return resp, err
}

func (m *mockConn) SendCommand(method string, data map[string]any) (map[string]any, error) {
	return m.next()
}
func (m *mockConn) SendCommandWithTimeout(method string, data map[string]any, timeout time.Duration) (map[string]any, error) {
	return m.next()
}
func (m *mockConn) callCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.calls
}

func newTestEngine(t *testing.T) (*Engine, *agent.Registry, *gorm.DB) {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{})
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	// 强制单连接：execute() 在后台 goroutine 写 DB，主线程读；
	// 否则 :memory: 连接池中各连接指向不同数据库，写不可见。
	sqlDB, _ := db.DB()
	sqlDB.SetMaxOpenConns(1)
	if err := models.AutoMigrate(db); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	hub := ws.NewHub()
	go hub.Run()
	reg := agent.NewRegistry(hub)
	e := NewEngine(db, reg, hub, t.TempDir())
	return e, reg, db
}

func registerAgent(reg *agent.Registry, id string, conn agent.AgentConn) {
	reg.Register(id, "host-"+id, "10.0.0."+id[len(id)-1:], conn)
}

// ---------- 纯逻辑：DAG 环检测 ----------

func TestDetectCycleAcceptsDAG(t *testing.T) {
	e, _, _ := newTestEngine(t)
	steps := []models.WorkflowStep{
		{ID: "a", Script: "a.lua"},
		{ID: "b", Script: "b.lua", DependsOn: []string{"a"}},
		{ID: "c", Script: "c.lua", DependsOn: []string{"a", "b"}},
	}
	if err := e.detectCycle(steps); err != nil {
		t.Fatalf("DAG should be accepted: %v", err)
	}
}

func TestDetectCycleRejectsCycle(t *testing.T) {
	e, _, _ := newTestEngine(t)
	steps := []models.WorkflowStep{
		{ID: "a", Script: "a.lua", DependsOn: []string{"c"}},
		{ID: "b", Script: "b.lua", DependsOn: []string{"a"}},
		{ID: "c", Script: "c.lua", DependsOn: []string{"b"}},
	}
	if err := e.detectCycle(steps); err == nil {
		t.Fatal("cycle should be rejected")
	}
}

// ---------- 纯逻辑：步骤校验 ----------

func TestValidateSteps(t *testing.T) {
	e, _, _ := newTestEngine(t)

	if err := e.validateSteps([]models.WorkflowStep{{ID: "a", Script: "a.lua"}, {ID: "a", Script: "b.lua"}}); err == nil {
		t.Error("duplicate ID should be rejected")
	}
	if err := e.validateSteps([]models.WorkflowStep{{ID: "", Script: "a.lua"}}); err == nil {
		t.Error("empty ID should be rejected")
	}
	if err := e.validateSteps([]models.WorkflowStep{{ID: "a", Script: "a.lua", DependsOn: []string{"ghost"}}}); err == nil {
		t.Error("missing dependency should be rejected")
	}
	if err := e.validateSteps([]models.WorkflowStep{{ID: "a", Script: "a.exe"}}); err == nil {
		t.Error("non-lua script should be rejected")
	}
	if err := e.validateSteps([]models.WorkflowStep{
		{ID: "a", Script: "a.lua"},
		{ID: "b", Script: "sub/b.lua", DependsOn: []string{"a"}},
	}); err != nil {
		t.Errorf("valid steps should pass: %v", err)
	}
}

// ---------- 负载均衡：selectAgent ----------

func TestSelectAgentPicksLeastBusy(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	busy := &mockConn{responses: []map[string]any{{"success": true}}}
	idle := &mockConn{responses: []map[string]any{{"success": true}}}
	registerAgent(reg, "a1", busy)
	registerAgent(reg, "a2", idle)

	// 模拟 a1 已有一个在执行步骤
	e.reserveAgent("a1")

	conn, id, err := e.selectAgent(nil)
	if err != nil {
		t.Fatalf("selectAgent: %v", err)
	}
	if id != "a2" {
		t.Errorf("expected least-busy agent a2, got %s", id)
	}
	if conn == nil {
		t.Error("expected non-nil conn")
	}
}

func TestSelectAgentRespectsPreferred(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})
	registerAgent(reg, "a2", &mockConn{})

	// 指定只允许 a2
	_, id, err := e.selectAgent([]string{"a2"})
	if err != nil || id != "a2" {
		t.Errorf("expected a2, got %s err=%v", id, err)
	}

	// 指定不存在的 worker
	_, _, err = e.selectAgent([]string{"ghost"})
	if err == nil {
		t.Error("expected error for unknown preferred worker")
	}
}

func TestSelectAgentNoAgentAvailable(t *testing.T) {
	e, _, _ := newTestEngine(t) // 无 agent 注册
	_, _, err := e.selectAgent(nil)
	if err == nil {
		t.Error("expected error when no agent available")
	}
}

// ---------- executeStep：成功 ----------

func TestExecuteStepSuccess(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{responses: []map[string]any{{"success": true}}}
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 5}})
	db.Create(wf)

	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	err := e.executeStep(context.Background(), exec, exec.Steps[0])
	if err != nil {
		t.Fatalf("expected success, got %v", err)
	}
	if ss.Status != "completed" {
		t.Errorf("expected completed, got %s", ss.Status)
	}
	if ss.WorkerID != "a1" {
		t.Errorf("expected worker a1, got %s", ss.WorkerID)
	}
}

// ---------- executeStep：重试后成功 ----------

func TestExecuteStepRetryThenSuccess(t *testing.T) {
	e, reg, db := newTestEngine(t)
	// 前两次失败，第三次成功
	conn := &mockConn{
		responses: []map[string]any{
			{"success": false, "message": "transient"},
			{"success": false, "message": "transient"},
			{"success": true},
		},
	}
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Script: "a.lua", TimeoutSeconds: 5,
		MaxRetries: 3, RetryBackoffSeconds: 0, // 立即重试以加速测试
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("expected eventual success, got %v", err)
	}
	if ss.Status != "completed" {
		t.Errorf("expected completed, got %s", ss.Status)
	}
	if conn.callCount() != 3 {
		t.Errorf("expected 3 attempts, got %d", conn.callCount())
	}
}

// ---------- executeStep：重试耗尽失败 ----------

func TestExecuteStepRetryExhausted(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{responses: []map[string]any{{"success": false, "message": "fail"}}}
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{
		ID: "s1", Script: "a.lua", TimeoutSeconds: 5,
		MaxRetries: 2, RetryBackoffSeconds: 0,
	}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err == nil {
		t.Fatal("expected failure after retries exhausted")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
	if conn.callCount() != 3 { // 1 + 2 retries
		t.Errorf("expected 3 attempts, got %d", conn.callCount())
	}
}

// ---------- executeStep：无可用 agent ----------

func TestExecuteStepNoAgent(t *testing.T) {
	e, _, db := newTestEngine(t) // 无 agent

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 5}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err == nil {
		t.Fatal("expected error when no agent")
	}
	if ss.Status != "failed" {
		t.Errorf("expected failed, got %s", ss.Status)
	}
}

// ---------- executeStep：取消 ----------

func TestExecuteStepCancelled(t *testing.T) {
	e, reg, db := newTestEngine(t)
	conn := &mockConn{delay: 2 * time.Second} // 慢响应
	registerAgent(reg, "a1", conn)

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Script: "a.lua", TimeoutSeconds: 10}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	go func() {
		time.Sleep(100 * time.Millisecond)
		cancel()
	}()
	if err := e.executeStep(ctx, exec, exec.Steps[0]); err == nil {
		t.Fatal("expected cancellation error")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", ss.Status)
	}
}

// ---------- Submit 端到端 ----------

func TestSubmitRunsWorkflowToCompletion(t *testing.T) {
	e, reg, db := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{responses: []map[string]any{{"success": true}}})

	wf := &models.Workflow{Name: "end-to-end"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "s1", Script: "a.lua", TimeoutSeconds: 5},
		{ID: "s2", Script: "b.lua", TimeoutSeconds: 5, DependsOn: []string{"s1"}},
	})
	if err := e.Submit(wf); err != nil {
		t.Fatalf("submit: %v", err)
	}

	// 轮询等待完成
	deadline := time.Now().Add(5 * time.Second)
	var status string
	for time.Now().Before(deadline) {
		var got models.Workflow
		db.First(&got, wf.ID)
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

func TestSubmitRejectsCycle(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})

	wf := &models.Workflow{Name: "cyclic"}
	wf.SetSteps([]models.WorkflowStep{
		{ID: "a", Script: "a.lua", DependsOn: []string{"b"}},
		{ID: "b", Script: "b.lua", DependsOn: []string{"a"}},
	})
	if err := e.Submit(wf); err == nil {
		t.Fatal("submit should reject cyclic workflow")
	}
}

// ---------- 步骤类型校验 ----------

func TestValidateStepsRejectsUnknownType(t *testing.T) {
	e, _, _ := newTestEngine(t)
	err := e.validateSteps([]models.WorkflowStep{{ID: "s", Type: "bogus", Script: "a.lua"}})
	if err == nil {
		t.Fatal("unknown step type should be rejected")
	}
}

func TestValidateStepsWaitDoesNotRequireScript(t *testing.T) {
	e, _, _ := newTestEngine(t)
	// wait 步骤无 Script 也应通过校验
	err := e.validateSteps([]models.WorkflowStep{{ID: "s", Type: "wait", TimeoutSeconds: 2}})
	if err != nil {
		t.Errorf("wait step should not require script: %v", err)
	}
}

// ---------- wait 步骤执行 ----------

func TestExecuteWaitStepCompletes(t *testing.T) {
	e, _, db := newTestEngine(t) // wait 不需要 agent

	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "wait", TimeoutSeconds: 1}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	start := time.Now()
	if err := e.executeStep(context.Background(), exec, exec.Steps[0]); err != nil {
		t.Fatalf("wait step should succeed: %v", err)
	}
	elapsed := time.Since(start)
	if elapsed < 900*time.Millisecond {
		t.Errorf("wait step should sleep ~1s, elapsed %v", elapsed)
	}
	if ss.Status != "completed" {
		t.Errorf("expected completed, got %s", ss.Status)
	}
}

func TestExecuteWaitStepCancellable(t *testing.T) {
	e, _, db := newTestEngine(t)
	wf := &models.Workflow{Name: "w", Status: "running"}
	wf.SetSteps([]models.WorkflowStep{{ID: "s1", Type: "wait", TimeoutSeconds: 10}})
	db.Create(wf)
	ss := &models.StepStatus{WorkflowID: wf.ID, StepID: "s1", Status: "pending"}
	db.Create(ss)
	exec := &Execution{Workflow: wf, Steps: wf.GetSteps(), StepState: map[string]*models.StepStatus{"s1": ss}}

	ctx, cancel := context.WithCancel(context.Background())
	go func() {
		time.Sleep(100 * time.Millisecond)
		cancel()
	}()
	if err := e.executeStep(ctx, exec, exec.Steps[0]); err == nil {
		t.Fatal("expected cancellation")
	}
	if ss.Status != "cancelled" {
		t.Errorf("expected cancelled, got %s", ss.Status)
	}
}
