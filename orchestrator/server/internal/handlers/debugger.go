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
	AgentID   string `json:"agentId" example:"agent-001"`
	Hostname  string `json:"hostname" example:"game-pc"`
	IP        string `json:"ip" example:"10.0.0.5"`
	Status    string `json:"status" example:"online"`
	Host      string `json:"host" example:"10.0.0.5"`
	Port      int    `json:"port" example:"9966"`
	Endpoint  string `json:"endpoint" example:"10.0.0.5:9966"`
	Reachable bool   `json:"reachable" example:"true"`
}

// DirectAttachResponse 「直连模式」固定响应（HTTP 501）。
// dashboard 据此展示直连指引而非当作崩溃错误处理。
type DirectAttachResponse struct {
	Success bool `json:"success" example:"false"`
	// 固定为 direct_attach
	Mode string `json:"mode" example:"direct_attach"`
	// 说明调试协议不经 server 中转
	Error string `json:"error" example:"EmmyLua debugging is not proxied through the server. Use VSCode EmmyLua to attach directly to the runtime debug port."`
	// 触发本次拒绝的操作名
	Operation string `json:"operation" example:"connect"`
	// 获取各 agent 调试端点的入口
	Hint string `json:"hint" example:"GET /api/debugger/info returns each agent's debug endpoint and a VSCode launch.json snippet."`
	// 推荐的 VSCode 扩展
	VSCodeExtension string `json:"vscodeExtension" example:"tangzx.emmylua"`
}

// DebuggerInfoResponse 调试器信息响应（直连模式说明 + 各 agent 端点 + launch.json 片段）
type DebuggerInfoResponse struct {
	Success bool `json:"success" example:"true"`
	// 固定为 direct_attach
	Mode string `json:"mode" example:"direct_attach"`
	// runtime 默认调试端口
	DefaultPort int `json:"defaultPort" example:"9966"`
	// 模式说明
	Description string `json:"description" example:"Lua debugging via EmmyLua: VSCode attaches directly to the runtime debug port."`
	// 推荐的 VSCode 扩展
	VSCodeExtension string `json:"vscodeExtension" example:"tangzx.emmylua"`
	// VSCode launch.json 片段
	LaunchConfig map[string]any `json:"launchConfig"`
	// 各 agent 的调试端点
	Agents []agentDebugEndpoint `json:"agents"`
}

// directAttachError 返回统一的「直连模式」结构化响应（HTTP 501）。
// dashboard 据此展示指引而非当作崩溃错误处理。
func directAttachError(operation string) (int, gin.H) {
	return http.StatusNotImplemented, gin.H{
		"success":         false,
		"mode":            "direct_attach",
		"error":           "EmmyLua debugging is not proxied through the server. Use VSCode EmmyLua to attach directly to the runtime debug port.",
		"operation":       operation,
		"hint":            "GET /api/debugger/info returns each agent's debug endpoint and a VSCode launch.json snippet.",
		"vscodeExtension": "tangzx.emmylua",
	}
}

// HandleDebuggerInfo 返回调试模式说明与各 agent 的调试端点 + VSCode 配置。
// GET /api/debugger/info
// @Summary      调试器信息（直连模式）
// @Description  EmmyLua 调试由 VSCode 直连 runtime 调试端口（默认 9966），server 不中转；返回各 agent 的 host:port、可达性与 launch.json 片段；仅 admin 角色可访问
// @Tags         debugger
// @Produce      json
// @Security     BearerAuth
// @Success      200  {object}  DebuggerInfoResponse
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse "非 admin 角色"
// @Router       /debugger/info [get]
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
		"success":         true,
		"mode":            "direct_attach",
		"defaultPort":     defaultDebugPort,
		"description":     "Lua debugging via EmmyLua: VSCode attaches directly to the runtime debug port. The orchestrator does not proxy the debug protocol.",
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
// @Summary      调试器连接（直连模式）
// @Description  调试协议是双向流（断点/单步/变量），不适合请求/响应模型；固定返回 501 与 VSCode 直连指引；仅 admin 角色可访问
// @Tags         debugger
// @Produce      json
// @Security     BearerAuth
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse "非 admin 角色"
// @Failure      501  {object}  DirectAttachResponse
// @Router       /debugger/connect [post]
func (h *DebugHandler) HandleDebuggerConnect(c *gin.Context) {
	code, body := directAttachError("connect")
	c.JSON(code, body)
}

// HandleDebuggerCommand 调试命令（直连模式）
// @Summary      调试命令（直连模式）
// @Description  调试命令由 VSCode 直连 runtime 执行，server 不中转；固定返回 501 与直连指引；仅 admin 角色可访问
// @Tags         debugger
// @Produce      json
// @Security     BearerAuth
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse "非 admin 角色"
// @Failure      501  {object}  DirectAttachResponse
// @Router       /debugger/command [post]
func (h *DebugHandler) HandleDebuggerCommand(c *gin.Context) {
	code, body := directAttachError("command")
	c.JSON(code, body)
}

// HandleDebuggerGetBreakpoints 获取断点（直连模式）
// @Summary      获取断点（直连模式）
// @Description  断点由 VSCode 直连 runtime 管理，server 不中转；固定返回 501 与直连指引；仅 admin 角色可访问
// @Tags         debugger
// @Produce      json
// @Security     BearerAuth
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse "非 admin 角色"
// @Failure      501  {object}  DirectAttachResponse
// @Router       /debugger/breakpoints [get]
func (h *DebugHandler) HandleDebuggerGetBreakpoints(c *gin.Context) {
	code, body := directAttachError("get_breakpoints")
	c.JSON(code, body)
}

// HandleDebuggerSetBreakpoints 设置断点（直连模式）
// @Summary      设置断点（直连模式）
// @Description  断点由 VSCode 直连 runtime 管理，server 不中转；固定返回 501 与直连指引；仅 admin 角色可访问
// @Tags         debugger
// @Produce      json
// @Security     BearerAuth
// @Failure      401  {object}  ErrorResponse
// @Failure      403  {object}  ErrorResponse "非 admin 角色"
// @Failure      501  {object}  DirectAttachResponse
// @Router       /debugger/breakpoints [post]
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
