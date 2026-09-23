// Guacamole 像素面端到端链路测试（三协议）。
//
// 运行前提（默认跳过，显式开启才跑）：
//
//	export WINGMAN_GUACD_E2E=1
//	export WINGMAN_GUACD_E2E_GUACD=127.0.0.1:4822   # 可省，默认回环
//
// 目标端点（integration/testdata/guacd-e2e-compose.yml 一键拉起）：
//   - SSH  127.0.0.1:2222  linuxserver/openssh-server（test/test）
//   - VNC  127.0.0.1:5901  accetto/ubuntu-vnc-xfce（密码 headless）
//   - RDP  127.0.0.1:3389  danielguerra/ubuntu-xrdp
//
// 每条链路断言：REST 票据 → WS 升级 → 隧道 UUID → guacd ready（协议翻译
// 会话建立）→ 数据指令流（SSH 终端 pipe / VNC·RDP 像素 drawing），即
// guacd 已把后端协议转码为 Guacamole 指令流。
package integration

import (
	"encoding/json"
	"net/http"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

const guacE2EAgentID = "guac-e2e-agent"

// requireGuacE2E 统一环境 gate。
func requireGuacE2E(t *testing.T) {
	t.Helper()
	if os.Getenv("WINGMAN_GUACD_E2E") != "1" {
		t.Skip("set WINGMAN_GUACD_E2E=1 and run testdata/guacd-e2e-compose.yml targets to enable")
	}
}

// readGuacFrame 带超时读一条 WS 文本帧。
func readGuacFrame(t *testing.T, conn *websocket.Conn, timeout time.Duration) string {
	t.Helper()
	_ = conn.SetReadDeadline(time.Now().Add(timeout))
	_, data, err := conn.ReadMessage()
	if err != nil {
		t.Fatalf("read ws frame: %v", err)
	}
	return string(data)
}

// guacCollectFrames 读帧直到断言函数命中或超时；返回收到的全部帧。
// gorilla 约束：任何一次读失败（含 deadline 超时）都会毒化连接，禁止再读，
// 因此 deadline 一次设足总超时，循环内不做超时续读。
func guacCollectFrames(t *testing.T, conn *websocket.Conn, timeout time.Duration, hit func(string) bool) []string {
	t.Helper()
	_ = conn.SetReadDeadline(time.Now().Add(timeout))
	var got []string
	for {
		_, data, err := conn.ReadMessage()
		if err != nil {
			t.Fatalf("stopped waiting for frame after %d frames: %v; tail: %v",
				len(got), err, tailOf(got, 10))
		}
		frame := string(data)
		got = append(got, frame)
		if hit(frame) {
			return got
		}
	}
}

func tailOf(frames []string, n int) []string {
	if len(frames) <= n {
		return frames
	}
	return frames[len(frames)-n:]
}

// guacReadyHit ready 指令命中判定（guacd 会话建立）。
func guacReadyHit(frame string) bool {
	return strings.HasPrefix(frame, "5.ready,")
}

// guacSetupChain 全链路公共段：注册假 agent（IP 指向宿主回环，容器端口
// 映射在回环上）→ admin 登录 → 票据 → WS 升级 → 读隧道 UUID。
func guacSetupChain(t *testing.T, env *testEnv, protocol string, port int, username, password string) *websocket.Conn {
	t.Helper()

	env.registry.Register(guacE2EAgentID, "guac-e2e-host", "127.0.0.1", nil)

	token := env.login(t, "admin")
	res := env.do(t, http.MethodPost, "/api/remote/tickets", token, map[string]any{
		"agentId":  guacE2EAgentID,
		"protocol": protocol,
		"port":     port,
		"username": username,
		"password": password,
		"readOnly": false,
		"width":    1024,
		"height":   768,
	})
	if res.Status != http.StatusOK {
		t.Fatalf("%s: ticket request: %d %s", protocol, res.Status, res.Body)
	}
	resp := decodeJSON(t, res.Body)
	ticketID, _ := resp["data"].(map[string]any)["ticket"].(string)
	if ticketID == "" {
		t.Fatalf("%s: no ticket in response: %s", protocol, res.Body)
	}

	wsURL := "ws" + strings.TrimPrefix(env.httpSrv.URL, "http") + "/api/remote/guacamole?ticket=" + ticketID
	dialer := websocket.Dialer{HandshakeTimeout: 10 * time.Second}
	conn, _, err := dialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("%s: ws dial: %v", protocol, err)
	}
	t.Cleanup(func() { _ = conn.Close() })

	// 首帧必为隧道 UUID（空 opcode 内部指令）
	first := readGuacFrame(t, conn, 5*time.Second)
	if !strings.HasPrefix(first, "0.,32.") {
		t.Fatalf("%s: first frame should be tunnel uuid, got %q", protocol, first)
	}
	return conn
}

// decodeJSON 极简 JSON 展开（e2e 断言用）。
func decodeJSON(t *testing.T, raw []byte) map[string]any {
	t.Helper()
	var m map[string]any
	if err := json.Unmarshal(raw, &m); err != nil {
		t.Fatalf("decode json %s: %v", raw, err)
	}
	return m
}

// ---------- 三协议链路 ----------

func TestGuacdE2ESSHLink(t *testing.T) {
	requireGuacE2E(t)
	env := newTestEnv(t)
	conn := guacSetupChain(t, env, "ssh", 2222, "test", "test")

	// ready：guacd 连上 sshd 且协议翻译会话建立
	guacCollectFrames(t, conn, 20*time.Second, guacReadyHit)
	// 终端数据流：SSH 无像素，转码产物是 pipe（stdout）指令
	guacCollectFrames(t, conn, 15*time.Second, func(f string) bool {
		return strings.HasPrefix(f, "4.pipe,") || strings.HasPrefix(f, "4.size,")
	})
}

func TestGuacdE2EVNCLink(t *testing.T) {
	requireGuacE2E(t)
	env := newTestEnv(t)
	conn := guacSetupChain(t, env, "vnc", 5901, "", "headless")

	guacCollectFrames(t, conn, 20*time.Second, guacReadyHit)
	// 像素面转码证据：RFB 帧缓冲更新被翻译为 drawing 指令
	guacCollectFrames(t, conn, 15*time.Second, func(f string) bool {
		return strings.HasPrefix(f, "3.img,") || strings.HasPrefix(f, "3.png,") ||
			strings.HasPrefix(f, "4.rect,") || strings.HasPrefix(f, "4.copy,") ||
			strings.HasPrefix(f, "4.cfill,") || strings.HasPrefix(f, "6.avatar,")
	})
}

func TestGuacdE2ERDPLink(t *testing.T) {
	requireGuacE2E(t)
	env := newTestEnv(t)
	conn := guacSetupChain(t, env, "rdp", 3389, "ubuntu", "ubuntu")

	// xrdp 首次会话建立较慢（X session 启动），ready 与首帧 drawing 都给足窗口
	guacCollectFrames(t, conn, 40*time.Second, guacReadyHit)
	guacCollectFrames(t, conn, 40*time.Second, func(f string) bool {
		return strings.HasPrefix(f, "3.img,") || strings.HasPrefix(f, "3.png,") ||
			strings.HasPrefix(f, "4.rect,") || strings.HasPrefix(f, "4.copy,") ||
			strings.HasPrefix(f, "4.cfill,") || strings.HasPrefix(f, "6.avatar,")
	})
}

// ---------- 拒绝路径（不依赖 guacd） ----------

func TestGuacE2ETicketRejectsUnknownAgent(t *testing.T) {
	env := newTestEnv(t)
	token := env.login(t, "admin")
	res := env.do(t, http.MethodPost, "/api/remote/tickets", token, map[string]any{
		"agentId":  "no-such-agent",
		"protocol": "vnc",
	})
	if res.Status != http.StatusNotFound {
		t.Fatalf("unknown agent: got %d want 404 (%s)", res.Status, res.Body)
	}
}

func TestGuacE2ETicketRejectsBadProtocol(t *testing.T) {
	env := newTestEnv(t)
	token := env.login(t, "admin")
	env.registry.Register(guacE2EAgentID, "h", "127.0.0.1", nil)
	res := env.do(t, http.MethodPost, "/api/remote/tickets", token, map[string]any{
		"agentId":  guacE2EAgentID,
		"protocol": "ftp",
	})
	if res.Status != http.StatusBadRequest {
		t.Fatalf("bad protocol: got %d want 400 (%s)", res.Status, res.Body)
	}
}

func TestGuacE2EWSRejectsInvalidTicket(t *testing.T) {
	env := newTestEnv(t)
	wsURL := "ws" + strings.TrimPrefix(env.httpSrv.URL, "http") + "/api/remote/guacamole?ticket=bogus"
	dialer := websocket.Dialer{HandshakeTimeout: 5 * time.Second}
	_, resp, err := dialer.Dial(wsURL, nil)
	if err == nil {
		t.Fatalf("bogus ticket should be rejected")
	}
	if resp != nil && resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("bogus ticket: got %d want 401", resp.StatusCode)
	}
}
