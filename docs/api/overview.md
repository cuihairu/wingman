# API 参考

Wingman 提供了丰富的 API 来支持游戏自动化开发。

## 📋 目录

- [核心 API](#核心-api)
- [数据持久化 API](#数据持久化-api)
- [序列化 API](#序列化-api)
- [脚本 API](#脚本-api)
- [调试 API](#调试-api)

## 核心 API

核心 API 提供屏幕操作、输入模拟、窗口管理等基础功能。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `screen` | 屏幕截图、颜色检测、图像识别 | [screen](screen.md) |
| `input` | 鼠标点击、键盘输入、文本输入 | [input](input.md) |
| `window` | 窗口查找、激活、位置获取 | [window](window.md) |
| `process` | 进程查找和管理 | [process](process.md) |
| `vision` | 图像识别和匹配 | [vision](vision.md) |
| `ocr` | OCR 文字识别 | [ocr](ocr.md) |
| `smarttrigger` | 智能触发器系统 | [smart-trigger](smart-trigger.md) |
| `human` | 人性化模拟 | [human](human.md) |
| `perf` | 性能监控 | [perf](perf.md) |
| `system` | 系统信息获取 | [system](system.md) |
| `uia` | UI Automation 控件交互 | [uia](uia/index.md) |
| `bt` | 行为树引擎 | [behavior-tree](behavior-tree.md) |
| `security` | 安全模块 | [security](security.md) |
| `crypto` | 加密函数 | [crypto](crypto.md) |
| `verification` | 验证码识别、TOTP | [verification](verification.md) |
| `gameprofile` | 游戏配置档案 | [gameprofile](gameprofile.md) |
| `util` | 工具函数 | [util](util.md) |

### 快速示例

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

## 数据持久化 API

数据持久化 API 提供键值存储和数据库支持。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `kv` | 键值存储 | [kv](kv.md) |
| `db` | SQLite 数据库 | [db](db.md) |

### 快速示例

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

## 序列化 API

序列化 API 提供多种数据格式支持。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `json` | JSON 编码/解码 | [json](json.md) |
| `ini` | INI 配置文件解析 | [ini](ini.md) |

### 快速示例

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

## 脚本 API

脚本 API 提供脚本执行和管理功能。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `event` | 事件系统 | [event](event.md) |
| `task` | 任务管理 | [task](task.md) |
| `notify` | 通知系统 | [notify](notify.md) |
| `fsm` | 有限状态机 | [fsm](fsm.md) |
| `http` | HTTP 请求 | [http](http.md) |
| `config` | 配置管理 | [config](config.md) |
| `transport` | TCP/UDP 网络通信 | [transport](transport.md) |
| `orchestration` | 编排控制 | [orchestration](orchestration.md) |
| `inbox` | 消息收件箱 | [inbox](inbox.md) |
| `team` | 组队编排 | [team](team.md) |
| `node` | 节点管理 | [orchestration](orchestration.md) |

## 调试 API

调试 API 提供脚本调试功能。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `debugger` | EmmyLua 调试器 | [debugger](debugger.md) |

### 快速示例

#### Lua

```lua
local wingman = require("wingman")

-- 启动调试器
wingman.debugger.start()

-- 在当前位置设置断点
wingman.debugger.breakHere()
```

## 按功能查找 API

### 屏幕和图像

- 📸 **截图**: `screen.capture()`, `screen.captureRegion()`
- 🎨 **颜色检测**: `screen.findColor()`, `screen.findColors()`
- 🖼️ **图像识别**: `screen.findImage()`
- 📖 **OCR**: `ocr.recognize()`

### 输入和控制

- 🖱️ **鼠标**: `input.click()`, `input.move()`, `input.scroll()`
- ⌨️ **键盘**: `input.keyDown()`, `input.keyUp()`, `input.key()`
- 📝 **文本**: `input.type()`
- 🎯 **人性化**: `human.mouse_move()`, `human.mouse_click()`, `human.keyboard_type()`

### 窗口和进程

- 🪟 **窗口**: `window.find()`, `window.activate()`, `window.getBounds()`
- 📊 **进程**: `process.find()`, `process.exists()`

### 数据和配置

- 💾 **存储**: `kv.set()`, `kv.get()`, `db.open()`
- 📄 **配置**: `json.encode()`, `json.decode()`, `ini.encode()`, `ini.decode()`

### 自动化

- ⚡ **触发器**: `smarttrigger.create()`, `smarttrigger.start()`
- 📼 **录制**: `macro.start()`, `macro.stop()`, `macro.playback()`
- 🔔 **事件**: `event.on()`, `event.emit()`
- 📋 **任务**: `task.submit()`, `task.status()`

### 网络

- 🌐 **HTTP**: `http.get()`, `http.post()`

### 调试

- 🐛 **调试器**: `debugger.start()`, `debugger.breakpoint()`, `debugger.breakHere()`

## API 命名约定

### Lua

- 统一引入: `local wingman = require("wingman")`，模块作为子表访问: `wingman.screen`
- 方法使用驼峰命名: `wingman.screen.capture()`
- 常量使用全大写: `KEY_CTRL`

### Python

- 模块使用小写: `from wingman import screen`
- 方法使用蛇形命名: `screen.capture()`
- 常量使用全大写: `KEY_CTRL`

> ⚠️ **注意**：当前 Python 绑层未做 snake_case 转换，Python 调用须使用代码注册名（如 `getForeground` 而非 `get_foreground`），snake_case 支持规划中。

## 类型转换

### Lua 到 ScriptValue

| Lua 类型 | ScriptValue 类型 |
|----------|-----------------|
| `nil` | Null |
| `boolean` | Boolean |
| `number` | Integer/Float |
| `string` | String |
| `table` | Object/Array |
| `function` | Function |

### Python 到 ScriptValue

| Python 类型 | ScriptValue 类型 |
|-------------|-----------------|
| `None` | Null |
| `bool` | Boolean |
| `int` | Integer |
| `float` | Float |
| `str` | String |
| `dict` | Object |
| `list` | Array |
| `function` | Function |

## 错误处理

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

## 🔗 相关文档

- [快速开始](../getting-started.md)
- [用户指南](../user-guide.md)
- [快速开始](../getting-started.md)

---

**返回**: [文档首页](../README.md)
