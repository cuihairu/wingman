package handlers

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/cuihaitao/wingman/orchestrator/server/pkg/websocket"
	"github.com/gin-gonic/gin"
	"github.com/prometheus/client_golang/prometheus/testutil"
)

// stubConnectionCounter 用于脱离真实 Hub 测试指标端点。
type stubConnectionCounter struct{ count int }

func (s *stubConnectionCounter) ConnectionCount() int { return s.count }

func newMetricsTestRouter(hub interface{ ConnectionCount() int }, registry *agent.Registry) (*gin.Engine, *MetricsHandler) {
	gin.SetMode(gin.TestMode)
	h := NewMetricsHandler(hub, registry, time.Now().Add(-90*time.Second))
	r := gin.New()
	r.Use(h.RequestCounter())
	r.GET("/metrics", h.HandleMetrics)
	r.GET("/ping", func(c *gin.Context) { c.JSON(http.StatusOK, gin.H{"success": true}) })
	return r, h
}

// 请求计数中间件应按 method/route/status 累计请求数。
func TestMetricsRequestCounter(t *testing.T) {
	r, _ := newMetricsTestRouter(&stubConnectionCounter{count: 2}, nil)

	for i := 0; i < 3; i++ {
		w := httptest.NewRecorder()
		r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/ping", nil))
		if w.Code != http.StatusOK {
			t.Fatalf("expected 200, got %d", w.Code)
		}
	}
	// 未匹配路由计入 "unmatched"
	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/nope", nil))
	if w.Code != http.StatusNotFound {
		t.Fatalf("expected 404, got %d", w.Code)
	}

	if got := testutil.ToFloat64(httpRequestsTotal.WithLabelValues(http.MethodGet, "/ping", "200")); got < 3 {
		t.Errorf("expected >=3 requests counted for /ping, got %v", got)
	}
	if got := testutil.ToFloat64(httpRequestsTotal.WithLabelValues(http.MethodGet, "unmatched", "404")); got < 1 {
		t.Errorf("expected >=1 request counted for unmatched, got %v", got)
	}
}

// 指标端点应输出 Prometheus 文本格式，包含连接数/agent 数/uptime/请求数指标。
func TestMetricsEndpointOutput(t *testing.T) {
	r, _ := newMetricsTestRouter(&stubConnectionCounter{count: 5}, nil)

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/metrics", nil))

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if ct := w.Header().Get("Content-Type"); !strings.Contains(ct, "text/plain") {
		t.Errorf("expected text/plain content type, got %q", ct)
	}

	body := w.Body.String()
	for _, metric := range []string{
		"wingman_http_requests_total",
		"wingman_active_websocket_connections",
		"wingman_registered_agents",
		"wingman_uptime_seconds",
	} {
		if !strings.Contains(body, metric) {
			t.Errorf("metrics output should contain %s", metric)
		}
	}

	// 活跃连接数来自注入的 counter；uptime 基准为 90s 前启动
	if !strings.Contains(body, "wingman_active_websocket_connections 5") {
		t.Errorf("active websocket connections should be 5, got:\n%s", body)
	}
	if !strings.Contains(body, "wingman_uptime_seconds 9") {
		t.Errorf("uptime should be ~90s, got:\n%s", body)
	}
}

// 请求指标端点本身也应被计数（route=/metrics）。
func TestMetricsEndpointCountsItself(t *testing.T) {
	r, _ := newMetricsTestRouter(&stubConnectionCounter{}, nil)

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/metrics", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if got := testutil.ToFloat64(
		httpRequestsTotal.WithLabelValues(http.MethodGet, "/metrics", "200")); got < 1 {
		t.Errorf("metrics endpoint should count itself, got %v", got)
	}
}

// 注入真实 registry（空注册表）时端点正常输出 registered_agents=0。
func TestMetricsWithRealRegistry(t *testing.T) {
	hub := websocket.NewHub()
	r, _ := newMetricsTestRouter(hub, agent.NewRegistry(hub))

	w := httptest.NewRecorder()
	r.ServeHTTP(w, httptest.NewRequest(http.MethodGet, "/metrics", nil))
	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if !strings.Contains(w.Body.String(), "wingman_registered_agents 0") {
		t.Errorf("registered agents should be 0, got:\n%s", w.Body.String())
	}
}
