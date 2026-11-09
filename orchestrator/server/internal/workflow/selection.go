package workflow

import (
	"fmt"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
)

// reserveAgent 增加 agent 在执行步骤计数
func (e *Engine) reserveAgent(id string) {
	e.mu.Lock()
	e.inflight[id]++
	e.mu.Unlock()
}

// releaseAgent 减少 agent 在执行步骤计数
func (e *Engine) releaseAgent(id string) {
	e.mu.Lock()
	if e.inflight[id] > 0 {
		e.inflight[id]--
	}
	e.mu.Unlock()
}

func (e *Engine) inflightFor(id string) int {
	e.mu.RLock()
	defer e.mu.RUnlock()
	return e.inflight[id]
}

// selectAgent 选择执行步骤的 agent。
// 显式指定 workers 时，在可用 worker 中选负载最低的；否则在所有在线 agent 中选负载最低的。
// requires 非空时先按声明要求过滤候选（ADR: Capability System）：platform
// 归一化（空按 desktop，与 registry.ToJSON 一致）匹配、capabilities 全部命中；
// 无候选时错误信息列出在线节点与各自缺失项，便于排查。
// 返回 (conn, agentID)。无可用 agent 时返回错误。
func (e *Engine) selectAgent(preferred []string, requires *models.StepRequirements) (agent.AgentConn, string, error) {
	agents := e.registry.List()

	var candidates, online []*agent.AgentInfo
	want := make(map[string]bool, len(preferred))
	for _, w := range preferred {
		want[w] = true
	}
	for _, a := range agents {
		if a.Client == nil || a.Status == agent.StatusOffline {
			continue
		}
		online = append(online, a)
		if len(preferred) > 0 && !want[a.AgentID] {
			continue
		}
		if requires != nil && missingRequirement(a, requires) != "" {
			continue
		}
		candidates = append(candidates, a)
	}
	if len(candidates) == 0 {
		return nil, "", e.noCandidateError(preferred, requires, online)
	}

	// 选负载最低的；并列时取列表顺序靠前的（稳定）
	best := candidates[0]
	bestLoad := e.inflightFor(best.AgentID)
	for _, a := range candidates[1:] {
		load := e.inflightFor(a.AgentID)
		if load < bestLoad {
			best = a
			bestLoad = load
		}
	}
	return best.Client, best.AgentID, nil
}

// missingRequirement 描述节点未满足步骤要求的部分（空串 = 满足）。
// platform 空值视为 desktop（旧版桌面 agent 不上报，见 docs/android-agent-design.md §3.3）；
// Capabilities 需全部命中（要求自身须来自词汇表，见 capabilities.go 注释）。
func missingRequirement(a *agent.AgentInfo, req *models.StepRequirements) string {
	missing := make([]string, 0, len(req.Capabilities)+1)
	if req.Platform != "" {
		plat := a.Platform
		if plat == "" {
			plat = "desktop"
		}
		if plat != req.Platform {
			missing = append(missing, "platform="+req.Platform)
		}
	}
	if len(req.Capabilities) > 0 {
		have := make(map[string]bool, len(a.Capabilities))
		for _, c := range a.Capabilities {
			have[c] = true
		}
		for _, c := range req.Capabilities {
			if !have[c] {
				missing = append(missing, c)
			}
		}
	}
	return strings.Join(missing, ", ")
}

// noCandidateError 无可用候选时的诊断错误：requires 无要求时保持既有文案
// （对应测试有断言），有要求时列出在线节点与各自缺失项。
func (e *Engine) noCandidateError(preferred []string, requires *models.StepRequirements, online []*agent.AgentInfo) error {
	if requires == nil {
		if len(preferred) > 0 {
			return fmt.Errorf("agent not connected: %v", preferred)
		}
		return fmt.Errorf("no available agent")
	}
	detail := make([]string, 0, len(online))
	for _, a := range online {
		name := a.AgentID
		if miss := missingRequirement(a, requires); miss != "" {
			name += "(missing " + miss + ")"
		}
		detail = append(detail, name)
	}
	if len(detail) == 0 {
		detail = []string{"none"}
	}
	return fmt.Errorf("no agent satisfies platform=%q capabilities=%v; online: %s",
		requires.Platform, requires.Capabilities, strings.Join(detail, ", "))
}
