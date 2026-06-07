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
| `screen` | 屏幕截图、颜色检测、图像识别 | [核心 API](core.md#screen) |
| `input` | 鼠标点击、键盘输入、文本输入 | [核心 API](core.md#input) |
| `window` | 窗口查找、激活、位置获取 | [核心 API](core.md#window) |
| `process` | 进程查找和管理 | [核心 API](core.md#process) |
| `trigger` | 触发器系统 | [核心 API](core.md#trigger) |
| `vision` | 图像识别和匹配 | [核心 API](core.md#vision) |
| `ocr` | OCR 文字识别 | [核心 API](core.md#ocr) |
| `clipboard` | 剪贴板操作 | [核心 API](core.md#clipboard) |
| `filewatcher` | 文件监控 | [核心 API](core.md#filewatcher) |
| `recorder` | 宏录制和回放 | [核心 API](core.md#recorder) |

### 快速示例

#### Lua

```lua
local wingman = require("wingman")

-- 截图
local screenshot = wingman.screen.capture(0, 0, 1920, 1080)
screenshot:save("screen.png")

-- 查找颜色
local points = wingman.screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    wingman.input.click(points[1].x, points[1].y)
end

-- 查找图像
local match = wingman.screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
if match then
    wingman.input.move(match.x, match.y, 500)
    wingman.input.click(match.x, match.y)
end
```

#### Python

```python
from wingman import screen, input

# 截图
screenshot = screen.capture(0, 0, 1920, 1080)
screenshot.save("screen.png")

# 查找颜色
points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    input.click(points[0]["x"], points[0]["y"])

# 查找图像
match = screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
if match:
    input.move(match["x"], match["y"], 500)
    input.click(match["x"], match["y"])
```

## 数据持久化 API

数据持久化 API 提供键值存储和数据库支持。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `kv` | 键值存储 | [数据持久化](storage.md#kv-键值存储) |
| `db` | SQLite 数据库 | [数据持久化](storage.md#sqlite-数据库-db) |

### 快速示例

#### Lua

```lua
local kv = require("wingman.kv")
local db = require("wingman.db")

-- 键值存储
kv.set("username", "player1")
local name = kv.get("username")

-- 数据库操作
local conn = db.connect("game_data")
conn:execute("CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT)")
conn:execute("INSERT INTO players (name) VALUES (?)", {"player1"})
local rows = conn:query("SELECT * FROM players")
```

#### Python

```python
from wingman import kv, db

# 键值存储
kv.set("username", "player1")
name = kv.get("username")

# 数据库操作
conn = db.connect("game_data")
conn.execute("CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT)")
conn.execute("INSERT INTO players (name) VALUES (?)", ["player1"])
rows = conn.query("SELECT * FROM players")
```

## 序列化 API

序列化 API 提供多种数据格式支持。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `json` | JSON 编码/解码 | [序列化](serialization.md#json-模块) |
| `ini` | INI 配置文件解析 | [序列化](serialization.md#ini-模块) |

### 快速示例

#### Lua

```lua
local json = require("wingman.json")
local ini = require("wingman.ini")

-- JSON
local data = {name = "player1", level = 10}
local json_string = json.encode(data)
local decoded = json.decode(json_string)

-- INI
local config = ini.decode(ini_content)
local config_string = ini.encode(config)
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
| `script` | 脚本执行和管理 | [脚本 API](script.md) |
| `event` | 事件系统 | [脚本 API](script.md#event) |
| `task` | 任务管理 | [脚本 API](script.md#task) |
| `notify` | 通知系统 | [脚本 API](script.md#notify) |
| `fsm` | 有限状态机 | [脚本 API](script.md#fsm) |
| `http` | HTTP 请求 | [脚本 API](script.md#http) |
| `config` | 配置管理 | [脚本 API](script.md#config) |

## 调试 API

调试 API 提供脚本调试功能。

### 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `debug` | EmmyLua 调试器 | [调试 API](debugging.md) |

### 快速示例

#### Lua

```lua
local debug = require("wingman.debug")

-- 启动调试器
debug.start()

-- 设置断点
debug.breakpoint()

-- 输出调试信息
debug.log("Variable value:", some_var)
```

## 按功能查找 API

### 屏幕和图像

- 📸 **截图**: `screen.capture()`
- 🎨 **颜色检测**: `screen.findColor()`, `screen.findColors()`
- 🖼️ **图像识别**: `screen.findImage()`, `screen.findImages()`
- 🔍 **图像匹配**: `screen.matchTemplate()`
- 📖 **OCR**: `ocr.recognize()`

### 输入和控制

- 🖱️ **鼠标**: `input.click()`, `input.move()`, `input.scroll()`
- ⌨️ **键盘**: `input.keyDown()`, `input.keyUp()`, `input.keyPress()`
- 📝 **文本**: `input.text()`
- 🎯 **人性化**: `input.moveHuman()`, `input.clickHuman()`

### 窗口和进程

- 🪟 **窗口**: `window.find()`, `window.activate()`, `window.getBounds()`
- 📊 **进程**: `process.find()`, `process.exists()`

### 数据和配置

- 💾 **存储**: `kv.set()`, `kv.get()`, `db.connect()`
- 📄 **配置**: `json.encode()`, `json.decode()`, `ini.encode()`, `ini.decode()`

### 自动化

- ⚡ **触发器**: `trigger.create()`, `trigger.start()`
- 📼 **录制**: `recorder.start()`, `recorder.stop()`, `recorder.play()`
- 🔔 **事件**: `event.on()`, `event.emit()`
- 📋 **任务**: `task.create()`, `task.start()`

### 网络

- 🌐 **HTTP**: `http.get()`, `http.post()`, `http.request()`

### 调试

- 🐛 **调试器**: `debug.start()`, `debug.breakpoint()`, `debug.log()`

## API 命名约定

### Lua

- 模块使用小写: `require("wingman.screen")`
- 方法使用驼峰命名: `screen.capture()`
- 常量使用全大写: `KEY_CTRL`

### Python

- 模块使用小写: `from wingman import screen`
- 方法使用蛇形命名: `screen.capture()`
- 常量使用全大写: `KEY_CTRL`

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
    return wingman.screen.capture(0, 0, 1920, 1080)
end)

if not ok then
    print("Error:", result)
else
    -- 使用 result
end
```

### Python

```python
try:
    screenshot = screen.capture(0, 0, 1920, 1080)
except Exception as e:
    print(f"Error: {e}")
```

## 🔗 相关文档

- [快速开始](../getting-started.md)
- [Lua 脚本指南](../guides/lua-scripting.md)
- [Python 脚本指南](../guides/python-scripting.md)

---

**返回**: [文档首页](../README.md)
