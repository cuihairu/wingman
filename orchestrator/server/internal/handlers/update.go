package handlers

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/middleware"
	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// UpdateHandler 自动更新发布与 manifest（ROADMAP M8.1）。
// 发布写接口走 admin 路由组；latest/download 公开只读（仅版本号与校验值，
// 无敏感数据），供 runtime/客户端 outbound 轮询（架构约束：runtime 只出站
// 连 Go server，不经 JWT 的 transport 通道）。
type UpdateHandler struct {
	db  *gorm.DB
	dir string // 制品根目录（config.UpdatesDir）
}

func NewUpdateHandler(db *gorm.DB, dir string) *UpdateHandler {
	return &UpdateHandler{db: db, dir: dir}
}

var updateVersionRe = regexp.MustCompile(`^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$`)
var updateNameRe = regexp.MustCompile(`^[A-Za-z0-9][A-Za-z0-9._-]{0,254}$`)

// compareUpdateVersions 语义化版本比较（与 C++ 客户端 lib/wingman
// update.cpp compareVersions 同口径）：先按 x.y.z 数值分量比较；三元组
// 相同则正式版本 > 预发布（1.2.3 > 1.2.3-beta.1）；都相同按后缀字典序。
func compareUpdateVersions(a, b string) int {
	an, as := splitUpdateVersion(a)
	bn, bs := splitUpdateVersion(b)
	for i := 0; i < 3; i++ {
		if an[i] != bn[i] {
			if an[i] < bn[i] {
				return -1
			}
			return 1
		}
	}
	if as == bs {
		return 0
	}
	if as == "" {
		return 1 // 正式版高于预发布
	}
	if bs == "" {
		return -1
	}
	if as < bs {
		return -1
	}
	return 1
}

func splitUpdateVersion(v string) ([3]int, string) {
	var nums [3]int
	main, _, _ := strings.Cut(v, "+") // 构建元数据不参与比较（semver §10）
	if idx := strings.IndexByte(main, '-'); idx >= 0 {
		if n, err := parseUpdateTriple(main[:idx]); err == nil {
			nums = n
		}
		return nums, main[idx+1:]
	}
	if n, err := parseUpdateTriple(main); err == nil {
		nums = n
	}
	return nums, ""
}

func parseUpdateTriple(s string) ([3]int, error) {
	var nums [3]int
	parts := strings.Split(s, ".")
	if len(parts) != 3 {
		return nums, fmt.Errorf("bad triple %q", s)
	}
	for i, p := range parts {
		n, err := strconv.Atoi(p)
		if err != nil || n < 0 {
			return nums, fmt.Errorf("bad component %q", p)
		}
		nums[i] = n
	}
	return nums, nil
}

// artifactPath 组装制品路径（dir/<channel>/<platform>-<arch>/<fileName>），
// 保证结果恒在 dir 内（防 .. 穿越）。
func (h *UpdateHandler) artifactPath(channel, platform, arch, fileName string) (string, error) {
	if !updateNameRe.MatchString(fileName) {
		return "", fmt.Errorf("unsafe file name")
	}
	// 纵深防御：发布路径上 mime/multipart 已把文件名归一到 basename
	// （Part.FileName 剥离目录分量，"../evil.exe" 到手已是 "evil.exe"），
	// 本检查兜住绕过发布校验直接落库的异常记录（下载路径以 DB 中
	// FileName 重新拼路径）。
	if filepath.Base(fileName) != fileName {
		return "", fmt.Errorf("path escaping artifact name")
	}
	rel := filepath.Join(channel, platform+"-"+arch, fileName)
	full := filepath.Join(h.dir, rel)
	if !strings.HasPrefix(full, filepath.Clean(h.dir)+string(os.PathSeparator)) {
		return "", fmt.Errorf("path escapes updates dir")
	}
	return full, nil
}

// HandlePublish 发布新版本（multipart 表单：file + version/channel/
// platform/arch/notes/mandatory）
// @Summary      发布自动更新版本
// @Description  admin 上传安装制品；服务端实测 sha256/size 并落库，
//
//	同 channel+platform+arch+version 重复返回 409
//
// @Tags         update
// @Accept       multipart/form-data
// @Produce      json
// @Security     BearerAuth
// @Param        file       formData file   true  "安装制品"
// @Param        version    formData string true  "x.y.z[-后缀]"
// @Param        channel    formData string true  "stable/beta/nightly"
// @Param        platform   formData string true  "windows/macos/linux"
// @Param        arch       formData string true  "amd64/arm64"
// @Param        notes      formData string false "发布说明"
// @Param        mandatory  formData string false "true=强制更新"
// @Success      201  {object}  object
// @Failure      400  {object}  ErrorResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Failure      409  {object}  ErrorResponse
// @Router       /api/v1/update/publish [post]
func (h *UpdateHandler) HandlePublish(c *gin.Context) {
	if err := c.Request.ParseMultipartForm(64 << 20); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "multipart form required"})
		return
	}

	channel := strings.TrimSpace(c.Request.FormValue("channel"))
	platform := strings.TrimSpace(c.Request.FormValue("platform"))
	arch := strings.TrimSpace(c.Request.FormValue("arch"))
	version := strings.TrimSpace(c.Request.FormValue("version"))
	notes := strings.TrimSpace(c.Request.FormValue("notes"))
	mandatory := strings.EqualFold(strings.TrimSpace(c.Request.FormValue("mandatory")), "true")

	if channel == "" || platform == "" || arch == "" {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "channel/platform/arch required"})
		return
	}
	if !updateVersionRe.MatchString(version) {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "version must be x.y.z[-suffix]"})
		return
	}

	file, header, err := c.Request.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "file field required"})
		return
	}
	defer file.Close()

	_, username, _ := middleware.GetCurrentUser(c)
	// 同 channel+platform+arch+version 视为已发布：拒绝覆盖（已分发制品的
	// sha256 与 DB 记录必须一致，更新走新版本号）
	var existing models.UpdateRelease
	if err := h.db.Where("channel = ? AND platform = ? AND arch = ? AND version = ?",
		channel, platform, arch, version).First(&existing).Error; err == nil {
		c.JSON(http.StatusConflict, gin.H{"success": false, "error": "release already exists"})
		return
	}

	dstPath, err := h.artifactPath(channel, platform, arch, header.Filename)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"success": false, "error": "invalid file name"})
		return
	}
	if err := os.MkdirAll(filepath.Dir(dstPath), 0755); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to prepare updates dir"})
		return
	}

	tmp, err := os.CreateTemp(filepath.Dir(dstPath), ".upload-*")
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to create temp file"})
		return
	}
	tmpName := tmp.Name()
	defer func() { _ = os.Remove(tmpName) }()

	hasher := sha256.New()
	size, err := io.Copy(io.MultiWriter(tmp, hasher), file)
	if err != nil {
		_ = tmp.Close()
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to write artifact"})
		return
	}
	if err := tmp.Close(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to write artifact"})
		return
	}
	if err := os.Chmod(tmpName, 0644); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to chmod artifact"})
		return
	}
	if err := os.Rename(tmpName, dstPath); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to publish artifact"})
		return
	}

	release := models.UpdateRelease{
		Channel:   channel,
		Platform:  platform,
		Arch:      arch,
		Version:   version,
		FileName:  header.Filename,
		Size:      size,
		Sha256:    hex.EncodeToString(hasher.Sum(nil)),
		Notes:     notes,
		Mandatory: mandatory,
		CreatedBy: username,
	}
	if err := h.db.Create(&release).Error; err != nil {
		_ = os.Remove(dstPath)
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to save release"})
		return
	}

	WriteAuditLog(h.db, username, "update.publish", "update_release", map[string]any{
		"id":       release.ID,
		"channel":  channel,
		"platform": platform,
		"arch":     arch,
		"version":  version,
		"size":     size,
		"sha256":   release.Sha256,
		"ip":       c.ClientIP(),
	})

	c.JSON(http.StatusCreated, gin.H{
		"success": true,
		"id":      release.ID,
		"url":     "/api/v1/update/download/" + strconv.FormatUint(uint64(release.ID), 10),
		"sha256":  release.Sha256,
		"size":    size,
	})
}

// HandleLatest 查询最新 manifest（channel/platform/arch 可空，默认
// stable/windows/amd64）
// @Summary      查询最新自动更新版本
// @Description  公开只读：返回最新 manifest（version/url/sha256/size/notes）；
//
//	版本比较在 Go 侧语义化比较（非 SQL 字典序）
//
// @Tags         update
// @Produce      json
// @Param        channel  query string false "默认 stable"
// @Param        platform query string false "默认 windows"
// @Param        arch    query string false "默认 amd64"
// @Success      200  {object}  object
// @Failure      404  {object}  ErrorResponse
// @Router       /api/v1/update/latest [get]
func (h *UpdateHandler) HandleLatest(c *gin.Context) {
	channel := defaultStr(c.Query("channel"), "stable")
	platform := defaultStr(c.Query("platform"), "windows")
	arch := defaultStr(c.Query("arch"), "amd64")

	var releases []models.UpdateRelease
	if err := h.db.Where("channel = ? AND platform = ? AND arch = ?",
		channel, platform, arch).Find(&releases).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to query releases"})
		return
	}
	if len(releases) == 0 {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "no release for this target"})
		return
	}

	latest := &releases[0]
	for i := 1; i < len(releases); i++ {
		if compareUpdateVersions(releases[i].Version, latest.Version) > 0 {
			latest = &releases[i]
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"manifest": gin.H{
			"version":     latest.Version,
			"url":         "/api/v1/update/download/" + strconv.FormatUint(uint64(latest.ID), 10),
			"sha256":      latest.Sha256,
			"size":        latest.Size,
			"notes":       latest.Notes,
			"mandatory":   latest.Mandatory,
			"publishedAt": latest.CreatedAt,
		},
	})
}

// HandleDownload 下载制品（按 ID）
// @Summary      下载自动更新制品
// @Description  公开只读：返回制品文件并带 X-Release-Sha256 响应头供客户端校验
// @Tags         update
// @Produce      octet-stream
// @Param        id   path int true  "发布 ID"
// @Success      200  file
// @Failure      404  {object}  ErrorResponse
// @Router       /api/v1/update/download/{id} [get]
func (h *UpdateHandler) HandleDownload(c *gin.Context) {
	id, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "release not found"})
		return
	}
	var release models.UpdateRelease
	if err := h.db.First(&release, id).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "release not found"})
		return
	}

	dstPath, err := h.artifactPath(release.Channel, release.Platform, release.Arch, release.FileName)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "invalid artifact path"})
		return
	}
	if _, err := os.Stat(dstPath); err != nil {
		c.JSON(http.StatusNotFound, gin.H{"success": false, "error": "artifact missing on disk"})
		return
	}
	c.Header("X-Release-Sha256", release.Sha256)
	c.File(dstPath)
}

// HandleList 发布列表（admin，运维/审计）
// @Summary      自动更新发布列表
// @Description  admin：列出全部发布记录（按时间倒序）
// @Tags         update
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  object
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse
// @Router       /api/v1/update/releases [get]
func (h *UpdateHandler) HandleList(c *gin.Context) {
	var releases []models.UpdateRelease
	if err := h.db.Order("created_at desc").Find(&releases).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"success": false, "error": "failed to query releases"})
		return
	}
	if releases == nil {
		releases = []models.UpdateRelease{}
	}
	c.JSON(http.StatusOK, gin.H{"success": true, "releases": releases})
}

func defaultStr(v, def string) string {
	if strings.TrimSpace(v) == "" {
		return def
	}
	return strings.TrimSpace(v)
}
