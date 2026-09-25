package handlers

// 会话录像检索 handler 测试（设计 §16）：未配置 501、列表排序与后缀过滤、
// 名字校验拒绝路径穿越、下载/删除 happy path 与 404、审计落库。

import (
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm"
)

// newRecordingsEnv 构造测试路由（admin 身份直注入）与录像目录。
func newRecordingsEnv(t *testing.T, configured bool) (*gin.Engine, string, *gorm.DB) {
	t.Helper()
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	dir := ""
	if configured {
		dir = t.TempDir()
	}
	rh := NewRecordingsHandler(db, dir)
	r := gin.New()
	grp := r.Group("/api/remote/recordings", asAdmin(1))
	{
		grp.GET("", rh.HandleList)
		grp.GET("/:name/download", rh.HandleDownload)
		grp.DELETE("/:name", rh.HandleDelete)
	}
	return r, dir, db
}

func writeRecording(t *testing.T, dir, name, content string) string {
	t.Helper()
	full := filepath.Join(dir, name)
	if err := os.WriteFile(full, []byte(content), 0o600); err != nil {
		t.Fatalf("write recording: %v", err)
	}
	return full
}

func TestRecordingsNotConfiguredReturns501(t *testing.T) {
	r, _, _ := newRecordingsEnv(t, false)
	for _, tc := range []struct{ method, path string }{
		{http.MethodGet, "/api/remote/recordings"},
		{http.MethodGet, "/api/remote/recordings/a.mjs/download"},
		{http.MethodDelete, "/api/remote/recordings/a.mjs"},
	} {
		w := httptest.NewRecorder()
		req := httptest.NewRequest(tc.method, tc.path, nil)
		r.ServeHTTP(w, req)
		if w.Code != http.StatusNotImplemented {
			t.Fatalf("%s %s: got %d want 501 (%s)", tc.method, tc.path, w.Code, w.Body.String())
		}
		if !strings.Contains(w.Body.String(), "recording not configured") {
			t.Errorf("%s %s: error should explain config: %s", tc.method, tc.path, w.Body.String())
		}
	}
}

func TestRecordingsListFiltersAndSorts(t *testing.T) {
	r, dir, _ := newRecordingsEnv(t, true)
	writeRecording(t, dir, "older.mjs", "aaa")
	time.Sleep(10 * time.Millisecond) // 保证 mtime 可区分
	writeRecording(t, dir, "newer.mjs", "bbbb")
	// 非 .mjs 与子目录不进列表
	writeRecording(t, dir, "notes.txt", "x")
	if err := os.Mkdir(filepath.Join(dir, "sub"), 0o700); err != nil {
		t.Fatalf("mkdir: %v", err)
	}

	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/recordings", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("list: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	body := w.Body.String()
	if !strings.Contains(body, "newer.mjs") || !strings.Contains(body, "older.mjs") {
		t.Fatalf("list should contain both recordings: %s", body)
	}
	if strings.Contains(body, "notes.txt") {
		t.Errorf("non-mjs files must be filtered: %s", body)
	}
	// mtime 倒序：newer 在前
	if strings.Index(body, "newer.mjs") > strings.Index(body, "older.mjs") {
		t.Errorf("list must sort by mtime desc: %s", body)
	}
}

func TestRecordingsListEmptyDirMissingIsOK(t *testing.T) {
	// 尚无任何录像时目录不存在（guacd 尚未创建）是正常空态
	gin.SetMode(gin.TestMode)
	db := newDB(t)
	rh := NewRecordingsHandler(db, filepath.Join(t.TempDir(), "not-created-yet"))
	r := gin.New()
	r.GET("/api/remote/recordings", rh.HandleList)

	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/recordings", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("empty: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	if !strings.Contains(w.Body.String(), `"data":[]`) {
		t.Errorf("empty list should be []: %s", w.Body.String())
	}
}

func TestRecordingsDownloadHappyAndAudit(t *testing.T) {
	r, dir, db := newRecordingsEnv(t, true)
	writeRecording(t, dir, "agent-1-sess-9.mjs", "session-bytes")

	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/recordings/agent-1-sess-9.mjs/download", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("download: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	if w.Body.String() != "session-bytes" {
		t.Errorf("download body wrong: %q", w.Body.String())
	}
	var count int64
	db.Table("audit_logs").Where("kind = ?", "desktop.recording_download").Count(&count)
	if count != 1 {
		t.Errorf("download must be audited, got %d rows", count)
	}
}

func TestResolveRecordingName(t *testing.T) {
	// 名字校验是防御底线：gin 路由会先行拦截 URL 编码的穿越，但 handler
	// 不能依赖路由层（内部复用/未来调用方都可能直接传名）
	ok := []string{"a.mjs", "agent-1-sess-9.mjs", "...mjs"}
	for _, name := range ok {
		if _, valid := resolveRecordingName(name); !valid {
			t.Errorf("name %q should be valid", name)
		}
	}
	bad := []string{"", ".", "..", "a.txt", "a/b.mjs", `a\b.mjs`, "/abs.mjs", "sub/../a.mjs", "a.mjs/../b.mjs"}
	for _, name := range bad {
		if _, valid := resolveRecordingName(name); valid {
			t.Errorf("name %q must be rejected", name)
		}
	}
}

func TestRecordingsNameValidation(t *testing.T) {
	r, dir, _ := newRecordingsEnv(t, true)
	writeRecording(t, dir, "keep.mjs", "x")

	// 非法后缀 400（直名进 handler 的路径）
	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodGet, "/api/remote/recordings/a.txt/download", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("non-mjs suffix: got %d want 400 (%s)", w.Code, w.Body.String())
	}

	// URL 编码穿越被 gin 路由层拦截（不进 handler 即不可达文件系统）
	w2 := httptest.NewRecorder()
	req2 := httptest.NewRequest(http.MethodGet, "/api/remote/recordings/..%2F..%2Fetc%2Fpasswd.mjs/download", nil)
	r.ServeHTTP(w2, req2)
	if w2.Code == http.StatusOK {
		t.Errorf("encoded traversal must not reach handler: %d", w2.Code)
	}

	// 未知录像 404
	w3 := httptest.NewRecorder()
	req3 := httptest.NewRequest(http.MethodGet, "/api/remote/recordings/ghost.mjs/download", nil)
	r.ServeHTTP(w3, req3)
	if w3.Code != http.StatusNotFound {
		t.Errorf("missing recording: got %d want 404", w3.Code)
	}
}

func TestRecordingsDeleteHappyAndAudit(t *testing.T) {
	r, dir, db := newRecordingsEnv(t, true)
	full := writeRecording(t, dir, "gone.mjs", "x")

	w := httptest.NewRecorder()
	req := httptest.NewRequest(http.MethodDelete, "/api/remote/recordings/gone.mjs", nil)
	r.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("delete: got %d want 200 (%s)", w.Code, w.Body.String())
	}
	if _, err := os.Stat(full); !os.IsNotExist(err) {
		t.Errorf("recording file should be removed")
	}
	var count int64
	db.Table("audit_logs").Where("kind = ?", "desktop.recording_delete").Count(&count)
	if count != 1 {
		t.Errorf("delete must be audited, got %d rows", count)
	}

	// 再删同一名：404
	w2 := httptest.NewRecorder()
	req2 := httptest.NewRequest(http.MethodDelete, "/api/remote/recordings/gone.mjs", nil)
	r.ServeHTTP(w2, req2)
	if w2.Code != http.StatusNotFound {
		t.Errorf("second delete: got %d want 404", w2.Code)
	}
}
