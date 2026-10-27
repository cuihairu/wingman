> [中文](../zh/api/index.md) | **English**

# API Reference

Wingman's scripting SDK treats **Lua** as a first-class citizen (enabled by default, tested along with the main build and CI) and offers **Python** as an optional backend (embedded CPython + pybind11, off by default, must be enabled explicitly via `WINGMAN_ENABLE_PYTHON=ON`). Both share the same set of module functionality and the `IScriptEngine` abstraction.

## Why Two Languages

Wingman supports both Python and Lua in order to serve different users' needs:

### Python

**Suitable for**:
- Developers with programming experience
- Automation tasks that involve complex logic
- Work that needs third-party libraries (network requests, data processing, etc.)
- Users who value development efficiency and code maintainability

**Strengths**:
- Third-party libraries install directly via pip
- Type friendly: supports type hints with solid IDE autocompletion
- Easy debugging: mature debugging tools and error tracing
- Code organization: full object-oriented features including modules, classes, and functions

**Typical scenarios**:
- Complex game assistance tools (needing network sync, database storage)
- Work that calls AI/vision capabilities (OCR, ONNX model loading, etc.)
- Large automation projects developed by teams

### Lua

**Suitable for**:
- Gamers or light-programming users
- Situations that call for quick, simple scripts
- Resource-constrained environments (embedded, low-spec devices)
- Users who care about startup speed and memory footprint

**Strengths**:
- Lightweight: small interpreter (< 1MB), fast startup
- Easy to learn: concise syntax, quick to pick up
- Easy deployment: single-file scripts, no dependency management
- Game industry: many games' macro systems use Lua, so users may already know it

**Typical scenarios**:
- Simple AFK-farming scripts
- Minor tweaks to recorded in-game macros
- Quickly testing automation ideas

---

## SDK Design Philosophy

### Naming Conventions

Both languages follow their respective communities' best practices:

**Python (snake_case)**:
```python
screen.get_pixel(x, y)
screen.find_image(path, region)
uia.find_button("确定")
http.post(url, data)
```

**Lua (camelCase)**:
```lua
screen.getPixel(x, y)
screen.findImage(path, region)
uia.find_button("确定")
http.post(url, data)
```

> Note: Each module's actually registered names are authoritative. Modules such as `screen`/`input`/`window` are registered with camelCase names, and the Python binding layer automatically provides snake_case aliases; the `uia` module's registered names are themselves snake_case (same names in both languages, no camelCase forms on the Python side).

### Modular Design

All functionality is divided into modules, clear and well-defined:

| Module | Function | Typical use |
|-----|------|---------|
| `screen` | Screen operations | Screenshots, image finding, pixel sampling |
| `input` | Input simulation | Mouse clicks, keyboard input |
| `window` | Window management | Finding windows, activating windows |
| `process` | Process management | Launching games, checking processes |
| `uia` | UI Automation | Reliable control manipulation |
| `http` | Network requests | API calls, data syncing |
| `kv` | Data storage | Persisting script state |

### Consistency Principle

The two languages' APIs are fully equivalent in functionality; only the syntax adapts to each language's conventions:

**Same functionality, different syntax**:

```python
# Python
from wingman import screen

img = screen.capture()
result = screen.find_image("button.png", {"x": 0, "y": 0, "width": 1920, "height": 1080})
# result 为数组 [point, found]（point 为 {x, y} 对象）
```

```lua
local wingman = require("wingman")

-- Lua
local img = wingman.screen.capture()
local result = wingman.screen.findImage("button.png", {x = 0, y = 0, width = 1920, height = 1080})
-- result 为数组 {point, found}（point 为 {x, y} 对象）
```

---

## Design Trade-offs

### Why Not JavaScript / TypeScript

- **Execution efficiency**: the V8 engine is large and slow to start
- **Deployment complexity**: requires a Node.js environment, a high barrier for ordinary users
- **Game conflicts**: many games embed JS engines in their browser components, which may conflict

### Why Not C# / .NET

- **Runtime dependency**: needs the .NET Runtime, unfriendly to deploy
- **Compilation step**: users would have to compile, which clashes with scripts' rapid iteration
- **Learning curve**: too heavy for light users

### Python vs Lua Performance

How the two languages behave in practice:

**Python strengths**:
- Rich third-party library support (numpy, pandas, requests, etc.)
- Mature development tools and debuggers
- Type hints and IDE autocompletion
- Suited to complex logic and large-scale projects

**Lua strengths**:
- Small interpreter, fast startup
- Relatively low memory footprint
- Concise syntax, small script files
- Easy deployment, no extra dependencies

**Recommendations**:
- Simple scripts, quick prototypes → Lua
- Complex projects, third-party libraries needed → Python

---

## Quick Start

### Python

```python
from wingman import screen, input

# 截屏（返回是否成功，图像存于内部缓冲区）
ok = screen.capture()

# 全屏找图（region 表；返回数组 [point, found]）
result = screen.find_image("button.png", {"x": 0, "y": 0, "width": 1920, "height": 1080})

if result[1]:
    point = result[0]
    input.click(point["x"], point["y"])
```

### Lua

```lua
local wingman = require("wingman")

-- 截屏（返回是否成功，图像存于内部缓冲区）
local ok = wingman.screen.capture()

-- 全屏找图（region 表；返回数组 {point, found}）
local result = wingman.screen.findImage("button.png", {x = 0, y = 0, width = 1920, height = 1080})

if result[2] then
    wingman.input.click(result[1].x, result[1].y)
end
```

---

## Data Types

See the [Data Types reference](./types.md) for the various objects and data structures used in the API.

---

## API Modules

### Core Modules

Foundational operation modules providing core functionality such as screen, input, window, and process.

- [screen](../zh/api/screen.md) - Screen capture, pixel lookup, image matching
- [input](../zh/api/input.md) - Mouse clicks, movement, keyboard input
- [window](../zh/api/window.md) - Finding, activating, and inspecting windows
- [process](../zh/api/process.md) - Starting, finding, and terminating processes
- [system](../zh/api/system.md) - Getting system information
- [uia](./uia/) - UI Automation control interaction

### Vision and AI

Computer vision and AI-related functionality.

- [vision](../zh/api/vision.md) - Color detection, edge detection, contour recognition
- [ocr](./ocr.md) - Text recognition (OCR, optional dependency)
- [ml](../zh/api/ml.md) - ONNX model loading and input/output metadata queries (optional dependency)

### Automation Systems

Advanced automation orchestration.

- [event](../zh/api/event.md) - Event subscribe/publish system
- [fsm](./fsm.md) - Finite state machine (FSM)
- [task](../zh/api/task.md) - Task orchestration and execution
- [script](../zh/api/script.md) - Script management control plane (listing, start/stop/reload, environment variables)
- [notify](../zh/api/notify.md) - Notification system (logging, Toast, Webhook, tray intents)
- [smarttrigger](../zh/api/smart-trigger.md) - Smart triggers
- [bt](./behavior-tree.md) - Behavior tree engine
- [hotkey](./hotkey.md) - Global hotkey listening (polling-based, Python callbacks)
- [systemwatch](./systemwatch.md) - System event source: process/window change observation (polling-based, Python callbacks)

### Networking

Network requests and communication.

- [http](../zh/api/http.md) - HTTP client (GET, POST, downloads)
- [transport](../zh/api/transport.md) - TCP client, server, and UDP sockets

### Serialization

Data serialization and format conversion.

- [serialize](./serialize.md) - Serialization format overview (json, yaml, ini selection guide)
- [json](./json.md) - JSON parsing and serialization
- [ini](../zh/api/ini.md) - INI configuration file parsing and encoding

### Data Storage

Local data persistence and caching.

- [data](./data.md) - Data storage overview (kv vs db selection guide)
- [kv](../zh/api/kv.md) - Persistent key-value store (Redis-like API)
- [db](../zh/api/db.md) - Local SQLite database (SQL and ORM supported)

### Utilities

Tool modules that assist automation scripts.

- [human](../zh/api/human.md) - Human-like simulation (random delays, irregular input)
- [perf](../zh/api/perf.md) - Performance monitoring
- [config](./config.md) - Configuration management
- [verification](./verification.md) - CAPTCHA recognition, TOTP
- [gameprofile](./gameprofile.md) - Game configuration profiles
- [debugger](./debugger.md) - Debugger
- [util](./util.md) - Utility functions (sleep, random numbers, etc.)
- [timer](./timer.md) - Timer (one-shot/periodic callbacks, sleep)
- [file](../zh/api/file.md) - File IO (read/write, move/copy, directory operations)

### Security and Encryption

Security- and encryption-related functionality.

- [security](./security.md) - Security module
- [crypto](../zh/api/crypto.md) - Cryptographic functions
- [clipboard](../zh/api/clipboard.md) - Clipboard read/write
- [macro](../zh/api/macro.md) - Macro recording and playback
- [filewatcher](./filewatcher.md) - File system watching

### Orchestration and Collaboration

Multi-instance collaboration and orchestration.

- [orchestration](./orchestration.md) - Orchestration control
- [inbox](../zh/api/inbox.md) - Message inbox
- [team](../zh/api/team.md) - Team orchestration
- [node](../zh/guide/config.md) - Node status and heartbeat (documented in the "Node module (node)" section of the configuration system)
- [script](../zh/api/script.md) - Script management control plane
