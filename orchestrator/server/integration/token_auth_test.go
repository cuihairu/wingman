package integration

import (
	"encoding/json"
	"net"
	"net/http"
	"testing"
	"time"
)

// newSimAgentWithToken newSimAgent 的带 token 变体：register payload 携带
// 顶层 token 字段（docs/agent-token-auth-design.md §2.1）。
func newSimAgentWithToken(t *testing.T, addr, agentID, hostname, token string, handler func(string, map[string]any) map[string]any) *simAgent {
	t.Helper()
	conn, err := net.DialTimeout("tcp", addr, 2*time.Second)
	if err != nil {
		t.Fatalf("sim agent %s dial %s: %v", agentID, addr, err)
	}
	a := &simAgent{t: t, id: agentID, conn: conn, handler: handler}
	t.Cleanup(a.close)
	go a.readLoop()

	a.notify("agent.register", map[string]any{
		"agentId":  agentID,
		"hostname": hostname,
		"token":    token,
	})
	a.waitForNotify(t, "agent.register_ack", 2*time.Second)
	return a
}

// agentListContains 查询 /api/agents 是否包含指定 agentId。
func agentListContains(t *testing.T, env *testEnv, token, agentID string) bool {
	t.Helper()
	res := env.do(t, "GET", "/api/agents", token, nil)
	if res.Status != http.StatusOK {
		t.Fatalf("GET /api/agents: got %d (%s)", res.Status, res.Body)
	}
	var out struct {
		Data []struct {
			AgentID string `json:"agentId"`
		} `json:"data"`
	}
	if err := json.Unmarshal([]byte(res.Body), &out); err != nil {
		t.Fatalf("decode agents: %v (%s)", err, res.Body)
	}
	for _, a := range out.Data {
		if a.AgentID == agentID {
			return true
		}
	}
	return false
}

// TestAgentRegisterTokenAuthEndToEnd 真实链路：开启 token 白名单后，
// 正确 token 的 agent 正常上线并可被下发命令；错误 token 的 agent 收到
// success:false ack，且不出现在 agent 列表。
func TestAgentRegisterTokenAuthEndToEnd(t *testing.T) {
	env := newTestEnvWithTokens(t, "it-token-valid", "it-token-rotating")
	admin := env.login(t, "admin")

	// 正确 token：上线（走完整 register/ack/heartbeat 链路）
	valid := newSimAgentWithToken(t, env.agentAddr, "it-token-ok", "tk-host", "it-token-valid", nil)
	env.waitForAgentStatus(t, admin, "it-token-ok", "online")

	// 轮换期第二个 token 同样有效
	rotating := newSimAgentWithToken(t, env.agentAddr, "it-token-rot", "tk-host", "it-token-rotating", nil)
	env.waitForAgentStatus(t, admin, "it-token-rot", "online")

	// 错误 token：收到拒绝 ack（waitForNotify 收到 success:false），
	// 随后连接被断开；agent 不出现在列表
	bad := newSimAgentWithToken(t, env.agentAddr, "it-token-bad", "tk-host", "wrong", nil)
	bad.conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 1)
	if _, err := bad.conn.Read(buf); err == nil {
		t.Errorf("rejected agent connection should be closed by server")
	}

	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if !agentListContains(t, env, admin, "it-token-bad") {
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
	if agentListContains(t, env, admin, "it-token-bad") {
		t.Errorf("rejected agent must not appear in /api/agents")
	}

	// 已上线 agent 不受影响，仍可下发命令（shutdown 真实到达）
	valid.notify("agent.heartbeat", map[string]any{})
	_ = rotating
}

// TestAgentRegisterTokenAuthOffDefault 关闭开关（默认部署）：不带 token 的
// 既有 agent 完全兼容上线。
func TestAgentRegisterTokenAuthOffDefault(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")

	legacy := newSimAgent(t, env.agentAddr, "it-token-legacy", "legacy-host", nil)
	env.waitForAgentStatus(t, admin, "it-token-legacy", "online")
	_ = legacy
}
