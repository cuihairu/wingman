package handlers

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/gin-gonic/gin"
)

// ---------- guacEncode ----------

func TestGuacEncode(t *testing.T) {
	cases := []struct {
		opcode string
		args   []string
		want   string
	}{
		{"size", []string{"1024", "768", "96"}, "4.size,4.1024,3.768,2.96;"},
		{"select", []string{"rdp"}, "6.select,3.rdp;"},
		{"connect", []string{"hostname", "3389"}, "7.connect,8.hostname,4.3389;"},
		{"", []string{"uuid-1"}, "0.,6.uuid-1;"},
		{"sync", nil, "4.sync;"},
	}
	for _, tc := range cases {
		if got := guacEncode(tc.opcode, tc.args...); got != tc.want {
			t.Errorf("guacEncode(%q, %v) = %q, want %q", tc.opcode, tc.args, got, tc.want)
		}
	}
}

// ---------- guacParseArgs：真实 guacd args 响应形状 ----------

func TestGuacParseArgs(t *testing.T) {
	// guacd 1.5.x 对 select rdp 的真实响应形状：首段是协议版本名
	// （VERSION_x_y_z），connect 必须按位回应——版本段不可跳过，否则
	// 参数整体少一位，guacd 以 "Client did not return the expected number
	// of arguments" 拒连（e2e 实测回归护栏）。
	rdp := "4.args,13.VERSION_1_5_0,8.hostname,4.port,6.domain,8.username,8.password," +
		"8.security,11.ignore-cert,11.color-depth,5.width,6.height,9.read-only;"
	got := guacParseArgs(rdp)
	want := []string{"VERSION_1_5_0", "hostname", "port", "domain", "username", "password",
		"security", "ignore-cert", "color-depth", "width", "height", "read-only"}
	if len(got) != len(want) {
		t.Fatalf("args count = %d, want %d: %v", len(got), len(want), got)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Errorf("args[%d] = %q, want %q", i, got[i], want[i])
		}
	}

	if guacParseArgs("5.error,3.bad;") != nil {
		t.Errorf("non-args instruction should return nil")
	}
	if guacParseArgs("") != nil {
		t.Errorf("empty frame should return nil")
	}
}

// ---------- connect 参数按位组装（协议正确性核心） ----------

func TestGuacConnectValuesAlignedToArgs(t *testing.T) {
	// guacd 的 args 名单含版本段与未提供的参数：版本位回显占位，
	// 其余缺省位必须为空串，总个数与名单等长
	argNames := guacParseArgs("4.args,13.VERSION_1_5_0,8.hostname,4.port,8.username,8.password,6.domain,11.color-depth,9.read-only,5.width,6.height;")
	table := guacParamTable("rdp", "10.0.0.5", 3389,
		map[string]string{"username": "admin", "password": "secret"}, 1920, 1080, true)
	guacApplyVersionArg(argNames, table)
	values := buildConnectValues(argNames, table)
	want := []string{"VERSION_1_5_0", "10.0.0.5", "3389", "admin", "secret", "", "32", "true", "1920", "1080"}
	if len(values) != len(want) {
		t.Fatalf("values count = %d, want %d: %v", len(values), len(want), values)
	}
	for i := range want {
		if values[i] != want[i] {
			t.Errorf("values[%d] = %q, want %q", i, values[i], want[i])
		}
	}
	if len(values) != len(argNames) {
		t.Fatalf("connect values (%d) must align with args names (%d)", len(values), len(argNames))
	}

	// 整条指令可见格式：值里不含 "name=" 前缀（cockpit 版缺陷回归护栏）
	line := guacEncode("connect", values...)
	if strings.Contains(line, "hostname=") || strings.Contains(line, "password=") {
		t.Errorf("connect values must be bare values, not name=value: %s", line)
	}
}

func TestGuacApplyVersionArg(t *testing.T) {
	// 真实 guacd 1.5.5 ssh 名单（39 段，首段版本）→ connect 39 值
	sshArgs := "4.args,13.VERSION_1_5_0,8.hostname,8.host-key,4.port,8.username,8.password," +
		"9.font-name,9.font-size,11.enable-sftp,19.sftp-root-directory,21.sftp-disable-download," +
		"19.sftp-disable-upload,11.private-key,10.passphrase,12.color-scheme,7.command," +
		"15.typescript-path,15.typescript-name,22.create-typescript-path,14.recording-path," +
		"14.recording-name,24.recording-exclude-output,23.recording-exclude-mouse," +
		"22.recording-include-keys,21.create-recording-path,9.read-only,21.server-alive-interval," +
		"9.backspace,13.terminal-type,10.scrollback,6.locale,8.timezone,12.disable-copy," +
		"13.disable-paste,15.wol-send-packet,12.wol-mac-addr,18.wol-broadcast-addr," +
		"12.wol-udp-port,13.wol-wait-time;"
	argNames := guacParseArgs(sshArgs)
	if len(argNames) != 39 {
		t.Fatalf("ssh args count = %d, want 39", len(argNames))
	}
	table := guacParamTable("ssh", "127.0.0.1", 2222,
		map[string]string{"username": "test", "password": "test"}, 1024, 768, false)
	guacApplyVersionArg(argNames, table)
	values := buildConnectValues(argNames, table)
	if len(values) != len(argNames) {
		t.Fatalf("values (%d) must align with args (%d)", len(values), len(argNames))
	}
	if values[0] != "VERSION_1_5_0" {
		t.Errorf("values[0] should echo version, got %q", values[0])
	}
	if values[1] != "127.0.0.1" || values[3] != "2222" || values[4] != "test" || values[5] != "test" {
		t.Errorf("positional values wrong: %v", values[:6])
	}
	// 空名单/无版本段名单：不 panic、不注入
	safe := map[string]string{}
	guacApplyVersionArg(nil, safe)
	if len(safe) != 0 {
		t.Errorf("nil names should not inject")
	}
	guacApplyVersionArg([]string{"hostname"}, safe)
	if len(safe) != 0 {
		t.Errorf("non-version first name should not inject")
	}
}

func TestGuacParamTablePerProtocol(t *testing.T) {
	rdp := guacParamTable("rdp", "h", 3389, nil, 0, 0, false)
	if rdp["security"] != "any" || rdp["ignore-cert"] != "true" || rdp["color-depth"] != "32" {
		t.Errorf("rdp defaults missing: %v", rdp)
	}
	// 音频必须显式禁用：官方 guacd 镜像音频编码器缺失，RDP 音频流会触发
	// guac_audio_assign_encoder NULL crash（e2e 实测）
	if rdp["disable-audio"] != "true" {
		t.Errorf("rdp must disable audio: %v", rdp)
	}
	if _, ok := rdp["width"]; ok {
		t.Errorf("width should be omitted when 0")
	}
	vnc := guacParamTable("vnc", "h", 5900, map[string]string{"password": "pw"}, 0, 0, true)
	if vnc["password"] != "pw" || vnc["read-only"] != "true" || vnc["color-depth"] != "32" {
		t.Errorf("vnc table wrong: %v", vnc)
	}
	ssh := guacParamTable("ssh", "h", 22, map[string]string{"username": "u", "password": "p"}, 0, 0, false)
	if ssh["username"] != "u" || ssh["password"] != "p" || ssh["read-only"] != "false" {
		t.Errorf("ssh table wrong: %v", ssh)
	}
	if _, ok := ssh["color-depth"]; ok {
		t.Errorf("ssh should not carry color-depth")
	}
}

// ---------- guacIsInternal（ping 拦截判定） ----------

func TestGuacIsInternal(t *testing.T) {
	cases := []struct {
		frame string
		want  bool
	}{
		{"0.,12.abc123def456;", true},  // 空 opcode（uuid/ping 家族）
		{"0.5.ping;", true},            // INTERNAL_DATA ping（空 opcode 单元素）
		{"4.size,3.256;", false},       // 正常指令
		{"6.select,3.rdp;", false},     // 正常指令
		{"size,3.256;", false},         // 无长度前缀
		{".ping;", false},              // 空长度段
		{"1a.size,3.256;", false},      // 非数字长度
	}
	for _, tc := range cases {
		if got := guacIsInternal([]byte(tc.frame)); got != tc.want {
			t.Errorf("guacIsInternal(%q) = %v, want %v", tc.frame, got, tc.want)
		}
	}
}

// ---------- 票据双通道提取 ----------

func TestGuacTicketFromRequest(t *testing.T) {
	req := httptest.NewRequest(http.MethodGet, "/api/remote/guacamole?ticket=abc123", nil)
	if got := guacTicketFromRequest(req); got != "abc123" {
		t.Errorf("query channel: got %q", got)
	}

	req2 := httptest.NewRequest(http.MethodGet, "/api/remote/guacamole", nil)
	req2.Header.Set("Sec-WebSocket-Protocol", "ticket-via-subproto")
	if got := guacTicketFromRequest(req2); got != "ticket-via-subproto" {
		t.Errorf("subprotocol channel: got %q", got)
	}

	req3 := httptest.NewRequest(http.MethodGet, "/api/remote/guacamole", nil)
	if got := guacTicketFromRequest(req3); got != "" {
		t.Errorf("no ticket should be empty, got %q", got)
	}

	// query 优先于 subprotocol
	req4 := httptest.NewRequest(http.MethodGet, "/api/remote/guacamole?ticket=q", nil)
	req4.Header.Set("Sec-WebSocket-Protocol", "s")
	if got := guacTicketFromRequest(req4); got != "q" {
		t.Errorf("query must win: got %q", got)
	}
}

// ---------- REST 票据 handler 的拒绝路径 ----------

func newGuacTestHandler(t *testing.T) (*GuacamoleHandler, *gin.Engine) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	tickets := remoteticket.NewManager()
	t.Cleanup(tickets.Stop)
	reg, _ := newRegistry(t)
	h := NewGuacamoleHandler(db, reg, tickets, "127.0.0.1:4822", "", "", "")
	r := gin.New()
	r.POST("/api/remote/tickets", h.HandleTicketCreate)
	return h, r
}

func TestHandleTicketCreateValidation(t *testing.T) {
	_, r := newGuacTestHandler(t)

	// 非法协议
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/api/remote/tickets",
		strings.NewReader(`{"agentId":"a1","protocol":"telnet"}`))
	req.Header.Set("Content-Type", "application/json")
	c, _ := gin.CreateTestContext(w)
	c.Request = req
	r.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("bad protocol: got %d want 400 (%s)", w.Code, w.Body.String())
	}
}

// ---------- 阶段二：文件传输 / 录制参数（设计 §15/§16） ----------

func TestGuacApplyFileTransfer(t *testing.T) {
	// SSH：SFTP 恒开（§15——鉴权仍在 SSH 凭证层）
	ssh := map[string]string{}
	guacApplyFileTransfer("ssh", ssh, "/wingman-drive")
	if ssh["enable-sftp"] != "true" {
		t.Errorf("ssh must enable sftp: %v", ssh)
	}
	if _, ok := ssh["enable-drive"]; ok {
		t.Errorf("ssh must not carry drive params: %v", ssh)
	}

	// RDP：虚拟盘双参数，drive-path 取配置注入
	rdp := map[string]string{}
	guacApplyFileTransfer("rdp", rdp, "/data/drive")
	if rdp["enable-drive"] != "true" || rdp["drive-path"] != "/data/drive" {
		t.Errorf("rdp drive params wrong: %v", rdp)
	}

	// VNC：RFB 无文件通道，零注入
	vnc := map[string]string{}
	guacApplyFileTransfer("vnc", vnc, "/wingman-drive")
	if len(vnc) != 0 {
		t.Errorf("vnc must have no file transfer params: %v", vnc)
	}
}

func TestGuacRecordingName(t *testing.T) {
	// 常规 agentID 原样保留
	if got := guacRecordingName("agent-01", "sess1234"); got != "agent-01-sess1234.mjs" {
		t.Errorf("plain agent id: got %q", got)
	}
	// 路径注入字符全部清洗为下划线（录像名进入共享卷文件系统）
	if got := guacRecordingName("../../etc", "s"); got != ".._.._etc-s.mjs" {
		t.Errorf("path chars must be sanitized: got %q", got)
	}
	if got := guacRecordingName("a/b\\c:d", "s"); got != "a_b_c_d-s.mjs" {
		t.Errorf("separators must be sanitized: got %q", got)
	}
	// 清洗后无分隔符残留即安全（"___" 是合法文件名，无需兜底）
	if got := guacRecordingName("///", "s"); got != "___-s.mjs" {
		t.Errorf("all-sanitized id must have no separators: got %q", got)
	}
	if got := guacRecordingName("..", "s"); got != "agent-s.mjs" {
		t.Errorf("dotdot id must fall back: got %q", got)
	}
}

func TestGuacApplyRecording(t *testing.T) {
	table := map[string]string{}
	guacApplyRecording(table, "/recordings", "agent-1-sess-9.mjs")
	if table["recording-path"] != "/recordings" || table["recording-name"] != "agent-1-sess-9.mjs" {
		t.Errorf("recording paths wrong: %v", table)
	}
	if table["create-recording-path"] != "true" {
		t.Errorf("create-recording-path must be true: %v", table)
	}
	// 安全默认：永不录制按键内容（§16——录像出现明文口令是泄漏源）
	if table["recording-include-keys"] != "false" {
		t.Errorf("recording-include-keys must stay false: %v", table)
	}
	// 鼠标轨迹保留（排障关键信息）
	if table["recording-exclude-mouse"] != "false" {
		t.Errorf("recording-exclude-mouse must stay false: %v", table)
	}
}

func TestHandleTicketCreateRecordValidation(t *testing.T) {
	// 未配置录制（recordingPath/recordingDir 双空）：record=true 400，
	// 且错误信息指引两个环境变量
	_, r := newGuacTestHandler(t)
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/api/remote/tickets",
		strings.NewReader(`{"agentId":"a1","protocol":"vnc","record":true}`))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Fatalf("record without config: got %d want 400 (%s)", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), "recording not configured") {
		t.Errorf("error should explain recording config: %s", w.Body.String())
	}
}
