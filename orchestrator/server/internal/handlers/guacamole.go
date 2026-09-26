package handlers

// Guacamole 像素面网关（docs/remote-gateway-guacamole-design.md §4/DG-2/DG-6）：
// 浏览器 ↔ server 段是 WebSocket（文本帧即 Guacamole 指令流），server ↔ guacd
// 段是 TCP（Guacamole 原生协议）。数据面只做双向字节管道——不解析指令内容，
// 协议演进由 guacamole-common-js 与 guacd 自己对齐。
//
// 唯一的破例在握手阶段（协议要求，非自由发挥）：
//   - select 之后 guacd 回 args 指令（参数名名单，首段为协议版本号）；
//   - connect 指令的参数必须与该名单**按位置一一对应**（Apache Guacamole
//     协议参考「connect」节），因此网关必须解析 args 名单、按名取值、按序
//     填参。凭据只存在于网关 → guacd 的 connect 指令里，不下发浏览器。
//
// 与 cockpit 项目共用同一套端点/票据/部署约定（DG-6）：WS 路径
// /api/remote/guacamole、票据 REST /api/remote/tickets、guacd 地址经
// WINGMAN_GUACD_ADDR（默认 127.0.0.1:4822）。
//
// 出口边界：wingman 无独立 allow-list 配置，host 一律取 agent 注册表里的
// 上报 IP（票据申请时绑定）——注册表本身就是白名单，票据携带的 host 不可
// 被 WS 侧篡改（WS 只认票据内的参数）。

import (
	"bufio"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"gorm.io/gorm"
)

// 权限点（remote-gateway-guacamole-design.md P0「RBAC 两权限点」）：
// desktop:view 监看（read-only），desktop:control 接管（输入注入）。
const (
	PermDesktopView    = "desktop:view"
	PermDesktopControl = "desktop:control"
)

// guacdDialTimeout 拨 guacd 超时；guacamoleReadBuf guacd→WS 读缓冲
// （Guacamole 指令含 base64 位图 blob，单条可较大）。
const (
	guacdDialTimeout = 10 * time.Second
	guacamoleReadBuf = 256 * 1024
)

// guacamoleUpgrader 隧道专用 upgrader。Origin 语义与 /ws 先例一致：
// 非浏览器（无 Origin）放行，浏览器仅同源——防跨站 WebSocket 劫持。
var guacamoleUpgrader = websocket.Upgrader{
	ReadBufferSize:  guacamoleReadBuf,
	WriteBufferSize: guacamoleReadBuf,
	CheckOrigin: func(r *http.Request) bool {
		origin := r.Header.Get("Origin")
		if origin == "" {
			return true
		}
		host := r.Host
		if host == "" {
			return false
		}
		// 同源判定：Origin 的 authority 与 Host 一致（含端口）
		return strings.HasPrefix(origin, "http://"+host) || strings.HasPrefix(origin, "https://"+host)
	},
}

// GuacamoleHandler 像素面网关：票据 REST + WS 反代 guacd + 审计。
type GuacamoleHandler struct {
	db        *gorm.DB
	registry  *agent.Registry
	tickets   *remoteticket.Manager
	guacdAddr string
	// drivePath RDP 文件传输虚拟盘（guacd 容器内路径，设计 §15）
	drivePath string
	// recordingPath / recordingDir 会话录制（设计 §16）。两者均非空才启用
	// record=true 票据与录像检索；recordingDir 是 server 侧检索挂载点。
	recordingPath string
	recordingDir  string
	// opSessions 票据 ID → 会话快照（§15.1 文件操作审计：客户端上报只带
	// 票据，session/agent/操作者由服务端反解，不信任请求体）。
	opSessions sync.Map
}

// NewGuacamoleHandler 构造网关。guacdAddr 为空时回退 127.0.0.1:4822；
// drivePath 为空时回退 /wingman-drive（与 deployments/guacd 挂卷对齐）。
// recordingPath/recordingDir 为空 = 会话录制关闭（record 票据 400、
// 检索 API 501，设计 §16）。
func NewGuacamoleHandler(db *gorm.DB, registry *agent.Registry, tickets *remoteticket.Manager, guacdAddr, drivePath, recordingPath, recordingDir string) *GuacamoleHandler {
	if strings.TrimSpace(guacdAddr) == "" {
		guacdAddr = "127.0.0.1:4822"
	}
	if strings.TrimSpace(drivePath) == "" {
		drivePath = "/wingman-drive"
	}
	return &GuacamoleHandler{
		db:            db,
		registry:      registry,
		tickets:       tickets,
		guacdAddr:     guacdAddr,
		drivePath:     drivePath,
		recordingPath: strings.TrimSpace(recordingPath),
		recordingDir:  strings.TrimSpace(recordingDir),
	}
}

// recordingEnabled 录制链路是否配置完整（guacd 写入路径 + server 检索路径）。
func (h *GuacamoleHandler) recordingEnabled() bool {
	return h.recordingPath != "" && h.recordingDir != ""
}

// guacProtocolPort 各协议的 agent 侧默认端口（endpoint 矩阵 §5：RDP 3389、
// VNC 5900、SSH 22）。
func guacProtocolPort(protocol string) int {
	switch protocol {
	case "rdp":
		return 3389
	case "vnc":
		return 5900
	case "ssh":
		return 22
	}
	return 0
}

// guacSupportedProtocol Guacamole 承接的协议（rdp/vnc/ssh）。链路一致性由
// guacd 保证，前端协议差异被吸收（DG-2）。
func guacSupportedProtocol(protocol string) bool {
	return protocol == "rdp" || protocol == "vnc" || protocol == "ssh"
}

// ---------- Guacamole 协议编解码（照抄官方格式，见文件头注释） ----------

// guacEncode 编码一条 Guacamole 指令：长度前缀 + 逗号分隔 + 分号结尾。
// 格式照抄 guacamole-common-js 的 Tunnel.sendMessage。
// 例：guacEncode("size", "1024", "768", "96") → "4.size,4.1024,3.768,2.96;"
func guacEncode(opcode string, args ...string) string {
	var b strings.Builder
	fmt.Fprintf(&b, "%d.%s", len(opcode), opcode)
	for _, a := range args {
		fmt.Fprintf(&b, ",%d.%s", len(a), a)
	}
	b.WriteByte(';')
	return b.String()
}

// guacParseArgs 解析 guacd 对 select 的 args 响应，返回参数名名单
// （含首段协议版本名——connect 必须与名单**等长**按位对应，版本位不可省，
// guacd 对个数硬校验，不等即静默断连）。非 args 指令返回 nil。
// 例："4.args,13.VERSION_1_5_0,8.hostname,4.port;" → ["VERSION_1_5_0","hostname","port"]
func guacParseArgs(frame string) []string {
	frame = strings.TrimSuffix(frame, ";")
	if frame == "" {
		return nil
	}
	parts := strings.Split(frame, ",")
	if len(parts) == 0 || !guacExpectOpcode(parts[0], "args") {
		return nil
	}
	// parts[0] = "4.args"，parts[1] 起为参数名（首名是版本段）
	names := make([]string, 0, len(parts)-1)
	for _, p := range parts[1:] {
		dot := strings.Index(p, ".")
		if dot <= 0 {
			continue
		}
		names = append(names, p[dot+1:])
	}
	return names
}

// guacExpectOpcode 校验首段形如 "N.opcode" 且 opcode 匹配。
func guacExpectOpcode(segment, opcode string) bool {
	dot := strings.Index(segment, ".")
	if dot <= 0 {
		return false
	}
	n, err := strconv.Atoi(segment[:dot])
	if err != nil || n != len(opcode) || segment[dot+1:] != opcode {
		return false
	}
	return true
}

// guacParamTable 组装 connect 参数表（名字 → 值）。后续 buildConnectValues
// 按 guacd args 名单的顺序从中取值。凭据来自票据 params，不经浏览器二次经手。
func guacParamTable(protocol, host string, port int, params map[string]string, width, height int, readOnly bool) map[string]string {
	table := map[string]string{
		"hostname":  host,
		"port":      strconv.Itoa(port),
		"read-only": strconv.FormatBool(readOnly),
	}
	if v := params["username"]; v != "" {
		table["username"] = v
	}
	if v := params["password"]; v != "" {
		table["password"] = v
	}
	if v := params["domain"]; v != "" {
		table["domain"] = v
	}
	switch protocol {
	case "rdp":
		// 办公场景验收（设计 §10）：色深 32、安全协商 any（NLA/TLS/RDP 自适应）、
		// 忽略自签证书（内网自签常见）。
		table["security"] = "any"
		table["ignore-cert"] = "true"
		table["color-depth"] = "32"
		// 音频显式禁用：官方 guacd 镜像（1.5.5/1.6.0，Debian/Alpine 均实测）
		// 的 libguac 未链接音频编码器（opus/flac），RDP 音频流（RDPSND）协商
		// 走 guac_audio_assign_encoder 时返回 NULL 直接 SIGSEGV（e2e 实测崩溃，
		// 栈：assign_encoder ← add_user ← sync_pending_user_audio）。禁音频
		// 绕开该路径；音频透传待上游修复后在控制 UI 阶段再评估。
		table["disable-audio"] = "true"
		// 图形扩展（rdpgfx/RemoteFX）同样显式关闭：xrdp 等 VNC 后端服务器不
		// 承接 GFX 动态通道，FreeRDP 加载 rdpgfx 后等待 GFX 帧会无限挂起
		// （e2e 实测：CLIPRDR 已连通、认证通过，但 40s 无任何像素指令）。
		// 常规位图管线（BGRX32）对办公桌面足够，真实 Windows 服务器同样兼容。
		table["disable-gfx"] = "true"
		if width > 0 && height > 0 {
			table["width"] = strconv.Itoa(width)
			table["height"] = strconv.Itoa(height)
		}
	case "vnc":
		table["color-depth"] = "32"
	}
	return table
}

// guacApplyFileTransfer 文件传输通道参数（设计 §15/DG-8）：SSH 走 SFTP
// 子系统（恒开——通道存在不代表能绕过远端文件系统权限，鉴权仍在 SSH 凭证
// 层）；RDP 走设备重定向虚拟盘（drive-path 挂在 guacd 容器卷，上行=写入
// 虚拟盘、下行=会话内拷入虚拟盘触发 guacd 推 file 流）；VNC 无文件通道
// （RFB 协议层不可能），不注入参数。
func guacApplyFileTransfer(protocol string, table map[string]string, drivePath string) {
	switch protocol {
	case "ssh":
		table["enable-sftp"] = "true"
	case "rdp":
		table["enable-drive"] = "true"
		table["drive-path"] = drivePath
	}
}

// guacRecordingName 录像文件名：{agentID}-{sessionID}.mjs。agentID 进入
// 文件名前做字符白名单清洗（录像目录是共享卷，杜绝路径注入）。
func guacRecordingName(agentID, sessionID string) string {
	safe := strings.Map(func(r rune) rune {
		switch {
		case r >= 'a' && r <= 'z', r >= 'A' && r <= 'Z', r >= '0' && r <= '9',
			r == '.', r == '_', r == '-':
			return r
		}
		return '_'
	}, agentID)
	if safe == "" || safe == "." || safe == ".." {
		safe = "agent"
	}
	return safe + "-" + sessionID + ".mjs"
}

// guacApplyRecording 会话录制参数（设计 §16/DG-9）。安全默认：
// recording-include-keys=false——**永不**把按键内容写进录像（录像出现明文
// 口令是审计资产变泄漏源）；鼠标轨迹保留（排障关键信息）。
// create-recording-path=true 免去部署侧预建目录。
func guacApplyRecording(table map[string]string, path, name string) {
	table["recording-path"] = path
	table["recording-name"] = name
	table["create-recording-path"] = "true"
	table["recording-include-keys"] = "false"
	table["recording-exclude-mouse"] = "false"
}

// guacApplyVersionArg 版本协商占位：名单首段若为协议版本名（VERSION_x_y_z），
// 把回显值写进参数表，保证 connect 与 args 名单等长且首参回应版本串。
func guacApplyVersionArg(argNames []string, table map[string]string) {
	if len(argNames) > 0 && strings.HasPrefix(argNames[0], "VERSION_") {
		table[argNames[0]] = argNames[0]
	}
}

// buildConnectValues 按 guacd args 名单顺序从参数表取值组装 connect 参数
// （协议要求与名单等长按位对应；名单里我们没有的参数给空串）。
func buildConnectValues(argNames []string, table map[string]string) []string {
	values := make([]string, 0, len(argNames))
	for _, name := range argNames {
		values = append(values, table[name])
	}
	return values
}

// guacIsInternal 判定是否为隧道内部控制指令（opcode 长度为 0，即
// Guacamole.Tunnel.INTERNAL_DATA_OPCODE，如 ping）。只读首段长度前缀，
// 不解析参数内容。
func guacIsInternal(frame []byte) bool {
	dot := indexByte(frame, '.')
	if dot <= 0 {
		return false
	}
	n := 0
	for i := 0; i < dot; i++ {
		c := frame[i]
		if c < '0' || c > '9' {
			return false
		}
		n = n*10 + int(c-'0')
	}
	return n == 0
}

// indexByte 字节切片内查找（避免 string 转换开销）。
func indexByte(b []byte, c byte) int {
	for i, x := range b {
		if x == c {
			return i
		}
	}
	return -1
}

// ---------- 票据 REST（POST /api/remote/tickets） ----------

// ticketRequest 票据申请体。
type ticketRequest struct {
	AgentID  string `json:"agentId"`
	Protocol string `json:"protocol"`
	Username string `json:"username"`
	Password string `json:"password"`
	Domain   string `json:"domain"`
	Width    int    `json:"width"`
	Height   int    `json:"height"`
	ReadOnly bool   `json:"readOnly"`
	Port     int    `json:"port"`   // 0 = 协议默认端口
	Record   bool   `json:"record"` // 会话录制（需服务端已配置录制，设计 §16）
}

// HandleTicketCreate 签发一次性连接票据（5 分钟有效）。挂 /api 组
// （AuthRequired）+ desktop:view/desktop:control 任一权限；
// readOnly=false（接管）额外要求 desktop:control。
// host 一律取 agent 注册表上报 IP（注册表即白名单，见文件头出口边界注释）。
// @Summary      签发远程桌面连接票据
// @Description  为指定 agent 签发一次性 guacamole 连接票据（5 分钟有效，单次使用）；host 取注册表上报 IP；接管模式额外要求 desktop:control；record=true 需服务端已配置录制双路径
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  ticketRequest  true  "连接参数（protocol: rdp/vnc/ssh）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router       /remote/tickets [post]
func (h *GuacamoleHandler) HandleTicketCreate(c *gin.Context) {
	var req ticketRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request body"})
		return
	}
	req.Protocol = strings.ToLower(strings.TrimSpace(req.Protocol))
	req.AgentID = strings.TrimSpace(req.AgentID)
	if !guacSupportedProtocol(req.Protocol) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "protocol must be rdp/vnc/ssh"})
		return
	}
	// 录制需服务端双路径配置（guacd 写入 + server 检索，设计 §16），
	// 未配置时在票据申请即拒绝，不留给握手期
	if req.Record && !h.recordingEnabled() {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "session recording not configured (set WINGMAN_GUACD_RECORDING_PATH and WINGMAN_RECORDING_DIR)"})
		return
	}

	userIDVal, _ := c.Get("user_id")
	userID, _ := userIDVal.(uint)
	usernameVal, _ := c.Get("username")
	username, _ := usernameVal.(string)

	// 接管（可注入输入）比监看多一档权限
	if !req.ReadOnly {
		ok, err := rbac.HasPermission(h.db, userID, PermDesktopControl)
		if err != nil || !ok {
			c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "desktop:control permission required for control mode"})
			return
		}
	}

	// agent 在线校验 + endpoint 绑定（地址解析失败在票据申请时就拒绝，
	// 不留给握手期——设计 §4.4）
	info, ok := h.registry.Get(req.AgentID)
	if !ok || info == nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "agent not found or offline"})
		return
	}
	if strings.TrimSpace(info.IP) == "" {
		c.JSON(http.StatusUnprocessableEntity, gin.H{"success": false, "error": "agent has no reported LAN address"})
		return
	}
	port := req.Port
	if port <= 0 {
		port = guacProtocolPort(req.Protocol)
	}
	if port <= 0 || port > 65535 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid port"})
		return
	}

	params := map[string]string{
		"agent_id":  req.AgentID,
		"host":      info.IP,
		"port":      strconv.Itoa(port),
		"protocol":  req.Protocol,
		"username":  req.Username,
		"password":  req.Password,
		"domain":    req.Domain,
		"read_only": strconv.FormatBool(req.ReadOnly),
		"record":    strconv.FormatBool(req.Record),
	}
	if req.Width > 0 {
		params["width"] = strconv.Itoa(req.Width)
	}
	if req.Height > 0 {
		params["height"] = strconv.Itoa(req.Height)
	}

	tk, err := h.tickets.GenerateTicket(userID, username, params)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to issue ticket"})
		return
	}

	WriteAuditLog(h.db, username, "desktop.ticket", req.AgentID, map[string]any{
		"protocol": req.Protocol,
		"host":     info.IP,
		"port":     port,
		"readOnly": req.ReadOnly,
		"record":   req.Record,
	})

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"data": gin.H{
			"ticket":    tk.ID,
			"expiresAt": tk.ExpiresAt.UTC().Format(time.RFC3339),
		},
	})
}

// ---------- WS 隧道（GET /api/remote/guacamole） ----------

// GuacamoleSession 一条桌面会话：浏览器 WS ↔ guacd TCP 双跳管道。
type GuacamoleSession struct {
	ID       string
	Username string
	Protocol string
	AgentID  string
	Host     string
	Port     int
	ReadOnly bool
	// Record 会话是否在录制（close 时写进审计，供报表关联录像）
	Record bool
	// RecordingName 录像文件名（{agentID}-{sessionID}.mjs）；未录制为空
	RecordingName string
	// TicketID 消费的一次性票据（§15.1 文件操作审计反查键；closeSession 清除）
	TicketID string
	ClientWS *websocket.Conn
	guacd    net.Conn
	guacdRd  *bufio.Reader
	Created  time.Time
	writeMu  sync.Mutex
	done     chan struct{}
	once     sync.Once
}

// guacTicketFromRequest 票据双通道提取：URL query ?ticket=（首选，
// guacamole-common-js 的 WebSocketTunnel 把 connect(data) 拼进 URL query 且
// 硬编码 subprotocol "guacamole"，无法自定义 subprotocol 传票据——上游约束）；
// Sec-WebSocket-Protocol[0]（兼容位，与 terminal/desktop 同款形状）。
func guacTicketFromRequest(r *http.Request) string {
	if v := r.URL.Query().Get("ticket"); v != "" {
		return v
	}
	if protocols := r.Header.Values("Sec-WebSocket-Protocol"); len(protocols) > 0 {
		if v := strings.TrimSpace(protocols[0]); v != "" {
			return v
		}
	}
	return ""
}

// HandleWS 浏览器 ↔ server 段。票据即凭证（WS 无法自定义 header，不挂
// AuthRequired，与 /ws 先例一致）；一次性消费，断线重连须重新申请。
func (h *GuacamoleHandler) HandleWS(c *gin.Context) {
	ticketID := guacTicketFromRequest(c.Request)
	if ticketID == "" {
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "missing ticket"})
		return
	}
	tk, err := h.tickets.ValidateTicket(ticketID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"success": false, "error": "invalid or expired ticket"})
		return
	}

	host := tk.Params["host"]
	agentID := tk.Params["agent_id"]
	protocol := tk.Params["protocol"]
	readOnly, _ := strconv.ParseBool(tk.Params["read_only"])
	record, _ := strconv.ParseBool(tk.Params["record"])
	port, _ := strconv.Atoi(tk.Params["port"])
	width, _ := strconv.Atoi(tk.Params["width"])
	height, _ := strconv.Atoi(tk.Params["height"])
	if host == "" || agentID == "" || !guacSupportedProtocol(protocol) || port <= 0 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "missing connection parameters in ticket"})
		return
	}
	// 票据签发后配置被摘除的边缘场景（如运行中改环境变量重启前）：
	// record 票据降级为不录制继续连接，而不是拒绝——连接可用性优先，
	// 录制缺失记日志可查
	if record && !h.recordingEnabled() {
		log.Printf("Guacamole: ticket requests recording but server recording disabled; continuing without recording")
		record = false
	}

	// 会话 ID 在任何可能失败的步骤之前生成：失败的尝试同样要能在审计里
	// 被唯一标识（否则报表只能看到「有一次失败」而无法定位是哪次）。
	sessionID := newGuacSessionID()
	startedAt := time.Now()

	// 拨 guacd TCP（不暴露 guacd 端口给浏览器——网关反代，设计 §4.1）
	guacdConn, err := net.DialTimeout("tcp", h.guacdAddr, guacdDialTimeout)
	if err != nil {
		log.Printf("Guacamole: dial guacd %s failed: %v", h.guacdAddr, err)
		WriteAuditLog(h.db, tk.Username, "desktop.connect_fail", agentID, map[string]any{
			"protocol": protocol, "host": host, "port": port, "reason": "guacd unreachable",
		})
		// 会话审计也要记 failed 行：报表要能回答「拨不通 guacd 发生过
		// 多少次」——只在 AuditLog 里的话聚合得翻 JSON meta。
		// 会话 ID 在拨号前就生成，好让失败行也能与后续重试关联。
		RecordRemoteSession(h.db, models.RemoteSessionAudit{
			SessionID:  sessionID,
			AgentID:    agentID,
			Operator:   tk.Username,
			Protocol:   protocol,
			Host:       host,
			Port:       port,
			ReadOnly:   readOnly,
			Record:     record,
			Status:     models.RemoteSessionStatusFailed,
			FailReason: "guacd unreachable",
			StartedAt:  startedAt,
		})
		c.JSON(http.StatusBadGateway, gin.H{"success": false, "error": "guacamole daemon unavailable"})
		return
	}

	conn, err := guacamoleUpgrader.Upgrade(c.Writer, c.Request, nil)
	if err != nil {
		log.Printf("Guacamole: websocket upgrade failed: %v", err)
		_ = guacdConn.Close()
		return
	}

	// 录像文件名在会话建立时就定下来（与审计同源唯一），close 时写入会话
	// 审计，报表因此能与录像检索 API 的 name 直接对上
	recordingName := ""
	if record {
		recordingName = guacRecordingName(agentID, sessionID)
	}

	session := &GuacamoleSession{
		ID:            sessionID,
		Username:      tk.Username,
		Protocol:      protocol,
		AgentID:       agentID,
		Host:          host,
		Port:          port,
		ReadOnly:      readOnly,
		Record:        record,
		RecordingName: recordingName,
		TicketID:      ticketID,
		ClientWS:      conn,
		guacd:         guacdConn,
		Created:       startedAt,
		done:          make(chan struct{}),
	}

	// tunnel UUID 先发浏览器（INTERNAL_DATA 空 opcode 单元素指令）：common-js
	// 首条指令若为内部指令单元素则 setUUID。这是隧道层指令，不进 guacd。
	session.writeWS([]byte(guacEncode("", session.ID)))

	// 握手：select → 消费并解析 guacd 的 args 名单 → size + connect（按名单
	// 顺序填参，协议要求按位对应——见文件头注释）
	hsReader := bufio.NewReaderSize(guacdConn, guacamoleReadBuf)
	if _, err := guacdConn.Write([]byte(guacEncode("select", protocol))); err != nil {
		log.Printf("Guacamole: select write failed: %v", err)
		h.closeSession(session)
		return
	}
	argsFrame, err := hsReader.ReadString(';')
	if err != nil {
		log.Printf("Guacamole: read args response failed: %v", err)
		h.closeSession(session)
		return
	}
	if width <= 0 {
		width = 1280
	}
	if height <= 0 {
		height = 800
	}
	argNames := guacParseArgs(argsFrame)
	table := guacParamTable(protocol, host, port, tk.Params, width, height, readOnly)
	// 文件传输通道（§15）与会话录制（§16）参数：都是按名注入，args 名单里
	// 没有的协议（如 vnc 的文件参数）自然落空，不影响按位对应
	guacApplyFileTransfer(protocol, table, h.drivePath)
	if record {
		guacApplyRecording(table, h.recordingPath, recordingName)
	}
	// 版本协商：args 首段是 guacd 声明的协议版本（如 VERSION_1_5_0），connect
	// 首参必须回应该版本串占住第一参数位（官方 common-js 即回显同串；回应
	// 不得高于服务器声明，回显天然满足）。不回应则参数整体少一位，guacd 拒连。
	guacApplyVersionArg(argNames, table)
	connectArgs := buildConnectValues(argNames, table)
	handshake := guacEncode("size", strconv.Itoa(width), strconv.Itoa(height), "96") +
		guacEncode("connect", connectArgs...)
	if _, err := guacdConn.Write([]byte(handshake)); err != nil {
		log.Printf("Guacamole: handshake write failed: %v", err)
		h.closeSession(session)
		return
	}
	session.guacdRd = hsReader

	WriteAuditLog(h.db, tk.Username, "desktop.connect", agentID, map[string]any{
		"protocol": protocol, "host": host, "port": port,
		"session": session.ID, "readOnly": readOnly, "record": record,
	})
	log.Printf("Guacamole session created: %s -> %s:%d (%s, read-only=%v)", session.ID, host, port, protocol, readOnly)

	// 快照在会话建立后入库（§15.1 文件操作审计的服务端可信来源；closeSession 删除）
	h.opSessions.Store(ticketID, GuacFileOpSession{
		SessionID: sessionID,
		AgentID:   agentID,
		Protocol:  protocol,
		Username:  tk.Username,
		ReadOnly:  readOnly,
	})

	go session.guacdToWS()
	session.wsToGuacd()

	h.closeSession(session)
}

// wsToGuacd 浏览器 → guacd：文本帧直写 TCP。隧道内部指令（ping）原样回显
// 不转发 guacd——common-js 的 WebSocketTunnel 周期发 ping 期望 identical
// 响应，不回显则 15s receiveTimeout 判超时关隧道。
func (gs *GuacamoleSession) wsToGuacd() {
	defer close(gs.done)
	for {
		_, data, err := gs.ClientWS.ReadMessage()
		if err != nil {
			return
		}
		if guacIsInternal(data) {
			gs.writeWS(data)
			continue
		}
		if _, err := gs.guacd.Write(data); err != nil {
			return
		}
	}
}

// guacdToWS guacd → 浏览器：按指令边界（分号）切分逐帧写文本帧，
// 保证 common-js 的 Tunnel 每帧一条完整指令；不解析参数内容。
func (gs *GuacamoleSession) guacdToWS() {
	reader := gs.guacdRd
	if reader == nil {
		reader = bufio.NewReaderSize(gs.guacd, guacamoleReadBuf)
	}
	var pending []byte
	buf := make([]byte, 32*1024)
	for {
		n, err := reader.Read(buf)
		if n > 0 {
			pending = append(pending, buf[:n]...)
			for {
				idx := indexByte(pending, ';')
				if idx < 0 {
					break
				}
				frame := pending[:idx+1]
				pending = pending[idx+1:]
				gs.writeWS(frame)
			}
		}
		if err != nil {
			if err != io.EOF {
				log.Printf("Guacamole: read from guacd failed: %v", err)
			}
			return
		}
	}
}

// writeWS 线程安全写 WS 文本帧（gorilla 单写者约束）。
func (gs *GuacamoleSession) writeWS(payload []byte) {
	gs.writeMu.Lock()
	defer gs.writeMu.Unlock()
	_ = gs.ClientWS.WriteMessage(websocket.TextMessage, payload)
}

// closeSession 幂等关闭：WS + guacd TCP + 审计结束（含时长；不含口令）。
// 关闭时才落会话审计终态行（设计 §11 报表）：进行中不落行，避免报表把
// 未结束会话算进时长，崩溃后也不会留永远不闭合的脏行。
func (h *GuacamoleHandler) closeSession(gs *GuacamoleSession) {
	gs.once.Do(func() {
		_ = gs.ClientWS.Close()
		_ = gs.guacd.Close()
		if gs.TicketID != "" {
			h.opSessions.Delete(gs.TicketID)
		}
		endedAt := time.Now()
		durationMs := endedAt.Sub(gs.Created).Milliseconds()
		log.Printf("Guacamole session closed: %s", gs.ID)
		WriteAuditLog(h.db, gs.Username, "desktop.close", gs.AgentID, map[string]any{
			"protocol": gs.Protocol, "host": gs.Host, "port": gs.Port,
			"session":    gs.ID,
			"durationMs": durationMs,
		})
		RecordRemoteSession(h.db, models.RemoteSessionAudit{
			SessionID:     gs.ID,
			AgentID:       gs.AgentID,
			Operator:      gs.Username,
			Protocol:      gs.Protocol,
			Host:          gs.Host,
			Port:          gs.Port,
			ReadOnly:      gs.ReadOnly,
			Record:        gs.Record,
			RecordingName: gs.RecordingName,
			Status:        models.RemoteSessionStatusClosed,
			StartedAt:     gs.Created,
			EndedAt:       &endedAt,
			DurationMs:    durationMs,
		})
	})
}

// newGuacSessionID 会话 ID（crypto/rand 16 字节 hex；不引 uuid 依赖）。
func newGuacSessionID() string {
	buf := make([]byte, 16)
	if _, err := rand.Read(buf); err != nil {
		return fmt.Sprintf("sess-%d", time.Now().UnixNano())
	}
	return hex.EncodeToString(buf)
}

// GuacFileOpSession 文件操作审计的会话快照（§15.1）：客户端上报只带票据 ID，
// 服务端由此反解出可信的会话/agent/操作者，请求体里的同类字段一律不采信。
type GuacFileOpSession struct {
	SessionID string
	AgentID   string
	Protocol  string
	Username  string
	ReadOnly  bool
}

// ResolveFileOpSession 按票据 ID 反解会话快照。票据一次性、WS 建连即消费，
// 但快照独立存续到会话关闭——上报发生在连接期间，正常必然查到；查不到
// （进程重启/会话已关）返回 false，调用方按无会话落审计（降级不拒绝）。
func (h *GuacamoleHandler) ResolveFileOpSession(ticketID string) (GuacFileOpSession, bool) {
	if v, ok := h.opSessions.Load(ticketID); ok {
		s, _ := v.(GuacFileOpSession)
		return s, true
	}
	return GuacFileOpSession{}, false
}
