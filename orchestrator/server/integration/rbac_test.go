package integration

import (
	"net/http"
	"strings"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

// TestRBACAgentOperationPermissions 验证真实中间件链下的权限矩阵：
// JWT 认证 → 角色/权限解析（admin 通配、operator 细粒度、viewer 只读）→ Agent 操作。
func TestRBACAgentOperationPermissions(t *testing.T) {
	env := newTestEnv(t)
	admin := env.login(t, "admin")
	operator := env.login(t, "operator")
	viewer := env.login(t, "viewer")

	a := newSimAgent(t, env.agentAddr, "it-rbac-agent", "rbac-host", nil)
	env.waitForAgentStatus(t, admin, "it-rbac-agent", "online")

	validSteps := []map[string]any{{"id": "s1", "script": "rbac.lua"}}

	cases := []struct {
		name   string
		method string
		path   string
		token  string
		body   any
		want   int
	}{
		// 认证层：无 token → 401
		{"无 token 查看 agent 列表", "GET", "/api/agents", "", nil, http.StatusUnauthorized},
		{"畸形 token 查看 agent 列表", "GET", "/api/agents", "garbage.token.sig", nil, http.StatusUnauthorized},

		// 只读层：所有登录用户
		{"viewer 查看 agent 列表", "GET", "/api/agents", viewer, nil, http.StatusOK},
		{"viewer 查看单个 agent", "GET", "/api/agents/it-rbac-agent", viewer, nil, http.StatusOK},
		{"viewer 查看脚本列表", "GET", "/api/scripts", viewer, nil, http.StatusOK},
		{"operator 查看 agent 列表", "GET", "/api/agents", operator, nil, http.StatusOK},

		// agents:manage：viewer 无 → 403；operator 有 → 200
		{"viewer 关闭 agent → 403", "POST", "/api/agents/it-rbac-agent/shutdown", viewer, nil, http.StatusForbidden},
		{"viewer 设置 agent 标签 → 403", "PUT", "/api/agents/it-rbac-agent/tags", viewer, map[string]any{"tags": []string{"x"}}, http.StatusForbidden},

		// scripts:run：viewer 无 → 403；operator 有 → 200（agent 在线）
		{"viewer 运行脚本 → 403", "POST", "/api/scripts/run", viewer, map[string]any{"path": "rbac.lua"}, http.StatusForbidden},
		{"operator 运行脚本", "POST", "/api/scripts/run", operator, map[string]any{"path": "rbac.lua"}, http.StatusOK},

		// workflows:run：viewer 无 → 403；operator 有 → 200
		{"viewer 提交工作流 → 403", "POST", "/api/workflows", viewer, map[string]any{"name": "v", "steps": validSteps}, http.StatusForbidden},
		{"operator 提交工作流", "POST", "/api/workflows", operator, map[string]any{"name": "op", "steps": validSteps}, http.StatusOK},

		// scripts:edit：viewer 无 → 403
		{"viewer 创建脚本 → 403", "POST", "/api/scripts", viewer, map[string]any{"name": "v.lua"}, http.StatusForbidden},

		// /api/v1 写接口为 RoleRequired(admin)：operator 也被拒
		{"operator POST /api/v1/screenshot → 403", "POST", "/api/v1/screenshot", operator, map[string]any{"image": "eA=="}, http.StatusForbidden},
		{"admin POST /api/v1/screenshot", "POST", "/api/v1/screenshot", admin, map[string]any{"image": "eA==", "width": 1, "height": 1}, http.StatusOK},

		// agents:manage 收尾：operator 关闭 agent（放在最后，agent 将转 offline）
		{"operator 关闭 agent", "POST", "/api/agents/it-rbac-agent/shutdown", operator, nil, http.StatusOK},
	}
	for _, tc := range cases {
		res := env.do(t, tc.method, tc.path, tc.token, tc.body)
		if res.Status != tc.want {
			t.Errorf("%s: %s %s got %d want %d (%s)", tc.name, tc.method, tc.path, res.Status, tc.want, res.Body)
		}
	}

	// shutdown 与 run_script 命令真实到达 agent
	cmds := a.waitForCommands(t, "system.shutdown", 1, 3*time.Second)
	if cmds[0].Method != "system.shutdown" {
		t.Errorf("shutdown 命令方法名: got %q", cmds[0].Method)
	}
	a.waitForCommands(t, "run_script", 1, 3*time.Second)

	// operator 的 shutdown 生效后：agent offline（写审计 + 状态联动）
	env.waitForAgentStatus(t, admin, "it-rbac-agent", "offline")

	// 被禁用用户无法登录
	env.loginWith(t, "ghostop", http.StatusForbidden)

	// WebSocket 无有效 token：握手被拒（HTTP 401）
	wsURL := strings.Replace(env.httpSrv.URL, "http://", "ws://", 1) + "/ws"
	_, resp, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err == nil {
		t.Error("无 token 的 WS 握手应失败")
	} else if resp == nil || resp.StatusCode != http.StatusUnauthorized {
		t.Errorf("无 token WS 握手状态: got %v want 401", resp)
	}
}
