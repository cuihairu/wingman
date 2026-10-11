package handlers

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io/fs"
	"mime/multipart"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/models"
	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

func setupUpdateRouter(t *testing.T) (*gin.Engine, *UpdateHandler, *gorm.DB, string) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := t.TempDir()
	h := NewUpdateHandler(db, dir)

	r := gin.New()
	r.POST("/api/v1/update/publish", asAdmin(1), h.HandlePublish)
	r.GET("/api/v1/update/releases", asAdmin(1), h.HandleList)
	r.GET("/api/v1/update/latest", h.HandleLatest)
	r.GET("/api/v1/update/download/:id", h.HandleDownload)
	return r, h, db, dir
}

func doUpdatePublish(r *gin.Engine, t *testing.T, fields map[string]string, fileName, content []byte) *httptest.ResponseRecorder {
	t.Helper()
	var buf bytes.Buffer
	w := multipart.NewWriter(&buf)
	fw, err := w.CreateFormFile("file", string(fileName))
	if err != nil {
		t.Fatalf("create form file: %v", err)
	}
	if _, err := fw.Write(content); err != nil {
		t.Fatalf("write file part: %v", err)
	}
	for k, v := range fields {
		if err := w.WriteField(k, v); err != nil {
			t.Fatalf("write field %s: %v", k, err)
		}
	}
	_ = w.Close()

	req := httptest.NewRequest(http.MethodPost, "/api/v1/update/publish", &buf)
	req.Header.Set("Content-Type", w.FormDataContentType())
	rec := httptest.NewRecorder()
	r.ServeHTTP(rec, req)
	return rec
}

func doUpdateGet(r *gin.Engine, t *testing.T, path string) *httptest.ResponseRecorder {
	t.Helper()
	req := httptest.NewRequest(http.MethodGet, path, nil)
	rec := httptest.NewRecorder()
	r.ServeHTTP(rec, req)
	return rec
}

func updateManifest(t *testing.T, path string, rec *httptest.ResponseRecorder) map[string]any {
	t.Helper()
	if rec.Code != http.StatusOK {
		t.Fatalf("GET %s: %d %s", path, rec.Code, rec.Body.String())
	}
	var resp struct {
		Success  bool           `json:"success"`
		Manifest map[string]any `json:"manifest"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode manifest: %v (%s)", err, rec.Body.String())
	}
	if !resp.Success || resp.Manifest == nil {
		t.Fatalf("bad manifest response: %s", rec.Body.String())
	}
	return resp.Manifest
}

func publishStable(t *testing.T, r *gin.Engine, version string, content []byte) {
	t.Helper()
	w := doUpdatePublish(r, t, map[string]string{
		"version": version, "channel": "stable", "platform": "windows", "arch": "amd64",
	}, []byte("setup-"+version+".exe"), content)
	if w.Code != http.StatusCreated {
		t.Fatalf("publish %s: %d %s", version, w.Code, w.Body.String())
	}
}

func stableLatest(t *testing.T, r *gin.Engine) map[string]any {
	t.Helper()
	return updateManifest(t, "/api/v1/update/latest", doUpdateGet(r, t, "/api/v1/update/latest"))
}

// TestUpdatePublishThenLatestDownload 发布 → 公开 latest → 公开下载：
// sha256/size 由服务端实测，下载响应带 X-Release-Sha256 供客户端校验。
func TestUpdatePublishThenLatestDownload(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)

	content := []byte("wingman-setup-1.2.3 payload")
	sum := sha256.Sum256(content)
	wantSha := hex.EncodeToString(sum[:])

	w := doUpdatePublish(r, t, map[string]string{
		"version":   "1.2.3",
		"channel":   "stable",
		"platform":  "windows",
		"arch":      "amd64",
		"notes":     "first release",
		"mandatory": "true",
	}, []byte("wingman-setup-1.2.3.exe"), content)
	if w.Code != http.StatusCreated {
		t.Fatalf("publish: %d %s", w.Code, w.Body.String())
	}
	var pub struct {
		Success bool   `json:"success"`
		ID      uint   `json:"id"`
		URL     string `json:"url"`
		Sha256  string `json:"sha256"`
		Size    int64  `json:"size"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &pub); err != nil {
		t.Fatalf("decode publish: %v", err)
	}
	if !pub.Success || pub.Sha256 != wantSha || pub.Size != int64(len(content)) || pub.ID == 0 {
		t.Fatalf("publish response: %s", w.Body.String())
	}

	m := stableLatest(t, r)
	if m["version"] != "1.2.3" || m["sha256"] != wantSha || m["notes"] != "first release" {
		t.Fatalf("manifest mismatch: %v", m)
	}
	if int(m["size"].(float64)) != len(content) || m["mandatory"] != true {
		t.Fatalf("manifest meta: %v", m)
	}
	if m["url"] != pub.URL {
		t.Fatalf("url mismatch: %v vs %s", m["url"], pub.URL)
	}

	rec := doUpdateGet(r, t, pub.URL)
	if rec.Code != http.StatusOK {
		t.Fatalf("download: %d %s", rec.Code, rec.Body.String())
	}
	if !bytes.Equal(rec.Body.Bytes(), content) {
		t.Fatalf("download payload mismatch")
	}
	if rec.Header().Get("X-Release-Sha256") != wantSha {
		t.Fatalf("sha256 header: %q", rec.Header().Get("X-Release-Sha256"))
	}
}

// TestUpdatePublishValidation 缺 file / 坏版本号 / 缺 target / 不安全
// 文件名一律 400。
func TestUpdatePublishValidation(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)

	base := map[string]string{
		"version": "1.0.0", "channel": "stable", "platform": "windows", "arch": "amd64",
	}
	cases := []struct {
		name   string
		fields map[string]string
		fname  string
	}{
		{"missing file", base, ""},
		{"bad version", map[string]string{
			"version": "abc", "channel": "stable", "platform": "windows", "arch": "amd64",
		}, "setup.exe"},
		{"missing platform", map[string]string{
			"version": "1.0.0", "channel": "stable", "arch": "amd64",
		}, "setup.exe"},
		// mime/multipart 已把 "../evil.exe" 归一成 "evil.exe"，穿越名到不了
		// 校验；正则面留给首字符非法的名字（multipart 归一不会救它）
		{"unsafe file name", base, ".hidden-setup.exe"},
	}
	for _, tc := range cases {
		w := doUpdatePublish(r, t, tc.fields, []byte(tc.fname), []byte("x"))
		if w.Code != http.StatusBadRequest {
			t.Fatalf("%s: want 400 got %d %s", tc.name, w.Code, w.Body.String())
		}
	}
}

// TestUpdatePublishTraversalNeutralized 穿越文件名 "../evil.exe" 发布：
// multipart 归一成 basename 后 201 落盘，制品必须落在
// <dir>/<channel>/<platform>-<arch>/ 内（不得借路径分量逃出 UpdatesDir）。
func TestUpdatePublishTraversalNeutralized(t *testing.T) {
	r, _, _, dir := setupUpdateRouter(t)

	w := doUpdatePublish(r, t, map[string]string{
		"version": "1.0.0", "channel": "stable", "platform": "windows", "arch": "amd64",
	}, []byte("../evil.exe"), []byte("payload"))
	if w.Code != http.StatusCreated {
		t.Fatalf("publish sanitized name: %d %s", w.Code, w.Body.String())
	}

	// 递归收集 UpdatesDir 全部文件：唯一制品必须在两层的 target 子目录里
	var inside int
	err := filepath.WalkDir(dir, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		inside++
		rel, relErr := filepath.Rel(dir, p)
		if relErr != nil {
			return relErr
		}
		if got := strings.Count(rel, string(os.PathSeparator)); got != 2 {
			t.Fatalf("artifact depth %d (want target/channel/file layout): %s", got, rel)
		}
		return nil
	})
	if err != nil {
		t.Fatalf("walk updates dir: %v", err)
	}
	if inside != 1 {
		t.Fatalf("artifact count in dir: %d", inside)
	}
}

// TestUpdatePublishDuplicateConflict 同 target+version 二次发布 409
// （版本不可变：已分发制品的 sha256 必须与 DB 记录一致）。
func TestUpdatePublishDuplicateConflict(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)

	fields := map[string]string{
		"version": "1.0.0", "channel": "stable", "platform": "windows", "arch": "amd64",
	}
	if w := doUpdatePublish(r, t, fields, []byte("a.exe"), []byte("one")); w.Code != http.StatusCreated {
		t.Fatalf("first publish: %d %s", w.Code, w.Body.String())
	}
	w := doUpdatePublish(r, t, fields, []byte("a.exe"), []byte("two"))
	if w.Code != http.StatusConflict {
		t.Fatalf("second publish: %d %s", w.Code, w.Body.String())
	}
}

// TestUpdateLatestEmpty 无发布记录 → 404（客户端据此判定无可更新）。
func TestUpdateLatestEmpty(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)
	rec := doUpdateGet(r, t, "/api/v1/update/latest")
	if rec.Code != http.StatusNotFound {
		t.Fatalf("empty latest: %d %s", rec.Code, rec.Body.String())
	}
}

// TestUpdateLatestSemverOrdering 版本比较为语义化比较而非字典序：
// 1.10.0 > 1.9.0；同三元组正式版 > 预发布（2.0.0 > 2.0.0-beta.1）。
func TestUpdateLatestSemverOrdering(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)

	pub := func(version string) string {
		t.Helper()
		publishStable(t, r, version, []byte(version))
		return version
	}

	pub("1.9.0")
	pub("1.10.0")
	if got := stableLatest(t, r)["version"]; got != "1.10.0" {
		t.Fatalf("numeric ordering: want 1.10.0 got %v", got)
	}
	// 同 channel 内预发布参与排序（beta 通道隔离交给 channel 过滤）：
	// 2.0.0-beta.1 > 1.10.0，同三元组正式版 2.0.0 > 2.0.0-beta.1
	pub("2.0.0-beta.1")
	if got := stableLatest(t, r)["version"]; got != "2.0.0-beta.1" {
		t.Fatalf("higher prerelease wins: got %v", got)
	}
	pub("2.0.0")
	if got := stableLatest(t, r)["version"]; got != "2.0.0" {
		t.Fatalf("release beats same-triple prerelease: got %v", got)
	}
}

// TestUpdateLatestTargetFilter channel/platform/arch 过滤互不串数据。
func TestUpdateLatestTargetFilter(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)

	w := doUpdatePublish(r, t, map[string]string{
		"version": "3.0.0", "channel": "beta", "platform": "macos", "arch": "arm64",
	}, []byte("wingman.dmg"), []byte("mac"))
	if w.Code != http.StatusCreated {
		t.Fatalf("publish: %d %s", w.Code, w.Body.String())
	}

	if rec := doUpdateGet(r, t, "/api/v1/update/latest"); rec.Code != http.StatusNotFound {
		t.Fatalf("default target should miss: %d", rec.Code)
	}
	m := updateManifest(t, "/api/v1/update/latest?beta/macos/arm64", doUpdateGet(r, t,
		"/api/v1/update/latest?channel=beta&platform=macos&arch=arm64"))
	if m["version"] != "3.0.0" {
		t.Fatalf("filtered manifest: %v", m)
	}
}

// TestUpdateDownloadUnknown 未知 ID → 404。
func TestUpdateDownloadUnknown(t *testing.T) {
	r, _, _, _ := setupUpdateRouter(t)
	rec := doUpdateGet(r, t, "/api/v1/update/download/999")
	if rec.Code != http.StatusNotFound {
		t.Fatalf("unknown download: %d %s", rec.Code, rec.Body.String())
	}
}

// TestUpdateListAndAudit 列表返回全部记录；发布落审计（update.publish）。
func TestUpdateListAndAudit(t *testing.T) {
	r, _, db, _ := setupUpdateRouter(t)

	publishStable(t, r, "1.0.0", []byte("payload-a"))
	publishStable(t, r, "1.0.1", []byte("payload-b"))

	rec := doUpdateGet(r, t, "/api/v1/update/releases")
	if rec.Code != http.StatusOK {
		t.Fatalf("list: %d %s", rec.Code, rec.Body.String())
	}
	var resp struct {
		Releases []models.UpdateRelease `json:"releases"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("decode list: %v", err)
	}
	if len(resp.Releases) != 2 {
		t.Fatalf("list count: %+v", resp.Releases)
	}

	var audits []models.AuditLog
	if err := db.Where("kind = ?", "update.publish").Find(&audits).Error; err != nil {
		t.Fatalf("query audits: %v", err)
	}
	if len(audits) != 2 {
		t.Fatalf("audit rows: %d", len(audits))
	}
}

// TestUpdateArtifactMissingOnDisk DB 有记录但制品文件被清 → 下载 404
// （不暴露 500/panic，客户端按"暂不可更新"处理）。
func TestUpdateArtifactMissingOnDisk(t *testing.T) {
	r, _, _, dir := setupUpdateRouter(t)

	publishStable(t, r, "1.0.0", []byte("payload"))
	url, _ := stableLatest(t, r)["url"].(string)
	if url == "" {
		t.Fatalf("manifest url empty")
	}
	if rec := doUpdateGet(r, t, url); rec.Code != http.StatusOK {
		t.Fatalf("baseline download: %d %s", rec.Code, rec.Body.String())
	}

	// 清空 UpdatesDir 全部制品（模拟磁盘清理）
	if err := os.RemoveAll(dir); err != nil {
		t.Fatalf("remove artifacts: %v", err)
	}
	if rec := doUpdateGet(r, t, url); rec.Code != http.StatusNotFound {
		t.Fatalf("missing artifact download: %d %s", rec.Code, rec.Body.String())
	}
	// manifest 仍可查询（DB 记录独立于磁盘），客户端拿 url 再试
	if rec := doUpdateGet(r, t, "/api/v1/update/latest"); rec.Code != http.StatusOK {
		t.Fatalf("latest after artifact loss: %d %s", rec.Code, rec.Body.String())
	}
}
