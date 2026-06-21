package handlers

import (
	"net/http"
	"strconv"

	"github.com/cuihaitao/wingman/orchestrator/server/internal/agent"
	"github.com/gin-gonic/gin"
)

// 默认 EmmyLua 调试端口（runtime agent.toml [debugger].listen_port）。
const defaultDebugPort = 9966

// DebugHandler 调试器信息端点。
//
// Wingman 的 Lua 调试基于 EmmyLua，由 VSCode 直连 runtime 的调试端口（默认 9966），
// **不**经由 Go server 中转。原因：调试协议是双向流（断点/单步/变量），不适合
// dashboard → Go server → runtime agent 的请求/响应模型；且架构上 dashboard 只连 Go server、
// runtime 是 outbound agent，没有反向拨入通道承载调试流。
//
// 因此 connect/command/breakpoints 端点返回结构化的「直连模式」说明，而非占位 501。
type DebugHandler struct {
	registry *agent.Registry
}

func NewDebugHandler(registry *agent.Registry) *DebugHandler {
	return &DebugHandler{registry: registry}
}

type agentDebugEndpoint struct {
	AgentID   string `json:"agentId"`
	Hostname  string `json:"hostname"`
	IP        string `json:"ip"`
	Status    string `json:"status"`
	Host      string `json:"host"`
	Port      int    `json:"port"`
	Endpoint  string `json:"endpoint"`
	Reachable bool   `json:"reachable"`
}

// directAttachError 返回统一的「直连模式」结构化响应（HTTP 501）。
// dashboard 据此展示指引而非当作崩溃错误处理。
func directAttachError(operation string) (int, gin.H) {
	return http.StatusNotImplemented, gin.H{
		"success": false,
		"mode":    "direct_attach",
		"error":   "EmmyLua debugging is not proxied through the server. Use VSCode EmmyLua to attach directly to the runtime debug port.",
		"operation":    operation,
		"hint":         "GET /api/debugger/info returns each agent's debug endpoint and a VSCode launch.json snippet.",
		"vscodeExtension": "tangzx.emmylua",
	}
}

// HandleDebuggerInfo 返回调试模式说明与各 agent 的调试端点 + VSCode 配置。
// GET /api/debugger/info
func (h *DebugHandler) HandleDebuggerInfo(c *gin.Context) {
	endpoints := make([]agentDebugEndpoint, 0)
	if h.registry != nil {
		for _, a := range h.registry.List() {
			host := firstNonEmpty(a.IP, a.Hostname, "127.0.0.1")
			endpoints = append(endpoints, agentDebugEndpoint{
				AgentID:   a.AgentID,
				Hostname:  a.Hostname,
				IP:        a.IP,
				Status:    string(a.Status),
				Host:      host,
				Port:      defaultDebugPort,
				Endpoint:  formatHostPort(host, defaultDebugPort),
				Reachable: a.Status == agent.StatusOnline || a.Status == agent.StatusIdle || a.Status == agent.StatusBusy,
			})
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"success": true,
		"mode":    "direct_attach",
		"defaultPort": defaultDebugPort,
		"description": "Lua debugging via EmmyLua: VSCode attaches directly to the runtime debug port. The orchestrator does not proxy the debug protocol.",
		"vscodeExtension": "tangzx.emmylua",
		"launchConfig": gin.H{
			"type":    "emmylua",
			"request": "attach",
			"name":    "Attach to Wingman",
			"host":    "localhost",
			"port":    defaultDebugPort,
			"ext":     []string{".lua", ".lua.txt"},
		},
		"agents": endpoints,
	})
}

// HandleDebuggerConnect 调试器连接（直连模式，不由 server 中转）
func (h *DebugHandler) HandleDebuggerConnect(c *gin.Context) {
	code, body := directAttachError("connect")
	c.JSON(code, body)
}

// HandleDebuggerCommand 调试命令（直连模式）
func (h *DebugHandler) HandleDebuggerCommand(c *gin.Context) {
	code, body := directAttachError("command")
	c.JSON(code, body)
}

// HandleDebuggerGetBreakpoints 获取断点（直连模式）
func (h *DebugHandler) HandleDebuggerGetBreakpoints(c *gin.Context) {
	code, body := directAttachError("get_breakpoints")
	c.JSON(code, body)
}

// HandleDebuggerSetBreakpoints 设置断点（直连模式）
func (h *DebugHandler) HandleDebuggerSetBreakpoints(c *gin.Context) {
	code, body := directAttachError("set_breakpoints")
	c.JSON(code, body)
}

func firstNonEmpty(values ...string) string {
	for _, v := range values {
		if v != "" {
			return v
		}
	}
	return ""
}

func formatHostPort(host string, port int) string {
	return host + ":" + strconv.Itoa(port)
}
