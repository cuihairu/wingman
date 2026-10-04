package workflow

import (
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// selectAgent 按 platform 过滤：未上报 platform 的旧版桌面 agent 归一为
// desktop 参与匹配；android 节点只在要求 android 时被选中。
func TestSelectAgentRequiresPlatform(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "desk-a", &mockConn{}) // 无 platform → desktop
	registerAgent(reg, "android-b", &mockConn{})
	reg.UpdatePlatform("android-b", "android")

	_, id, err := e.selectAgent(nil, &models.StepRequirements{Platform: "android"})
	if err != nil || id != "android-b" {
		t.Fatalf("android required: got id=%s err=%v, want android-b", id, err)
	}

	_, id, err = e.selectAgent(nil, &models.StepRequirements{Platform: "desktop"})
	if err != nil || id != "desk-a" {
		t.Fatalf("desktop required: got id=%s err=%v, want desk-a", id, err)
	}
}

// selectAgent 按 capabilities 过滤：要求的能力需全部命中；缺失即被剔除。
func TestSelectAgentRequiresCapabilities(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})
	reg.UpdateCapabilities("a1", []string{"screen.capture"})
	registerAgent(reg, "a2", &mockConn{})
	reg.UpdateCapabilities("a2", []string{"screen.capture", "input.touch"})

	_, id, err := e.selectAgent(nil, &models.StepRequirements{Capabilities: []string{"screen.capture"}})
	if err != nil {
		t.Fatalf("screen.capture: %v", err)
	}
	if id != "a1" && id != "a2" {
		t.Fatalf("screen.capture candidates wrong: %s", id)
	}

	_, id, err = e.selectAgent(nil, &models.StepRequirements{Capabilities: []string{"input.touch"}})
	if err != nil || id != "a2" {
		t.Fatalf("input.touch: got id=%s err=%v, want a2", id, err)
	}
}

// platform + capabilities 组合过滤，且要求为空字段时不设限。
func TestSelectAgentRequiresCombinedAndEmpty(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "android-cap", &mockConn{})
	reg.UpdatePlatform("android-cap", "android")
	reg.UpdateCapabilities("android-cap", []string{"screen.capture"})
	registerAgent(reg, "desk-cap", &mockConn{})
	reg.UpdateCapabilities("desk-cap", []string{"screen.capture"})

	req := &models.StepRequirements{Platform: "android", Capabilities: []string{"screen.capture"}}
	_, id, err := e.selectAgent(nil, req)
	if err != nil || id != "android-cap" {
		t.Fatalf("combined: got id=%s err=%v, want android-cap", id, err)
	}

	// 空要求 = 不限（等价 requires=nil 语义）
	_, id1, err1 := e.selectAgent(nil, &models.StepRequirements{})
	if err1 != nil || id1 == "" {
		t.Fatalf("empty requires should match anyone, got id=%s err=%v", id1, err1)
	}
}

// 无候选时的诊断错误：保留既有无要求文案；有要求时列出在线节点与缺失项。
func TestSelectAgentRequiresNoCandidateError(t *testing.T) {
	// 在线但能力缺失
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "a1", &mockConn{})
	reg.UpdateCapabilities("a1", []string{"screen.capture"})
	registerAgent(reg, "a2", &mockConn{}) // 无能力

	_, _, err := e.selectAgent(nil, &models.StepRequirements{Capabilities: []string{"ocr"}})
	if err == nil || !strings.Contains(err.Error(), "ocr") || !strings.Contains(err.Error(), "satisfies") {
		t.Fatalf("error should list missing capability, got %v", err)
	}
	if !strings.Contains(err.Error(), "a1") || !strings.Contains(err.Error(), "a2") {
		t.Errorf("error should list online agents, got %v", err)
	}

	// platform 缺失项在错误中标注
	_, _, err = e.selectAgent(nil, &models.StepRequirements{Platform: "ios", Capabilities: []string{"ocr"}})
	if err == nil || !strings.Contains(err.Error(), "platform=\"ios\"") {
		t.Fatalf("error should surface platform requirement, got %v", err)
	}

	// 无在线 agent → "none"
	e2, _, _ := newTestEngine(t)
	_, _, err = e2.selectAgent(nil, &models.StepRequirements{Platform: "android"})
	if err == nil || !strings.Contains(err.Error(), "none") {
		t.Fatalf("empty online should be explicit, got %v", err)
	}

	// 无要求时保留既有文案
	_, _, err = e2.selectAgent(nil, nil)
	if err == nil || err.Error() != "no available agent" {
		t.Fatalf("legacy no-requires message lost: %v", err)
	}
	_, _, err = e2.selectAgent([]string{"ghost"}, nil)
	if err == nil || !strings.Contains(err.Error(), "agent not connected: [ghost]") {
		t.Fatalf("legacy preferred message lost: %v", err)
	}
}

// workers 与 requires 同时给出：先按 requires 过滤，再在剩余候选中应用 workers。
func TestSelectAgentPreferredThenRequires(t *testing.T) {
	e, reg, _ := newTestEngine(t)
	registerAgent(reg, "android-a", &mockConn{})
	reg.UpdatePlatform("android-a", "android")
	reg.UpdateCapabilities("android-a", []string{"screen.capture"})
	registerAgent(reg, "desk-b", &mockConn{})
	reg.UpdateCapabilities("desk-b", []string{"screen.capture"})

	req := &models.StepRequirements{Platform: "android"}
	_, id, err := e.selectAgent([]string{"android-a", "desk-b"}, req)
	if err != nil || id != "android-a" {
		t.Fatalf("preferred+requires: got id=%s err=%v, want android-a", id, err)
	}

	// preferred 只给 desktop 节点 + android 要求 → 无候选，错误含缺失项
	_, _, err = e.selectAgent([]string{"desk-b"}, req)
	if err == nil || !strings.Contains(err.Error(), "missing platform=android") {
		t.Fatalf("mismatch should be diagnosed, got %v", err)
	}
}
