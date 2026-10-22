---
title: Script Development Guide
---

> [中文](script-development.md) | **English**

# Script Development Guide

Wingman scripts are written in **Lua** or **Python** and drive automation through the unified `wingman` module (screen / input / vision / trigger / event, etc.). This page covers the complete flow from your first script to debugging.

## Choosing a Language

| | Lua | Python |
|---|---|---|
| Startup speed | Extremely fast | Slower (interpreter cold start) |
| Dependencies | Lua 5.5 only (sol2 bindings) | Python 3 (vcpkg `python3` port, currently 3.12.x; embedded via pybind11) |
| Best suited for | High-frequency, low-latency, embedded scenarios | Complex logic, reusing Python third-party libraries |
| Editing/debugging | VS Code + EmmyLua (recommended) | VS Code + Python extension |

> For the local GUI, script **editing happens exclusively in VS Code**; the GUI only loads, runs, and stops scripts. Only the remote Dashboard has a built-in editor.

> **The Python engine is off by default**: officially released runtime packages (release/nightly) bundle only the Lua engine, and running a `.py` script directly reports that the engine does not exist. To use Python you must build it yourself: add `-DWINGMAN_ENABLE_PYTHON=ON` to CMake (the vcpkg manifest `python` feature, which pulls in the `python3` + `pybind11` ports; do not mix in the system CPython). This dimension is covered in the main CI: the `cpp-linux-python-tests` job runs the full test suite with Python enabled on Linux, and the `cpp-python` job validates the Windows link chain.

## Your First Script

### Lua

`hello.lua`:

```lua
local wingman = require("wingman")

print("Hello Wingman")
local w, h = wingman.screen.getScreenWidth(), wingman.screen.getScreenHeight()
print(string.format("屏幕: %dx%d", w, h))
```

Run it:

```bash
wingman-agent script hello.lua
```

### Python

`hello.py`:

```python
from wingman import screen, util

print("Hello Wingman")
w, h = screen.getScreenWidth(), screen.getScreenHeight()
print(f"屏幕: {w}x{h}")
```

Run it (requires a build with the Python engine enabled; see "The Python engine is off by default" above):

```bash
wingman-agent script hello.py
```

> On the Python side the `wingman` module surface is identical to Lua's (a unified, engine-agnostic registration), and module functions are exposed under both the original camelCase and snake_case names — `screen.getScreenWidth()` and `screen.get_screen_width()` are equivalent, the latter being PEP 8-compliant. `libs/python/typing/` provides `.pyi` type stubs.

## Core Modules at a Glance

See the "API" section in the sidebar for the complete API. The most used modules:

| Module | Typical Use | Representative Functions |
|------|----------|----------|
| `screen` | Pixel/image detection, screenshots | `findColor`, `findImage`, `capture`, `getScreenWidth` |
| `input` | Mouse/keyboard simulation (with humanization) | `click`, `move`, `key`, `type` |
| `vision` | Advanced vision (dominant color, template matching, AI detection) | `getDominantColor`, `findImage` |
| `trigger` | Conditional triggers | Color appears/disappears, image, hotkey, timer |
| `event` | Publish-subscribe events | `on`, `emit` |
| `task` | Task orchestration | Create/wait/cancel |
| `notify` | Notifications (log/toast) | `info`, `warn`, `error`, `toast` |
| `http` | HTTP client | `get`, `post`, `put`, `delete` |
| `human` | Humanized input (anti-detection) | Random delays, trajectory-based movement |
| `util` | Utilities (time, sleep, logging) | `sleep`, `getTime`, `log` |
| `kv` / `db` | Persistence | Key-value store, SQLite |

> Note: in Lua everything is accessed through the `wingman.<module>.<fn>` namespace after `require("wingman")`; there are no bare global modules. In Python everything is imported from the `wingman` package. Treat real usage in [examples/lua_scripts/](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) as the source of truth.

## Sandbox Mode

Both local execution paths — the CLI `wingman-agent script` command and the local GUI (Standalone mode) — **force sandboxed execution** (`apps/agent/src/commands/script_command.cpp` and `apps/agent/src/standalone_mode.cpp` hard-code `sandboxed = true`). When loading via the `wingman.script.load` API, the `sandboxed` config can be specified separately (see the [script API](../api/script.md)). The sandbox surface of both engines:

**Lua**:

- The globals `io`, `os`, `debug`, and `package`, along with `dofile` / `loadfile` / `load`, are removed — scripts have no disk-read or system-call path.
- `require` is a controlled shim: only `require("wingman")` resolves to the wingman module table; any other module name errors with `sandbox: module not allowed: <name>`.
- Common substitutes: time via `wingman.util.getTime()` (epoch milliseconds), files via `wingman.file.read` / `write` / `exists`, waiting via `wingman.util.sleep(ms)`.
- No `os.getenv`: keys injected through the script config `env` are read as `_ENV_<KEY>` global variables.

**Python**:

- `__builtins__` is replaced with a whitelist (`print` / `len` / `range`, basic types, exception classes, etc.), and the `os` / `sys` / `subprocess` / `io` / `importlib` aliases are cleared.
- `import` is taken over by a controlled shim: only `import wingman` (including `wingman.*` submodules and `from wingman import <module>`) is allowed; everything else — including the standard library — raises `ImportError: sandbox: module not allowed: <name>`. So do not `import time`; wait with `wingman.util.sleep(ms)`.
- Under the sandbox, `env` injection is disabled (there is no `os` to read environment variables).

## Typical Patterns

### Pixel Detection + Click (Color Trigger)

```lua
local wingman = require("wingman")

-- findColor(color, region 表, tolerance) 返回单数组：res[1] = 命中点，res[2] = 是否找到
-- （绑定层只回传单个 table，Lua 侧不会展开成多返回值）
local res = wingman.screen.findColor(0xFF0000, {x = 0, y = 0, width = 1920, height = 1080}, 10)
if res[2] then
    wingman.input.click(res[1].x, res[1].y)
end
```

### Loop Monitoring (with Exit Condition)

```lua
local wingman = require("wingman")

for i = 1, 100 do
    local res = wingman.screen.findColor(0x00FF00, {x = 0, y = 0, width = 1920, height = 1080}, 10)
    if res[2] then
        wingman.util.log("检测到目标: " .. res[1].x .. "," .. res[1].y)
        break
    end
    wingman.util.sleep(500)  -- 500ms 间隔
end
```

> When you need all matching points, use `wingman.screen.findColors(color, region, tolerance, maxCount?)` instead, which returns an array of points. `window.find` returns a single array `{handle, found}`, just like `findColor`.

For more patterns (image matching, hotkey triggers, HTTP integration, macro record/replay), see [examples](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts) and the [Trigger Guide](../guides/triggers.md).

## Ways to Run

| Mode | Command | Best For |
|------|------|------|
| Single script | `wingman-agent script foo.lua` | One-off tasks, quick verification |
| Local GUI | `wingman-agent start` (local capability) + Tauri GUI | Everyday single-machine use with a UI |
| Remote orchestration | `wingman-agent start` (agent capability) → Go server → Dashboard | Centralized multi-machine control |

For the control paths and architecture constraints of the three modes, see [Getting Started - Run Modes](./getting-started.md#运行模式) and [Communication Protocols](../protocols.md).

## Function Naming Conventions

Module functions are registered in camelCase in their C++ descriptors (e.g. `joinTeam`, `getEventCount`). Both script surfaces can use them:

- **Lua**: only the original camelCase name (`wingman.team.joinTeam(...)`).
- **Python**: the original camelCase name and the snake_case alias are **exposed simultaneously** (`wingman.team.join_team(...)` is equivalent to `wingman.team.joinTeam(...)`; the aliasing rule matches the engine's internal `camelToSnake`). The Python typing stubs (`libs/python/typing/wingman/*.pyi`) use snake_case as the primary form, with docstrings noting the corresponding camelCase form.

A few functions are registered directly in snake_case (e.g. `db.table_insert`, `human.mouse_move`); in that case both languages share the same name. Hand-written pythonic aliases defined in user scripts are not bound by this convention (e.g. `timer.after`).

**Drift guard**: `python3 scripts/audit_pyi_drift.py` compares the C++ module descriptors against the pyi stubs (module surface, function surface, `__init__` export surface) and exits non-zero on drift; the `api-typing-audit` job in CI enforces the same check.

## Debugging (Lua / EmmyLua)

1. Install the `tangzx.emmylua` extension in VS Code (already recommended by the repository's `.vscode/extensions.json`).
2. Completion: the project ships [assets/vscode-wingman-lua/wingman.d.lua](https://github.com/cuihairu/wingman/tree/main/assets/vscode-wingman-lua), providing type hints for the `wingman.*` modules.
3. Breakpoint debugging: the runtime listens on `:9966` (EmmyLua attach mode). The VS Code `launch.json` is already configured; press F5 to attach.
4. See the [Debugging Guide](./debugging.md) and the [VS Code Development Environment](../development-environment.md) for details.

## Next Steps

- [Getting Started](./getting-started.md)
- [API Reference](../api/script.md)
- [Example Scripts](https://github.com/cuihairu/wingman/tree/main/examples/lua_scripts)
- [Communication Protocols](../protocols.md)
