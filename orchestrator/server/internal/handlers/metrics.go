package handlers

import (
	"strconv"
	"time"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/gin-gonic/gin"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

// 模块级指标（promauto 注册到默认 registry，进程内单例）。
var (
	httpRequestsTotal = promauto.NewCounterVec(prometheus.CounterOpts{
		Name: "wingman_http_requests_total",
		Help: "Total number of HTTP requests processed, partitioned by method, route pattern and status code.",
	}, []string{"method", "route", "status"})

	activeWebsocketConnections = promauto.NewGauge(prometheus.GaugeOpts{
		Name: "wingman_active_websocket_connections",
		Help: "Current number of dashboard WebSocket connections.",
	})

	registeredAgents = promauto.NewGauge(prometheus.GaugeOpts{
		Name: "wingman_registered_agents",
		Help: "Current number of registered runtime agents (outbound connections).",
	})

	serverUptimeSeconds = promauto.NewGauge(prometheus.GaugeOpts{
		Name: "wingman_uptime_seconds",
		Help: "Seconds since the orchestrator server started.",
	})
)

// MetricsHandler 运行指标端点。
// hub/registry 以最小接口注入，与 ScreenshotHandler 的匿名接口风格一致。
type MetricsHandler struct {
	hub       interface{ ConnectionCount() int }
	registry  *agent.Registry
	startedAt time.Time
}

// NewMetricsHandler 创建指标 handler。startedAt 由 main 装配时传入进程启动时间。
func NewMetricsHandler(hub interface{ ConnectionCount() int }, registry *agent.Registry, startedAt time.Time) *MetricsHandler {
	return &MetricsHandler{
		hub:       hub,
		registry:  registry,
		startedAt: startedAt,
	}
}

// RequestCounter 请求计数中间件：按 method/路由模板/状态码累计总请求数。
// 使用 FullPath() 路由模板（如 /api/v1/metrics）作为 label，避免高基数；
// 未匹配路由记为 "unmatched"。
func (h *MetricsHandler) RequestCounter() gin.HandlerFunc {
	return func(c *gin.Context) {
		c.Next()

		route := c.FullPath()
		if route == "" {
			route = "unmatched"
		}
		httpRequestsTotal.WithLabelValues(c.Request.Method, route, strconv.Itoa(c.Writer.Status())).Inc()
	}
}

// HandleMetrics Prometheus 指标端点
// @Summary      Prometheus 运行指标
// @Description  输出 Prometheus 文本格式：wingman_http_requests_total（总请求数，按 method/route/status 维度）、wingman_active_websocket_connections（活跃 dashboard WebSocket 连接数）、wingman_registered_agents（已注册 runtime agent 数）、wingman_uptime_seconds（服务运行时长），附带 Go runtime/process 默认指标
// @Tags         metrics
// @Produce      text/plain
// @Security     BearerAuth
// @Success      200  {string}  string  "Prometheus 文本格式指标"
// @Failure      401  {object}  ErrorResponse
// @Router       /v1/metrics [get]
func (h *MetricsHandler) HandleMetrics(c *gin.Context) {
	// pull 时刷新实时 gauge（连接数/agent 数随 scrape 采样）
	if h.hub != nil {
		activeWebsocketConnections.Set(float64(h.hub.ConnectionCount()))
	}
	if h.registry != nil {
		registeredAgents.Set(float64(len(h.registry.List())))
	}
	serverUptimeSeconds.Set(time.Since(h.startedAt).Seconds())

	promhttp.Handler().ServeHTTP(c.Writer, c.Request)
}
