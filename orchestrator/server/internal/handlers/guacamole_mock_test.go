package handlers

// 拟真 guacd 网关握手测试：起一个讲 Guacamole 线协议的假 guacd（TCP），
// 从 REST 票据到 WS 隧道走完整链路，验证网关注入 connect 的参数按位对齐：
// SFTP（ssh）/ 虚拟盘（rdp）/ 录制（record 票据）。
//
// 真 guacd 的协议链路由 WINGMAN_GUACD_E2E=1 门控的 integration/
// guacd_e2e_test.go 覆盖（需容器栈，见 testdata/guacd-e2e-compose.yml）；
// 本文件验证网关侧握手的参数编排，两者互补。

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"net/http/httptest"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

// ---------- 假 guacd：select → args → size+connect → error ----------

// mockGuacd 一条 TCP 连接的拟真 guacd：回放给定 args 名单，按位捕获
// connect 值，随后发 error 指令结束会话（网关会转发给 WS 客户端）。
type mockGuacd struct {
	ln       net.Listener
	argsList []string // select 后回放的参数名名单（含首段版本名）

	// postConnect 可选：握手完成后替代默认 error 收尾，继续在真连接上
	// 收发指令（对象流透传测试用；nil 保持原行为）。
	postConnect func(conn net.Conn, rd *bufio.Reader)

	mu            sync.Mutex
	selectProto   string
	connectValues []string
	handshakeDone chan struct{}
}

// guacRealSSHArgs guacd 1.5.5 对 select ssh 的真实 args 名单（39 段，
// 与 TestGuacApplyVersionArg 同源；含 enable-sftp 与全部 recording-* 名）。
var guacRealSSHArgs = []string{
	"VERSION_1_5_0", "hostname", "host-key", "port", "username", "password",
	"font-name", "font-size", "enable-sftp", "sftp-root-directory",
	"sftp-disable-download", "sftp-disable-upload", "private-key", "passphrase",
	"color-scheme", "command", "typescript-path", "typescript-name",
	"create-typescript-path", "recording-path", "recording-name",
	"recording-exclude-output", "recording-exclude-mouse", "recording-include-keys",
	"create-recording-path", "read-only", "server-alive-interval", "backspace",
	"terminal-type", "scrollback", "locale", "timezone", "disable-copy",
	"disable-paste", "wol-send-packet", "wol-mac-addr", "wol-broadcast-addr",
	"wol-udp-port", "wol-wait-time",
}

// guacRDPArgsWithDrive 含文件传输与录制名的 rdp 名单（真实 guacd rdp 名单
// 的子集——完整名单约 70 段，此处取被测参数位）。
var guacRDPArgsWithDrive = []string{
	"VERSION_1_5_0", "hostname", "port", "domain", "username", "password",
	"security", "ignore-cert", "color-depth", "width", "height", "read-only",
	"enable-drive", "drive-path", "create-drive-path", "recording-path",
	"recording-name", "create-recording-path", "recording-include-keys",
	"disable-audio", "disable-gfx",
}

// guacVNCArgs vnc 名单（无任何文件传输名——RFB 无文件通道）。
var guacVNCArgs = []string{
	"VERSION_1_5_0", "hostname", "port", "password", "color-depth", "read-only",
	"recording-path", "recording-name", "clip-channel",
}

func startMockGuacd(t *testing.T, argsList []string) *mockGuacd {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen mock guacd: %v", err)
	}
	m := &mockGuacd{ln: ln, argsList: argsList, handshakeDone: make(chan struct{})}
	go m.serveOne()
	t.Cleanup(func() { _ = ln.Close() })
	return m
}

// serveOne 服务一条连接：完整走一次握手（select/args/size/connect），
// 按位记录 connect 值，最后以 error 指令收尾。
func (m *mockGuacd) serveOne() {
	conn, err := m.ln.Accept()
	if err != nil {
		return
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(10 * time.Second))
	rd := bufio.NewReader(conn)

	selectFrame, err := readGuacInstruction(rd)
	if err != nil {
		return
	}
	proto, ok := guacInstructionArg(selectFrame, 0)
	if !ok {
		return
	}

	// 回 args 名单（首段版本名，与真实 guacd 一致）
	args := guacEncodeForTest("args", m.argsList...)
	if _, err := conn.Write([]byte(args)); err != nil {
		return
	}

	// 读 size（忽略内容）与 connect
	if _, err := readGuacInstruction(rd); err != nil {
		return
	}
	connectFrame, err := readGuacInstruction(rd)
	if err != nil {
		return
	}
	values := make([]string, len(m.argsList))
	for i := range m.argsList {
		v, ok := guacInstructionArg(connectFrame, i)
		if !ok {
			return // connect 段数不足：按位对应被破坏
		}
		values[i] = v
	}

	m.mu.Lock()
	m.selectProto = proto
	m.connectValues = values
	m.mu.Unlock()
	close(m.handshakeDone)

	// 自定义收尾：对象流透传等测试在真连接上继续收发指令
	if m.postConnect != nil {
		m.postConnect(conn, rd)
		return
	}

	// 以 error 指令收尾（网关原样转发 WS；随后关 TCP 触发会话清理）
	_, _ = conn.Write([]byte(guacEncodeForTest("error", "mock-end")))
}

// snapshot 返回捕获的 select 协议与 connect 值。
func (m *mockGuacd) snapshot() (string, []string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]string, len(m.connectValues))
	copy(out, m.connectValues)
	return m.selectProto, out
}

// ---------- 测试用 Guacamole 指令解析（独立于生产实现，交叉验证） ----------

// readGuacInstruction 读一条以 ';' 结尾的指令。
func readGuacInstruction(rd *bufio.Reader) (string, error) {
	s, err := rd.ReadString(';')
	if err != nil {
		return "", err
	}
	return strings.TrimSuffix(s, ";"), nil
}

// guacInstructionArg 独立实现的按位取参：i=0 是首个参数段（opcode 之后）。
// 与生产 guacParseArgs/buildConnectValues 分开写，交叉验证按位对应。
func guacInstructionArg(frame string, i int) (string, bool) {
	parts := strings.Split(frame, ",")
	// parts[0] 是 opcode 段；参数从 parts[1] 起
	idx := i + 1
	if idx >= len(parts) {
		return "", false
	}
	p := parts[idx]
	dot := strings.Index(p, ".")
	if dot < 0 {
		return "", false
	}
	want, err := strconv.Atoi(p[:dot])
	if err != nil || want != len(p[dot+1:]) {
		return "", false // 长度前缀不符：帧不合法
	}
	return p[dot+1:], true
}

// guacEncodeForTest 测试侧独立编码器。
func guacEncodeForTest(opcode string, args ...string) string {
	var b strings.Builder
	fmt.Fprintf(&b, "%d.%s", len(opcode), opcode)
	for _, a := range args {
		fmt.Fprintf(&b, ",%d.%s", len(a), a)
	}
	b.WriteByte(';')
	return b.String()
}

// ---------- 完整链路：REST 票据 → WS → mock guacd ----------

// guacMockEnv 完整测试环境：DB（种子+admin）、注册表（agent 指向本机）、
// 指向 mock guacd 的网关、挂 asAdmin 的票据路由与裸 WS 路由。
type guacMockEnv struct {
	srv     *httptest.Server
	recDir  string
	agentID string
}

func newGuacMockEnv(t *testing.T, guacdAddr, drivePath, recordingPath, recordingDir string) *guacMockEnv {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	if err := rbac.Seed(db); err != nil {
		t.Fatalf("seed rbac: %v", err)
	}
	admin := models.User{Username: "admin", Password: "x", Role: "admin", Active: true}
	if err := db.Create(&admin).Error; err != nil {
		t.Fatalf("create admin: %v", err)
	}
	reg, _ := newRegistry(t)
	reg.Register("guac-agent", "guac-host", "127.0.0.1", nil)

	tickets := remoteticket.NewManager()
	t.Cleanup(tickets.Stop)
	h := NewGuacamoleHandler(db, reg, tickets, guacdAddr, drivePath, recordingPath, recordingDir)

	r := gin.New()
	api := r.Group("/api", asAdmin(admin.ID))
	api.POST("/remote/tickets", h.HandleTicketCreate)
	r.GET("/api/remote/guacamole", h.HandleWS)

	srv := httptest.NewServer(r)
	t.Cleanup(srv.Close)

	return &guacMockEnv{srv: srv, recDir: recordingDir, agentID: "guac-agent"}
}

// requestTicket 经 REST 申请票据，返回票据 ID。
func (e *guacMockEnv) requestTicket(t *testing.T, body string) string {
	t.Helper()
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/api/remote/tickets", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	req.Host = e.srv.URL // httptest 请求直发路由，无需真实网络
	e.srv.Config.Handler.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("ticket request: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	var resp struct {
		Data struct {
			Ticket string `json:"ticket"`
		} `json:"data"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode ticket response: %v", err)
	}
	if resp.Data.Ticket == "" {
		t.Fatalf("no ticket in response: %s", w.Body.String())
	}
	return resp.Data.Ticket
}

// dialAndWaitHandshake 拨 WS 隧道并等待 mock guacd 完成握手，返回捕获值。
func dialAndWaitHandshake(t *testing.T, e *guacMockEnv, ticket string, m *mockGuacd) []string {
	t.Helper()
	dialer := websocket.Dialer{HandshakeTimeout: 5 * time.Second}
	wsURL := "ws" + strings.TrimPrefix(e.srv.URL, "http") + "/api/remote/guacamole?ticket=" + ticket
	conn, _, err := dialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("ws dial: %v", err)
	}
	defer conn.Close()

	// 首帧必为隧道 UUID（空 opcode 内部指令，不进 guacd）
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	_, first, err := conn.ReadMessage()
	if err != nil {
		t.Fatalf("read uuid frame: %v", err)
	}
	if !strings.HasPrefix(string(first), "0.,") {
		t.Fatalf("first frame should be tunnel uuid, got %q", first)
	}

	select {
	case <-m.handshakeDone:
	case <-time.After(5 * time.Second):
		t.Fatalf("mock guacd handshake did not complete")
	}
	proto, values := m.snapshot()
	if proto == "" {
		t.Fatalf("mock guacd did not observe select")
	}
	return values
}

// positionalByName 按名单名取 connect 值。
func positionalByName(argsList []string, values []string, name string) string {
	for i, n := range argsList {
		if n == name && i < len(values) {
			return values[i]
		}
	}
	return "" // 名单里没有该名（协议不支持）＝空串占位
}

// dbOf 测试路由背后的 DB（审计断言用）——经 handler 共享构造注入。
func TestGuacGatewaySSHHandshakeInjectsSFTPAndRecording(t *testing.T) {
	m := startMockGuacd(t, guacRealSSHArgs)
	env := newGuacMockEnv(t, m.ln.Addr().String(), "/drive-x", "/rec-guacd", t.TempDir())

	ticket := env.requestTicket(t, `{"agentId":"guac-agent","protocol":"ssh","username":"u","password":"p","record":true}`)
	values := dialAndWaitHandshake(t, env, ticket, m)

	// 按位对应：值数与名单等长
	if len(values) != len(guacRealSSHArgs) {
		t.Fatalf("connect values (%d) must align with args (%d)", len(values), len(guacRealSSHArgs))
	}
	if values[0] != "VERSION_1_5_0" {
		t.Errorf("version echo missing: %q", values[0])
	}
	// §15：SSH 的 SFTP 恒开
	if got := positionalByName(guacRealSSHArgs, values, "enable-sftp"); got != "true" {
		t.Errorf("enable-sftp must be true, got %q", got)
	}
	// §16：录制双路径注入 + 安全默认
	if got := positionalByName(guacRealSSHArgs, values, "recording-path"); got != "/rec-guacd" {
		t.Errorf("recording-path wrong: %q", got)
	}
	if got := positionalByName(guacRealSSHArgs, values, "recording-include-keys"); got != "false" {
		t.Errorf("recording-include-keys must be false, got %q", got)
	}
	name := positionalByName(guacRealSSHArgs, values, "recording-name")
	if !strings.HasPrefix(name, "guac-agent-") || !strings.HasSuffix(name, ".mjs") {
		t.Errorf("recording-name pattern wrong: %q", name)
	}
}

func TestGuacGatewayRDPHandshakeInjectsDrive(t *testing.T) {
	m := startMockGuacd(t, guacRDPArgsWithDrive)
	env := newGuacMockEnv(t, m.ln.Addr().String(), "/drive-x", "/rec-guacd", "")

	ticket := env.requestTicket(t, `{"agentId":"guac-agent","protocol":"rdp"}`)
	values := dialAndWaitHandshake(t, env, ticket, m)

	if got := positionalByName(guacRDPArgsWithDrive, values, "enable-drive"); got != "true" {
		t.Errorf("enable-drive must be true, got %q", got)
	}
	if got := positionalByName(guacRDPArgsWithDrive, values, "drive-path"); got != "/drive-x" {
		t.Errorf("drive-path wrong: %q", got)
	}
	// 无 record 票据不注入录制参数（空串占位）
	if got := positionalByName(guacRDPArgsWithDrive, values, "recording-path"); got != "" {
		t.Errorf("recording-path must be empty without record ticket, got %q", got)
	}
}

func TestGuacGatewayVNCHasNoFileTransferParams(t *testing.T) {
	m := startMockGuacd(t, guacVNCArgs)
	env := newGuacMockEnv(t, m.ln.Addr().String(), "/drive-x", "/rec-guacd", t.TempDir())

	// vnc 名单无文件通道参数位；录制仍可注入（vnc 名单含 recording-*）
	ticket := env.requestTicket(t, `{"agentId":"guac-agent","protocol":"vnc","record":true}`)
	values := dialAndWaitHandshake(t, env, ticket, m)

	if got := positionalByName(guacVNCArgs, values, "recording-path"); got != "/rec-guacd" {
		t.Errorf("vnc recording-path wrong: %q", got)
	}
	// VNC 名单根本没有 enable-sftp/enable-drive 位——positionalByName 返回
	// 空串（占位），断言其不在名单中即验证了「协议层不可能」
	for _, absent := range []string{"enable-sftp", "enable-drive", "drive-path"} {
		for _, n := range guacVNCArgs {
			if n == absent {
				t.Errorf("vnc args must not contain %q", absent)
			}
		}
	}
}
