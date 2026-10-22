> [中文](overview.md) | **English**

# API Reference

Wingman provides a rich set of APIs to support game automation development.

## Table of Contents

- [Core APIs](#core-apis)
- [Data Persistence APIs](#data-persistence-apis)
- [Serialization APIs](#serialization-apis)
- [Scripting APIs](#scripting-apis)
- [Debugging APIs](#debugging-apis)

## Core APIs

The core APIs provide fundamental capabilities such as screen operations, input simulation, and window management.

### Module List

| Module | Functionality | Documentation |
|------|------|------|
| `screen` | Screen capture, color detection, image recognition | [screen](screen.md) |
| `input` | Mouse clicks, keyboard input, text input | [input](input.md) |
| `window` | Window lookup, activation, bounds retrieval | [window](window.md) |
| `process` | Process lookup and management | [process](process.md) |
| `vision` | Image recognition and matching | [vision](vision.md) |
| `ocr` | OCR text recognition | [ocr](ocr.md) |
| `smarttrigger` | Smart trigger system | [smart-trigger](smart-trigger.md) |
| `human` | Humanized simulation | [human](human.md) |
| `perf` | Performance monitoring | [perf](perf.md) |
| `system` | System information retrieval | [system](system.md) |
| `uia` | UI Automation control interaction | [uia](uia/index.md) |
| `bt` | Behavior tree engine | [behavior-tree](behavior-tree.md) |
| `security` | Security module | [security](security.md) |
| `crypto` | Cryptographic functions | [crypto](crypto.md) |
| `verification` | CAPTCHA recognition, TOTP | [verification](verification.md) |
| `gameprofile` | Game profiles | [gameprofile](gameprofile.md) |
| `util` | Utility functions | [util](util.md) |
| `ml` | ONNX model loading and input/output metadata queries (optional dependency) | [ml](ml.md) |
| `file` | File IO (read/write, move/copy, directory operations) | [file](file.md) |
| `filewatcher` | Filesystem watching | [filewatcher](filewatcher.md) |
| `clipboard` | Clipboard read/write | [clipboard](clipboard.md) |
| `hotkey` | Global hotkey listening | [hotkey](hotkey.md) |
| `systemwatch` | System event sources (process/window changes) | [systemwatch](systemwatch.md) |

### Quick Examples

#### Lua

```lua
local wingman = require("wingman")

-- 截图（返回是否成功，图像存于内部缓冲区）
local ok = wingman.screen.capture()
if not ok then
    print("截屏失败")
end

-- 查找颜色（返回 {point, found}，region 可选）
local result = wingman.screen.findColor(0xFF0000, {x=0, y=0, width=1920, height=1080}, 10)
if result.found then
    wingman.input.click(result.point.x, result.point.y)
end

-- 查找图像（返回 {point, found}，threshold 可选，默认 0.9）
local match = wingman.screen.findImage("target.png", {x=0, y=0, width=1920, height=1080}, 0.8)
if match.found then
    wingman.input.move(match.point.x, match.point.y, 500)
    wingman.input.click(match.point.x, match.point.y)
end
```

#### Python

```python
from wingman import screen, input

# 截图（返回是否成功，图像存于内部缓冲区）
ok = screen.capture()
if not ok:
    print("截屏失败")

# 查找颜色（返回 {point, found}，region 可选）
result = screen.findColor(0xFF0000, {"x": 0, "y": 0, "width": 1920, "height": 1080}, 10)
if result["found"]:
    input.click(result["point"]["x"], result["point"]["y"])

# 查找图像（返回 {point, found}，threshold 可选，默认 0.9）
match = screen.findImage("target.png", {"x": 0, "y": 0, "width": 1920, "height": 1080}, 0.8)
if match["found"]:
    input.move(match["point"]["x"], match["point"]["y"], 500)
    input.click(match["point"]["x"], match["point"]["y"])
```

## Data Persistence APIs

The data persistence APIs provide key-value storage and database support.

### Module List

| Module | Functionality | Documentation |
|------|------|------|
| `kv` | Key-value storage | [kv](kv.md) |
| `db` | SQLite database | [db](db.md) |

### Quick Examples

#### Lua

```lua
local wingman = require("wingman")

-- 键值存储
wingman.kv.set("username", "player1")
local name = wingman.kv.get("username")

-- 数据库操作
local conn = wingman.db.open("game_data")
wingman.db.execute(conn, "CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT)")
wingman.db.execute(conn, "INSERT INTO players (name) VALUES (?)", {"player1"})
local rows = wingman.db.query(conn, "SELECT * FROM players")
```

#### Python

```python
from wingman import kv, db

# 键值存储
kv.set("username", "player1")
name = kv.get("username")

# 数据库操作
conn = db.open("game_data")
db.execute(conn, "CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT)")
db.execute(conn, "INSERT INTO players (name) VALUES (?)", ["player1"])
rows = db.query(conn, "SELECT * FROM players")
```

## Serialization APIs

The serialization APIs provide support for multiple data formats.

### Module List

| Module | Functionality | Documentation |
|------|------|------|
| `json` | JSON encoding/decoding | [json](json.md) |
| `ini` | INI configuration file parsing | [ini](ini.md) |

### Quick Examples

#### Lua

```lua
local wingman = require("wingman")

-- JSON
local data = {name = "player1", level = 10}
local json_string = wingman.json.encode(data)
local decoded = wingman.json.decode(json_string)

-- INI
local config = wingman.ini.decode(ini_content)
local config_string = wingman.ini.encode(config)
```

#### Python

```python
from wingman import json, ini

# JSON
data = {"name": "player1", "level": 10}
json_string = json.encode(data)
decoded = json.decode(json_string)

# INI
config = ini.decode(ini_content)
config_string = ini.encode(config)
```

## Scripting APIs

The scripting APIs provide script execution and management functionality.

### Module List

| Module | Functionality | Documentation |
|------|------|------|
| `event` | Event system | [event](event.md) |
| `task` | Task management | [task](task.md) |
| `notify` | Notification system | [notify](notify.md) |
| `fsm` | Finite state machine | [fsm](fsm.md) |
| `http` | HTTP requests | [http](http.md) |
| `config` | Configuration management | [config](config.md) |
| `transport` | TCP/UDP networking | [transport](transport.md) |
| `script` | Script management control plane (listing, start/stop/reload) | [script](script.md) |
| `timer` | Timers (one-shot/periodic callbacks) | [timer](timer.md) |
| `macro` | Macro recording and playback | [macro](macro.md) |
| `orchestration` | Orchestration control | [orchestration](orchestration.md) |
| `inbox` | Message inbox | [inbox](inbox.md) |
| `team` | Team orchestration | [team](team.md) |
| `node` | Node management | [node](../guide/config.md) |

## Debugging APIs

The debugging APIs provide script debugging functionality.

### Module List

| Module | Functionality | Documentation |
|------|------|------|
| `debugger` | EmmyLua debugger | [debugger](debugger.md) |

### Quick Examples

#### Lua

```lua
local wingman = require("wingman")

-- 启动调试器
wingman.debugger.start()

-- 在当前位置设置断点
wingman.debugger.breakHere()
```

## Find APIs by Feature

### Screen and Image

- [**Screenshot**: `screen.capture()`, `screen.captureRegion()`]
- [**Color detection**: `screen.findColor()`, `screen.findColors()`]
- [**Image recognition**: `screen.findImage()`]
- [**OCR**: `ocr.recognize()`]

### Input and Control

- [**Mouse**: `input.click()`, `input.move()`, `input.scroll()`]
- **Keyboard**: `input.keyDown()`, `input.keyUp()`, `input.key()`
- [**Text**: `input.type()`]
- [**Humanized**: `human.mouse_move()`, `human.mouse_click()`, `human.keyboard_type()`]

### Window and Process

- [**Window**: `window.find()`, `window.activate()`, `window.getBounds()`]
- [**Process**: `process.find()`, `process.exists()`]

### Data and Configuration

- [**Storage**: `kv.set()`, `kv.get()`, `db.open()`]
- [**Configuration**: `json.encode()`, `json.decode()`, `ini.encode()`, `ini.decode()`]

### Automation

- [**Triggers**: `smarttrigger.create()`, `smarttrigger.start()`]
- [**Recording**: `macro.start()`, `macro.stop()`, `macro.playback()`]
- [**Events**: `event.on()`, `event.emit()`]
- [**Tasks**: `task.submit()`, `task.status()`]

### Networking

- [**HTTP**: `http.get()`, `http.post()`]

### Debugging

- [**Debugger**: `debugger.start()`, `debugger.breakpoint()`, `debugger.breakHere()`]

## API Naming Conventions

### Lua

- Unified import: `local wingman = require("wingman")`, with modules accessed as sub-tables: `wingman.screen`
- Method names follow what each module actually registers (e.g. `wingman.screen.capture()`, `wingman.uia.find_button()`)
- No constants are currently registered at the script layer

### Python

- Module names are lowercase: `from wingman import screen`
- Method names follow what each module actually registers (camelCase registered names also provide snake_case aliases; natively snake_case registered names exist in that one form only)

> [**Naming convention**: The Python binding layer automatically provides snake_case aliases for camelCase registered names (e.g. the `util` module's `getTime` can be written as `get_time`); natively snake_case registered names (e.g. the `uia` module's `find_by_name`) exist in that one form only, with no camelCase variant. It is recommended to check each module's documentation first to confirm registered names.]

## Type Conversion

### Lua to ScriptValue

| Lua Type | ScriptValue Type |
|----------|-----------------|
| `nil` | Null |
| `boolean` | Boolean |
| `number` | Integer/Float |
| `string` | String |
| `table` | Object/Array |
| `function` | Function |

### Python to ScriptValue

| Python Type | ScriptValue Type |
|-------------|-----------------|
| `None` | Null |
| `bool` | Boolean |
| `int` | Integer |
| `float` | Float |
| `str` | String |
| `dict` | Object |
| `list` | Array |
| `function` | Function |

## Error Handling

### Lua

```lua
local ok, result = pcall(function()
    return wingman.screen.capture()
end)

if not ok then
    print("Error:", result)
else
    -- result 为布尔值，表示是否截屏成功
end
```

### Python

```python
try:
    ok = screen.capture()
except Exception as e:
    print(f"Error: {e}")
```

## Related Documentation

- [Quick Start](../guide/getting-started.md)

---

**Back**: [Documentation Home](../README.md)
