package handlers

// 文件操作审计上报测试（设计 §15.1 第二版）：票据→会话快照的全链路生命周期
// （WS 建连入库 / 会话关闭清除）、上报载荷校验、监看/接管的权限矩阵在审计
// 通道的落点（download 要 view、upload 内联追加 control）、未知票据的降级行。

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
	"unicode/utf8"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/rbac"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/remoteticket"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"gorm.io/gorm"
)

// asPermissions 注入指定权限码的测试身份（复刻 middleware.PermissionRequired
// 的 c.Set("permissions", ...) 契约；user_id/username/role 三键齐全才能过
// middleware.GetCurrentUser 的类型断言，actorName 依赖它）。
func asPermissions(codes ...string) gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Set("user_id", uint(7))
		c.Set("username", "admin")
		c.Set("role", "admin")
		if len(codes) > 0 {
			c.Set("permissions", codes)
		}
		c.Next()
	}
}

// newFileOpsEnv 完整链路环境：mock guacd + 网关 + 票据/WS 路由，返回网关
// 与 DB 供快照断言。上报路由由各用例按所需权限自行注册（newFileOpsReportRouter）。
func newFileOpsEnv(t *testing.T, guacdAddr string) (*guacMockEnv, *GuacamoleHandler, *gorm.DB) {
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
	h := NewGuacamoleHandler(db, reg, tickets, guacdAddr, "", "", "")

	r := gin.New()
	api := r.Group("/api", asAdmin(admin.ID))
	api.POST("/remote/tickets", h.HandleTicketCreate)
	r.GET("/api/remote/guacamole", h.HandleWS)
	srv := httptest.NewServer(r)
	t.Cleanup(srv.Close)

	return &guacMockEnv{srv: srv, agentID: "guac-agent"}, h, db
}

// newFileOpsReportRouter 按权限码装配上报路由。
func newFileOpsReportRouter(db *gorm.DB, h *GuacamoleHandler, perms []string) *gin.Engine {
	r := gin.New()
	r.POST("/api/remote/file-ops", asPermissions(perms...), NewRemoteFileOpsHandler(db, h).HandleReport)
	return r
}

// postFileOp 向上报路由发一条载荷，返回响应。
func postFileOp(t *testing.T, r *gin.Engine, body string) *httptest.ResponseRecorder {
	t.Helper()
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodPost, "/api/remote/file-ops", strings.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	r.ServeHTTP(w, req)
	return w
}

// auditFileOpRow 取指定 kind 的审计行（断言恰一行）。
func auditFileOpRow(t *testing.T, db *gorm.DB, kind string) models.AuditLog {
	t.Helper()
	var rows []models.AuditLog
	if err := db.Where("kind = ?", kind).Find(&rows).Error; err != nil {
		t.Fatalf("query audit rows: %v", err)
	}
	if len(rows) != 1 {
		t.Fatalf("audit rows for %s: got %d want 1", kind, len(rows))
	}
	return rows[0]
}

// auditMeta 解析审计行 meta JSON。
func auditMeta(t *testing.T, row models.AuditLog) map[string]any {
	t.Helper()
	var meta map[string]any
	if err := json.Unmarshal([]byte(row.Meta), &meta); err != nil {
		t.Fatalf("decode audit meta %q: %v", row.Meta, err)
	}
	return meta
}

// fileOpsWaitFor 轮询等待异步会话生命周期事件（入库/清理在读泵 goroutine 里
// 跑）。10s 上限：共享机器负载高时 goroutine 调度可能显著慢于常规。
func fileOpsWaitFor(t *testing.T, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(10 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(20 * time.Millisecond)
	}
	t.Fatalf("condition not met in time: %s", what)
}

// TestFileOpSessionSnapshotLifecycle 全链路：WS 建连后票据可反解出会话快照，
// 会话关闭后快照清除（上报窗口与会话生命周期一致，不留可复用的归因键）。
func TestFileOpSessionSnapshotLifecycle(t *testing.T) {
	m := startMockGuacd(t, guacRealSSHArgs)
	env, h, _ := newFileOpsEnv(t, m.ln.Addr().String())

	ticket := env.requestTicket(t, `{"agentId":"guac-agent","protocol":"ssh","username":"u","password":"p"}`)
	if _, ok := h.ResolveFileOpSession(ticket); ok {
		t.Fatalf("snapshot must not exist before WS connect (stored only after session established)")
	}

	// 不能用 dialAndWaitHandshake：它在 defer 里关 WS，而快照的存续窗口依赖
	// 客户端保持连接——关早了 [Store, Delete] 窗口会从轮询眼下溜走
	dialer := websocket.Dialer{HandshakeTimeout: 5 * time.Second}
	wsURL := "ws" + strings.TrimPrefix(env.srv.URL, "http") + "/api/remote/guacamole?ticket=" + ticket
	conn, _, err := dialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("ws dial: %v", err)
	}
	_ = conn.SetReadDeadline(time.Now().Add(5 * time.Second))
	_, first, err := conn.ReadMessage()
	if err != nil {
		t.Fatalf("read uuid frame: %v", err)
	}
	if !strings.HasPrefix(string(first), "0.,") {
		t.Fatalf("first frame should be tunnel uuid, got %q", string(first))
	}

	// 快照入库在 desktop.connect 审计之后——轮询等落点，不做即时断言
	fileOpsWaitFor(t, "snapshot exists while session open", func() bool {
		_, ok := h.ResolveFileOpSession(ticket)
		return ok
	})
	snap, _ := h.ResolveFileOpSession(ticket)
	if snap.SessionID == "" || snap.AgentID != "guac-agent" || snap.Protocol != "ssh" || snap.Username != "admin" {
		t.Errorf("snapshot fields wrong: %+v", snap)
	}
	if snap.ReadOnly {
		t.Errorf("snapshot ReadOnly should follow ticket (absent=false), got true")
	}

	// 客户端断开 → 读泵退出 → closeSession 删快照
	_ = conn.Close()
	fileOpsWaitFor(t, "snapshot cleanup after session close", func() bool {
		_, ok := h.ResolveFileOpSession(ticket)
		return !ok
	})
}

// TestFileOpsReportValidation 载荷校验：坏 JSON、非法 action（含 list 不审计）、
// 非法 result、空/超长 path、负 sizeBytes、越界 attempts 均 400 且不落审计。
func TestFileOpsReportValidation(t *testing.T) {
	_, h, db := newFileOpsEnv(t, "127.0.0.1:1") // guacd 不可达：本组用例不走 WS
	r := newFileOpsReportRouter(db, h, []string{"desktop:view"})
	longPath := strings.Repeat("好", 4097)

	for _, tc := range []struct{ name, body string }{
		{"broken json", `{`},
		{"empty action", `{"action":"","path":"/a","result":"ok"}`},
		{"list is not audited", `{"action":"list","path":"/a","result":"ok"}`},
		{"unknown action", `{"action":"rename","path":"/a","result":"ok"}`},
		{"bad result", `{"action":"download","path":"/a","result":"maybe"}`},
		{"empty path", `{"action":"download","path":"","result":"ok"}`},
		{"oversize path", fmt.Sprintf(`{"action":"download","path":%q,"result":"ok"}`, longPath)},
		{"negative size", `{"action":"download","path":"/a","result":"ok","sizeBytes":-1}`},
		{"attempts out of range", `{"action":"download","path":"/a","result":"ok","attempts":9}`},
	} {
		w := postFileOp(t, r, tc.body)
		if w.Code != http.StatusBadRequest {
			t.Errorf("%s: got %d want 400 (%s)", tc.name, w.Code, w.Body.String())
		}
	}
	var count int64
	db.Table("audit_logs").Count(&count)
	if count != 0 {
		t.Errorf("rejected reports must not be audited, got %d rows", count)
	}
}

// TestFileOpsDownloadReportHappyPathAudit view 身份上报下载成功：200 + 审计
// 行，meta 会话/agent/协议取服务端快照而非请求体。
func TestFileOpsDownloadReportHappyPathAudit(t *testing.T) {
	_, h, db := newFileOpsEnv(t, "127.0.0.1:1")
	h.opSessions.Store("tk-snap", GuacFileOpSession{
		SessionID: "sess-1", AgentID: "agent-7", Protocol: "ssh", Username: "admin", ReadOnly: false,
	})
	r := newFileOpsReportRouter(db, h, []string{"desktop:view"})

	w := postFileOp(t, r, `{"ticket":"tk-snap","action":"download","path":"/var/log/app.log","result":"ok","sizeBytes":1234,"attempts":2}`)
	if w.Code != http.StatusOK {
		t.Fatalf("download report: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	row := auditFileOpRow(t, db, "desktop.file_download")
	if row.Actor != "admin" || row.Target != "/var/log/app.log" {
		t.Errorf("actor/target wrong: %+v", row)
	}
	meta := auditMeta(t, row)
	if meta["session"] != "sess-1" || meta["agent"] != "agent-7" || meta["protocol"] != "ssh" {
		t.Errorf("meta must carry server-resolved session snapshot: %v", meta)
	}
	if meta["read_only"] != false || meta["result"] != "ok" || meta["attempts"] != float64(2) || meta["sizeBytes"] != float64(1234) {
		t.Errorf("meta extras wrong: %v", meta)
	}
}

// TestFileOpsUploadReportRequiresControl 权限矩阵在审计通道的落点：
// upload 上报要 desktop:control（view-only 一律 403 且不落审计），
// download 上报 view 即可。
func TestFileOpsUploadReportRequiresControl(t *testing.T) {
	_, h, db := newFileOpsEnv(t, "127.0.0.1:1")
	h.opSessions.Store("tk-snap", GuacFileOpSession{
		SessionID: "sess-1", AgentID: "agent-7", Protocol: "ssh", Username: "admin", ReadOnly: true,
	})
	viewRouter := newFileOpsReportRouter(db, h, []string{"desktop:view"})
	ctrlRouter := newFileOpsReportRouter(db, h, []string{"desktop:control"})
	body := `{"ticket":"tk-snap","action":"upload","path":"/tmp/a.bin","result":"ok","sizeBytes":9}`

	if w := postFileOp(t, viewRouter, body); w.Code != http.StatusForbidden {
		t.Errorf("view-only upload report: got %d want 403 (%s)", w.Code, w.Body.String())
	}
	var count int64
	db.Table("audit_logs").Count(&count)
	if count != 0 {
		t.Fatalf("rejected upload report must not be audited, got %d rows", count)
	}

	if w := postFileOp(t, ctrlRouter, body); w.Code != http.StatusOK {
		t.Fatalf("control upload report: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	row := auditFileOpRow(t, db, "desktop.file_upload")
	meta := auditMeta(t, row)
	if meta["read_only"] != true {
		t.Errorf("meta read_only should follow snapshot: %v", meta)
	}
}

// TestFileOpsReportUnknownTicketFallsBackToNoSession 票据反解失败（进程重启/
// 会话已关）落 no_session 降级行而非拒绝——上报本身仍值得留痕。
func TestFileOpsReportUnknownTicketFallsBackToNoSession(t *testing.T) {
	_, h, db := newFileOpsEnv(t, "127.0.0.1:1")
	r := newFileOpsReportRouter(db, h, nil) // 无 permissions 注入：download 不靠 handler 内联校验

	w := postFileOp(t, r, `{"ticket":"tk-gone","action":"download","path":"/a.log","result":"fail","error":"sftp gone"}`)
	if w.Code != http.StatusOK {
		t.Fatalf("fallback report: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	meta := auditMeta(t, auditFileOpRow(t, db, "desktop.file_download"))
	if meta["no_session"] != true {
		t.Errorf("fallback row must be flagged no_session: %v", meta)
	}
	if _, has := meta["session"]; has {
		t.Errorf("fallback row must not carry session: %v", meta)
	}
	if meta["error"] != "sftp gone" {
		t.Errorf("client-reported error should be kept: %v", meta)
	}
}

// TestFileOpsReportErrorTextTruncated 失败原因超长时截断落库。
func TestFileOpsReportErrorTextTruncated(t *testing.T) {
	_, h, db := newFileOpsEnv(t, "127.0.0.1:1")
	r := newFileOpsReportRouter(db, h, []string{"desktop:view"})
	longErr := strings.Repeat("x", 600)

	w := postFileOp(t, r, fmt.Sprintf(`{"action":"download","path":"/a","result":"fail","error":%q}`, longErr))
	if w.Code != http.StatusOK {
		t.Fatalf("report: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	meta := auditMeta(t, auditFileOpRow(t, db, "desktop.file_download"))
	stored, _ := meta["error"].(string)
	if utf8.RuneCountInString(stored) != 513 || !strings.HasSuffix(stored, "…") {
		t.Errorf("error should be truncated to 512 runes + ellipsis, got %d runes: %q", utf8.RuneCountInString(stored), stored)
	}
}

// TestCurrentActorHasPermission 通配与精确码匹配；缺 permissions/类型不符拒绝。
func TestCurrentActorHasPermission(t *testing.T) {
	build := func(perms any) *gin.Context {
		c, _ := gin.CreateTestContext(httptest.NewRecorder())
		if perms != nil {
			c.Set("permissions", perms)
		}
		return c
	}
	if !currentActorHasPermission(build([]string{"*"}), "desktop:control") {
		t.Errorf("admin wildcard must match any code")
	}
	if !currentActorHasPermission(build([]string{"desktop:view", "desktop:control"}), "desktop:control") {
		t.Errorf("exact code must match")
	}
	if currentActorHasPermission(build([]string{"desktop:view"}), "desktop:control") {
		t.Errorf("missing code must not match")
	}
	if currentActorHasPermission(build(nil), "desktop:control") {
		t.Errorf("absent permissions must not match")
	}
	if currentActorHasPermission(build("desktop:control"), "desktop:control") {
		t.Errorf("wrong type must not match")
	}
}
