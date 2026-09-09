package middleware

import (
	"bytes"
	"net/http"
	"net/http/httptest"
	"regexp"
	"testing"

	"github.com/gin-gonic/gin"
)

// 请求日志应按 "[INFO] 时间 | 状态码 | 耗时 | 方法 路径" 格式输出。
func TestRequestLogFormatterOutput(t *testing.T) {
	gin.SetMode(gin.TestMode)

	var buf bytes.Buffer
	oldWriter := gin.DefaultWriter
	gin.DefaultWriter = &buf
	defer func() { gin.DefaultWriter = oldWriter }()

	r := gin.New()
	r.Use(gin.LoggerWithFormatter(RequestLogFormatter))
	r.GET("/ping", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"success": true}) })
	r.POST("/echo", func(c *gin.Context) { c.Status(http.StatusCreated) })

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/ping", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}

	line := buf.String()
	pattern := `^\[INFO\] \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \| 200 \| [0-9]+(\.[0-9]+)?(ns|µs|ms|s) \| GET /ping\n$`
	if !regexp.MustCompile(pattern).MatchString(line) {
		t.Errorf("log line does not match expected format:\n%q", line)
	}

	buf.Reset()
	w = httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodPost, "/echo", nil))
	if w.Code != http.StatusCreated {
		t.Fatalf("expected 201, got %d", w.Code)
	}
	pattern = `^\[INFO\] \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \| 201 \| [0-9]+(\.[0-9]+)?(ns|µs|ms|s) \| POST /echo\n$`
	if !regexp.MustCompile(pattern).MatchString(buf.String()) {
		t.Errorf("log line does not match expected format:\n%q", buf.String())
	}
}

// 格式化器本身对固定输入应产生确定输出（时间/耗时以外字段精确匹配）。
func TestRequestLogFormatterFields(t *testing.T) {
	out := RequestLogFormatter(gin.LogFormatterParams{
		StatusCode: 200,
		Latency:    15 * 1000 * 1000, // 15ms
		Method:     http.MethodGet,
		Path:       "/api/v1/agents",
	})
	pattern := `^\[INFO\] \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} \| 200 \| 15ms \| GET /api/v1/agents\n$`
	if !regexp.MustCompile(pattern).MatchString(out) {
		t.Errorf("unexpected formatter output: %q", out)
	}
}
