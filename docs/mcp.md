# MCP Server

Wingman Agent 自带一个 **MCP（Model Context Protocol）server**，以 **stdio 传输**
（newline-delimited JSON-RPC 2.0，不用 Content-Length 帧）对外暴露 UI 自动化能力。
AI 客户端（Claude Code / Cursor / Hermes 等）本地拉起 `wingman-agent mcp`，即可
语义级操作目标应用的 UI Automation 元素树——替代「截图 → LLM 猜坐标 → 盲点」链路，
无坐标漂移。

```bash
# 启动后 stdin/stdout 即协议流（每行一个 JSON-RPC 帧）
wingman-agent mcp
```

- **stdout 只承载协议帧**；所有日志（含 crash 采集、脚本输出）走 stderr。
  客户端按行读 stdout，每响应一行即 flush。
- 协议版本协商：回显客户端请求的受支持版本，未知版本回退到服务端最新
  （当前 `2025-11-25`；同时支持 `2025-06-18`、`2025-03-26`、`2024-11-05`）。

## 接入

### Claude Code

```bash
claude mcp add wingman -- /path/to/wingman-agent mcp
```

### Cursor

`Settings → MCP → Add new global MCP server`，或编辑 `~/.cursor/mcp.json`：

```json
{
  "mcpServers": {
    "wingman": {
      "command": "/path/to/wingman-agent",
      "args": ["mcp"]
    }
  }
}
```

### Hermes（或其它 stdio MCP 客户端）

```json
{
  "mcpServers": {
    "wingman": {
      "command": "/path/to/wingman-agent",
      "args": ["mcp"],
      "transport": "stdio"
    }
  }
}
```

## Tools

全部 tool 的返回都是 `content: [{type: "text", text: "<JSON>"}]` 形状（结构化负载
统一包在文本块里，LLM 消费面一致）；tool 执行失败返回同一形状 + `isError: true`，
错误在 `content[0].text` 的 `{"error": ...}` 字段里——**不是**协议级 error。
例外：`wingman_screen_shot` 成功时返回 MCP image content block + 文本元数据。

| Tool | 平台 | 说明 |
|------|------|------|
| `wingman_uia_tree` | Windows* | 读 UI Automation 元素树（≤3 层），每个元素带 handle 供后续 tool 使用 |
| `wingman_uia_find` | Windows* | 按选择器查元素（name/id/className/role/text，至少一条件） |
| `wingman_uia_click` | Windows* | 语义点击元素（Invoke pattern）；失败明确报错，**绝不静默转坐标点击** |
| `wingman_uia_action` | Windows* | 对元素执行动作：click / doubleClick / rightClick / setFocus / select / expand / collapse / setText / setChecked |
| `wingman_screen_shot` | vision 构建 | 截屏（可指定 region `[x,y,w,h]`），返回 MCP image block（JPEG base64）+ 元数据 |
| `wingman_input_click` | 全平台 | 坐标兜底点击（mouseMove + mouseClick），button: left（默认）/ right |
| `wingman_script_run` | 有 Lua 引擎 | 沙箱运行 Lua 脚本，返回 print 输出；timeoutMs 协作超时（默认 30000） |

\* 平台无 UIA 后端（Linux/macOS 当前无实现）时返回明确错误
`"UI Automation is not supported on this platform"`，不假装成功。

### 示例：读前台窗口树并点击按钮

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05"}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"wingman_uia_tree","arguments":{}}}
```

`wingman_uia_tree` 响应的 text 负载（每个元素带 `handle`）：

```json
{"window": 12345, "elements": {"name": "主窗口", "role": "Window", "handle": 1,
  "children": [{"name": "确定", "role": "Button", "handle": 2, "children": []}]}}
```

```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"wingman_uia_click","arguments":{"handle":2}}}
```

元素定位也可用 `name`/`id` 子串选择器，不必先查树：

```json
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"wingman_uia_find","arguments":{"role":"Button","name":"确定"}}}
```

### 示例：脚本兜底

```json
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"wingman_script_run","arguments":{"code":"print(wingman.system.getcwd())","timeoutMs":5000}}}
```

text 负载：`{"ok": true, "output": "/home/user\n"}`；脚本超时/异常时
`ok: false` 且带 `error` 字段。

## FAQ

**Q: Linux / macOS 上 uia_* tool 报错？**
A: Wingman 的 UI Automation 后端目前仅 Windows 实现（UIA / UIAutomationClient）。
其它平台明确返回 `isError`「not supported」，请改用 `wingman_input_click`（坐标兜底）
或 `wingman_script_run`。

**Q: `wingman_screen_shot` 报 "screenshot requires a vision build"？**
A: 该构建未编入 OpenCV（vcpkg `WINGMAN_HAS_OPENCV=0`）。用启用 OpenCV 的构建，
或改用 `wingman_script_run` 里调用截图 API 的脚本。

**Q: `wingman_uia_click` 失败会转成坐标点击吗？**
A: 不会。UIA 语义点击失败（元素不支持 Invoke pattern 等）返回明确错误；
需要坐标兜底时由 LLM 显式调用 `wingman_input_click`。

**Q: 日志出现在协议流里？**
A: 不会。`mcp` 模式下全部日志走 stderr；stdout 逐行只输出 JSON-RPC 帧且每行 flush。
若见到 stdout 出现非 JSON 行，属于 bug（客户端会解析失败），请反馈。

**Q: 元素 handle 会失效吗？**
A: handle 注册表与会话同生命周期、只增不减；引用已销毁的窗口元素时由
UIA 后端返回失败并报 `invalid element handle` 或动作失败——重新查树即可。
