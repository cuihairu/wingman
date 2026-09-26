package handlers

// 远程文件操作审计上报（设计 §15.1 第二版）。SFTP 浏览器的下载/上传动作
// 本身走 guacd 对象流（网关不解析内容），本 handler 接收前端的事后上报并
// 落审计——列表高频不审计（与 agents 列表同策略），删除/重命名不存在
// （Guacamole 1.5.x 对象流只有 get/put，见 §15.1 协议边界）。
//
// 信任模型：上报是客户端 best-effort，因此会话/agent/操作者一律由服务端
// 按票据 ID 反解（GuacamoleHandler.ResolveFileOpSession），请求体里的同类
// 字段即使伪造也只进「无会话」降级行，不能污染既有会话的审计链。票据在
// WS 建连时已消费，但快照独立存续到会话关闭，上报窗口内必然可查。

import (
	"net/http"
	"strings"
	"unicode/utf8"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// fileOpReport 前端上报载荷。
type fileOpReport struct {
	// Ticket 消费过的一次性连接票据（会话反查键）
	Ticket string `json:"ticket"`
	// Action 动作：download / upload（list 不审计，收到即 400）
	Action string `json:"action"`
	// Path 远端绝对路径（含文件名；服务端只做长度/可读性约束，不解释语义）
	Path string `json:"path"`
	// Result "ok" / "fail"
	Result string `json:"result"`
	// SizeBytes 文件字节数（可选，客户端已知时上报）
	SizeBytes *int64 `json:"sizeBytes,omitempty"`
	// Attempts 实际尝试次数（重试后成功时 >1；1~5）
	Attempts int `json:"attempts,omitempty"`
	// Error 最终失败原因（result=fail 时可选上报，截断落库）
	Error string `json:"error,omitempty"`
}

// RemoteFileOpsHandler 文件操作审计上报。
type RemoteFileOpsHandler struct {
	db   *gorm.DB
	guac *GuacamoleHandler
}

// NewRemoteFileOpsHandler 构造。db/guac 任一缺失时路由层不挂载本 handler。
func NewRemoteFileOpsHandler(db *gorm.DB, guac *GuacamoleHandler) *RemoteFileOpsHandler {
	return &RemoteFileOpsHandler{db: db, guac: guac}
}

// HandleReport 接收文件操作上报并落审计。权限：路由层要求 desktop:view；
// upload 是写动作，与上传入口仅接管渲染的 UI 约束对齐，handler 内联追加
// desktop:control 校验（监看会话的上传上报一律 403）。
//
// @Summary      上报远程文件操作（审计）
// @Description  SFTP 浏览器下载/上传的事后审计上报；会话/agent/操作者由服务端按票据反解，请求体不采信同类字段；list 动作不审计直接拒绝
// @Tags         remote
// @Accept       json
// @Produce      json
// @Security     BearerAuth
// @Param        request  body  fileOpReport  true  "操作载荷（action: download/upload，result: ok/fail）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /remote/file-ops [post]
func (fh *RemoteFileOpsHandler) HandleReport(c *gin.Context) {
	var req fileOpReport
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid request body"})
		return
	}
	req.Action = strings.TrimSpace(req.Action)
	req.Result = strings.TrimSpace(req.Result)
	if req.Action != "download" && req.Action != "upload" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "action must be download or upload (list is not audited)"})
		return
	}
	if req.Result != "ok" && req.Result != "fail" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "result must be ok or fail"})
		return
	}
	path := strings.TrimSpace(req.Path)
	if path == "" || !utf8.ValidString(path) || utf8.RuneCountInString(path) > 4096 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "path is required (<=4096 chars)"})
		return
	}
	if req.SizeBytes != nil && *req.SizeBytes < 0 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "sizeBytes must be >= 0"})
		return
	}
	if req.Attempts < 0 || req.Attempts > 5 {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "attempts must be 0 (not reported) or within 1..5"})
		return
	}
	// 监看（只读动作上报）不要求 control；上传是写动作，与 §15.1 权限矩阵
	// 对齐：无 desktop:control 的上报一律拒绝，防止监看者伪造上传审计或
	// 通过上报通道探测写权限
	if req.Action == "upload" && !currentActorHasPermission(c, "desktop:control") {
		c.JSON(http.StatusForbidden, gin.H{"success": false, "error": "upload report requires desktop:control permission"})
		return
	}

	kind := "desktop.file_download"
	if req.Action == "upload" {
		kind = "desktop.file_upload"
	}
	meta := map[string]any{
		"action": req.Action,
		"result": req.Result,
	}
	// 服务端可信字段优先：票据能反解出会话则以快照为准，否则落降级行
	// （no_session 标记，报表可区分「真上报」与「不可归因」——快照存续到
	// 会话关闭，不可归因基本只发生在进程重启后）
	if snap, ok := fh.guac.ResolveFileOpSession(req.Ticket); ok {
		meta["session"] = snap.SessionID
		meta["agent"] = snap.AgentID
		meta["protocol"] = snap.Protocol
		meta["read_only"] = snap.ReadOnly
	} else {
		meta["no_session"] = true
	}
	// 客户端自报的补充字段：两个分支都保留（取证有价值），可信度低于上面
	if req.SizeBytes != nil {
		meta["sizeBytes"] = *req.SizeBytes
	}
	if req.Attempts > 0 {
		meta["attempts"] = req.Attempts
	}
	if req.Error != "" {
		meta["error"] = truncateAuditText(req.Error, 512)
	}
	actor, _ := actorName(c)
	WriteAuditLog(fh.db, actor, kind, path, meta)
	c.JSON(http.StatusOK, gin.H{"success": true})
}

// currentActorHasPermission 判断当前请求者是否持有指定权限码。admin 的
// permissions 是通配 ["*"]（middleware.PermissionRequired 注入）。
func currentActorHasPermission(c *gin.Context, code string) bool {
	raw, ok := c.Get("permissions")
	if !ok {
		return false
	}
	codes, ok := raw.([]string)
	if !ok {
		return false
	}
	for _, item := range codes {
		if item == "*" || item == code {
			return true
		}
	}
	return false
}

// truncateAuditText 审计 meta 文本截断（防失败原因里带超长错误串撑爆行）。
func truncateAuditText(s string, max int) string {
	if utf8.RuneCountInString(s) <= max {
		return s
	}
	runes := []rune(s)
	return string(runes[:max]) + "…"
}
