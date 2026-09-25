package handlers

// 会话录像检索（设计 docs/remote-gateway-guacamole-design.md §16/DG-9）。
//
// guacd 把启用了 record 票据的会话写成 .mjs（Guacamole session 格式）落在
// recording-path（guacd 容器卷）；Go server 经共享卷的 server 侧挂载点
// （WINGMAN_RECORDING_DIR）提供 list/download/delete。文件名由网关侧生成
// （{agentID}-{sessionID}.mjs），与审计记录同源可关联。
//
// 权限：列表/下载 desktop:view，删除 desktop:control（路由层
// PermissionRequired）；下载与删除各记一条审计，列表高频不审计（与 agents
// 列表同策略）。回放：.mjs 是官方格式，下载后 guacenc 可离线转 mp4；
// 浏览器内回放列为远期（session-player 非 npm 分发）。

import (
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// recordingEntry 录像条目（列表响应）。
type recordingEntry struct {
	Name       string `json:"name"`
	SizeBytes  int64  `json:"sizeBytes"`
	ModifiedAt string `json:"modifiedAt"`
}

// RecordingsHandler 录像检索：读共享卷目录，无协议交互（guacd 侧写入，
// 本 handler 只做文件系统枚举与安全校验）。
type RecordingsHandler struct {
	db           *gorm.DB
	recordingDir string
}

// NewRecordingsHandler 构造。recordingDir 为空 = 录制未配置（API 返回
// 结构化 501，与 debugger 直连模式同款契约形态）。
func NewRecordingsHandler(db *gorm.DB, recordingDir string) *RecordingsHandler {
	return &RecordingsHandler{db: db, recordingDir: strings.TrimSpace(recordingDir)}
}

// configured 录制检索是否可用。
func (rh *RecordingsHandler) configured() bool {
	return rh.recordingDir != ""
}

// recordingNotConfigured 结构化 501（沿用 debugger 先例：能力未配置返回
// 指引而非裸 stub）。
func recordingNotConfigured(c *gin.Context) {
	c.JSON(http.StatusNotImplemented, gin.H{
		"success": false,
		"error":   "session recording not configured",
		"hint":    "set WINGMAN_GUACD_RECORDING_PATH and WINGMAN_RECORDING_DIR to enable (see docs/remote-gateway-guacamole-design.md §16)",
	})
}

// resolveRecordingName 校验并解析录像名（basename 白名单：杜绝路径穿越；
// 只接受 .mjs 后缀——目录里其他文件不属于本 API 的服务范围）。显式拒绝
// 两种分隔符：server 可跨平台部署，Windows 下 '\' 同为分隔符。
func resolveRecordingName(name string) (string, bool) {
	if name == "" || name != filepath.Base(name) || name == "." || name == ".." {
		return "", false
	}
	if strings.ContainsAny(name, `/\`) {
		return "", false
	}
	if !strings.HasSuffix(name, ".mjs") {
		return "", false
	}
	return name, true
}

// HandleList GET /api/remote/recordings：按修改时间倒序列出 .mjs 录像。
// @Summary      会话录像列表
// @Description  列出 guacd 录制目录中的 .mjs 会话录像（按修改时间倒序）；录制未配置返回 501 指引
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  map[string]interface{}
// @Failure      401  {object}  ErrorResponse
// @Failure      501  {object}  ErrorResponse
// @Router       /remote/recordings [get]
func (rh *RecordingsHandler) HandleList(c *gin.Context) {
	if !rh.configured() {
		recordingNotConfigured(c)
		return
	}
	entries, err := os.ReadDir(rh.recordingDir)
	if err != nil {
		if os.IsNotExist(err) {
			// create-recording-path=true 让 guacd 首次录制时建目录；
			// 尚无任何录像时目录不存在是正常态
			c.JSON(http.StatusOK, gin.H{"success": true, "data": []recordingEntry{}})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to read recordings directory"})
		return
	}
	list := make([]recordingEntry, 0, len(entries))
	for _, e := range entries {
		if e.IsDir() || !strings.HasSuffix(e.Name(), ".mjs") {
			continue
		}
		info, err := e.Info()
		if err != nil {
			continue // 并发删除的竞态：跳过该条
		}
		list = append(list, recordingEntry{
			Name:       e.Name(),
			SizeBytes:  info.Size(),
			ModifiedAt: info.ModTime().UTC().Format(time.RFC3339),
		})
	}
	sort.Slice(list, func(i, j int) bool {
		return list[i].ModifiedAt > list[j].ModifiedAt
	})
	c.JSON(http.StatusOK, gin.H{"success": true, "data": list})
}

// HandleDownload GET /api/remote/recordings/:name/download。
// @Summary      下载会话录像
// @Description  下载指定 .mjs 录像（Guacamole session 格式，可用 guacenc 离线转 mp4）；记 desktop.recording_download 审计
// @Tags         remote
// @Produce      octet-stream
// @Security     BearerAuth
// @Param        name  path  string  true  "录像文件名（basename，.mjs 后缀）"
// @Success      200  {file}  file
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      501  {object}  ErrorResponse
// @Router       /remote/recordings/{name}/download [get]
func (rh *RecordingsHandler) HandleDownload(c *gin.Context) {
	if !rh.configured() {
		recordingNotConfigured(c)
		return
	}
	name := c.Param("name")
	base, ok := resolveRecordingName(name)
	if !ok {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid recording name"})
		return
	}
	full := filepath.Join(rh.recordingDir, base)
	if _, err := os.Stat(full); err != nil {
		if os.IsNotExist(err) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "recording not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to stat recording"})
		return
	}

	actor, _ := actorName(c)
	WriteAuditLog(rh.db, actor, "desktop.recording_download", base, map[string]any{
		"sizeBytes": fileSizeOf(full),
	})

	// FileAttachment 设置 Content-Disposition（下载语义）与正确的
	// Content-Type 推断；.mjs 按文件直出
	c.FileAttachment(full, base)
}

// HandleDelete DELETE /api/remote/recordings/:name。
// @Summary      删除会话录像
// @Description  删除指定 .mjs 录像；记 desktop.recording_delete 审计
// @Tags         remote
// @Produce      json
// @Security     BearerAuth
// @Param        name  path  string  true  "录像文件名（basename，.mjs 后缀）"
// @Success      200  {object}  map[string]interface{}
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      501  {object}  ErrorResponse
// @Router       /remote/recordings/{name} [delete]
func (rh *RecordingsHandler) HandleDelete(c *gin.Context) {
	if !rh.configured() {
		recordingNotConfigured(c)
		return
	}
	name := c.Param("name")
	base, ok := resolveRecordingName(name)
	if !ok {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid recording name"})
		return
	}
	full := filepath.Join(rh.recordingDir, base)
	if _, err := os.Stat(full); err != nil {
		if os.IsNotExist(err) {
			c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "recording not found"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to stat recording"})
		return
	}
	if err := os.Remove(full); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to delete recording"})
		return
	}

	actor, _ := actorName(c)
	WriteAuditLog(rh.db, actor, "desktop.recording_delete", base, nil)

	c.JSON(http.StatusOK, gin.H{"success": true})
}

// fileSizeOf best-effort 文件大小（审计 meta 用；失败返回 0）。
func fileSizeOf(path string) int64 {
	info, err := os.Stat(path)
	if err != nil {
		return 0
	}
	return info.Size()
}
