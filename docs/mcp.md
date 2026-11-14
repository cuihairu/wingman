> **English** | [中文](zh/mcp.md)

# MCP Server

Wingman Agent ships a built-in **MCP (Model Context Protocol) server** over
**stdio transport** (newline-delimited JSON-RPC 2.0, no Content-Length framing)
that exposes its UI automation capabilities. An AI client (Claude Code /
Cursor / Hermes, etc.) spawns `wingman-agent mcp` locally and can then operate
the target app's UI Automation element tree semantically — replacing the
"screenshot → LLM guesses coordinates → blind click" pipeline, with no
coordinate drift.

```bash
# After startup, stdin/stdout is the protocol stream (one JSON-RPC frame per line)
wingman-agent mcp
```

- **stdout carries protocol frames only**; all logging (crash collection,
  script output included) goes to stderr. The client reads stdout line by
  line; every response is flushed per line.
- Protocol version negotiation: an explicitly requested supported version is
  echoed back; anything else falls back to the server's latest
  (currently `2025-11-25`; `2025-06-18`, `2025-03-26` and `2024-11-05`
  are also supported).

## Connecting

### Claude Code

```bash
claude mcp add wingman -- /path/to/wingman-agent mcp
```

### Cursor

`Settings → MCP → Add new global MCP server`, or edit `~/.cursor/mcp.json`:

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

### Hermes (or any other stdio MCP client)

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

Every tool returns `content: [{type: "text", text: "<JSON>"}]` (structured
payloads are uniformly wrapped in a text block so the LLM-facing shape is
consistent); a failed tool call returns the same shape plus `isError: true`,
with the error inside the `{"error": ...}` field of `content[0].text` —
**not** a protocol-level error. Exception: on success `wingman_screen_shot`
returns an MCP image content block plus textual metadata.

| Tool | Platform | Description |
|------|----------|-------------|
| `wingman_uia_tree` | Windows* | Read the UI Automation element tree (up to 3 levels); every element carries a handle for subsequent tools |
| `wingman_uia_find` | Windows* | Find elements by selector (name/id/className/role/text, at least one criterion) |
| `wingman_uia_click` | Windows* | Semantic click on an element (Invoke pattern); failures are explicit errors, **never a silent fallback to coordinate clicks** |
| `wingman_uia_action` | Windows* | Invoke an action on an element: click / doubleClick / rightClick / setFocus / select / expand / collapse / setText / setChecked |
| `wingman_screen_shot` | vision build | Capture the screen (optional region `[x,y,w,h]`); returns an MCP image block (JPEG base64) plus metadata |
| `wingman_input_click` | all platforms | Coordinate fallback click (mouseMove + mouseClick); button: left (default) / right |
| `wingman_script_run` | Lua engine present | Run a sandboxed Lua script and return its printed output; timeoutMs is a cooperative timeout (default 30000) |

\* On a platform without a UIA backend (Linux/macOS have no implementation
today) the tool returns the explicit error
`"UI Automation is not supported on this platform"` instead of pretending
to succeed.

### Example: read the foreground window tree and click a button

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05"}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"wingman_uia_tree","arguments":{}}}
```

Text payload of the `wingman_uia_tree` response (each element has a `handle`):

```json
{"window": 12345, "elements": {"name": "Main Window", "role": "Window", "handle": 1,
  "children": [{"name": "OK", "role": "Button", "handle": 2, "children": []}]}}
```

```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"wingman_uia_click","arguments":{"handle":2}}}
```

Elements can also be located by `name`/`id` substring selectors without
walking the tree first:

```json
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"wingman_uia_find","arguments":{"role":"Button","name":"OK"}}}
```

### Example: script fallback

```json
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"wingman_script_run","arguments":{"code":"print(wingman.system.getcwd())","timeoutMs":5000}}}
```

Text payload: `{"ok": true, "output": "/home/user\n"}`; on script timeout or
error, `ok: false` with an `error` field.

## FAQ

**Q: uia_* tools fail on Linux / macOS?**
A: Wingman's UI Automation backend is currently Windows-only (UIA /
UIAutomationClient). Other platforms return an explicit `isError`
"not supported"; use `wingman_input_click` (coordinate fallback) or
`wingman_script_run` instead.

**Q: `wingman_screen_shot` says "screenshot requires a vision build"?**
A: That build was compiled without OpenCV (vcpkg `WINGMAN_HAS_OPENCV=0`).
Use a build with OpenCV enabled, or call the screenshot API from a
`wingman_script_run` script.

**Q: Does a failed `wingman_uia_click` fall back to a coordinate click?**
A: No. A failed UIA semantic click (element without an Invoke pattern, etc.)
returns an explicit error; coordinate fallback is the LLM's explicit call to
`wingman_input_click`.

**Q: Logs appear in the protocol stream?**
A: They shouldn't. In `mcp` mode all logging goes to stderr; stdout emits
only JSON-RPC frames, flushed per line. Non-JSON lines on stdout are a bug
(clients would fail to parse) — please report it.

**Q: Can element handles go stale?**
A: The handle registry lives for the session and only grows. Referencing a
destroyed window element fails in the UIA backend with
`invalid element handle` or an action failure — re-query the tree.
