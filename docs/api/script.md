# 脚本 API

脚本 API 提供脚本执行、事件系统、任务管理、通知、状态机等功能。

## 📋 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `event` | 事件系统 | [event](event.md) |
| `task` | 任务管理 | [task](task.md) |
| `notify` | 通知系统 | [notify](notify.md) |
| `fsm` | 有限状态机 | [fsm](fsm.md) |
| `http` | HTTP 请求 | [http](http.md) |
| `config` | 配置管理 | [config](config.md) |
| `transport` | TCP/UDP 网络通信 | [transport](transport.md) |

---

## Event - 事件系统

事件系统提供发布-订阅模式的事件处理机制。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import event
```

### 函数列表

#### `on(eventName, handler)`

订阅事件。

**参数:**
- `eventName` (string): 事件名称
- `handler` (function): 事件处理函数

**返回:** 事件 ID（用于取消订阅）

**示例:**
```lua
-- Lua
local id = event.on("keydown", function(key)
    print("Key pressed:", key)
end)

-- 带过滤器的订阅
event.on("keydown", function(key)
    if key == "F1" then
        print("F1 pressed!")
    end
end, {key = "F1"})
```

```python
# Python
def on_keydown(key):
    print(f"Key pressed: {key}")

id = event.on("keydown", on_keydown)

# 带过滤器的订阅
def on_f1(key):
    if key == "F1":
        print("F1 pressed!")

event.on("keydown", on_f1, {"key": "F1"})
```

#### `off(eventId)`

取消订阅事件。

**参数:**
- `eventId` (number): 事件 ID

**示例:**
```lua
-- Lua
event.off(id)
```

```python
# Python
event.off(id)
```

#### `emit(eventName, data)`

触发事件。

**参数:**
- `eventName` (string): 事件名称
- `data` (any): 事件数据

**示例:**
```lua
-- Lua
event.emit("customEvent", {message = "Hello", value = 42})
```

```python
# Python
event.emit("customEvent", {"message": "Hello", "value": 42})
```

#### `once(eventName, handler)`

订阅一次性事件（触发后自动取消订阅）。

**参数:**
- `eventName` (string): 事件名称
- `handler` (function): 事件处理函数

**返回:** 事件 ID

**示例:**
```lua
-- Lua
event.once("startup", function()
    print("This will only run once")
end)
```

```python
# Python
event.once("startup", lambda: print("This will only run once"))
```

### 内置事件

| 事件名 | 触发时机 | 数据 |
|--------|----------|------|
| `keydown` | 按键按下 | `{key: 按键名}` |
| `keyup` | 按键释放 | `{key: 按键名}` |
| `mousedown` | 鼠标按下 | `{x, y, button}` |
| `mouseup` | 鼠标释放 | `{x, y, button}` |
| `mousemove` | 鼠标移动 | `{x, y}` |
| `startup` | 脚本启动时 | 无 |
| `shutdown` | 脚本关闭时 | 无 |

---

## Task - 任务管理

任务管理模块提供异步任务创建和管理功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import task
```

### 函数列表

#### `create(func, interval)`

创建周期性任务。

**参数:**
- `func` (function): 任务函数
- `interval` (number): 执行间隔（毫秒）

**返回:** Task 对象

**示例:**
```lua
-- Lua
local myTask = task.create(function()
    print("Task running...")
    local match = screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
    if match then
        input.click(match.x, match.y, "left")
    end
end, 1000)  -- 每秒执行一次
```

```python
# Python
my_task = task.create(lambda: (
    print("Task running..."),
    screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
), 1000)  # 每秒执行一次
```

#### `start(task)`

启动任务。

**参数:**
- `task` (Task): 任务对象

**示例:**
```lua
-- Lua
task.start(myTask)
```

#### `stop(task)`

停止任务。

**参数:**
- `task` (Task): 任务对象

**示例:**
```lua
-- Lua
task.stop(myTask)
```

#### `delay(ms, callback)`

创建延迟任务。

**参数:**
- `ms` (number): 延迟时间（毫秒）
- `callback` (function): 回调函数

**返回:** Task 对象

**示例:**
```lua
-- Lua
task.delay(5000, function()
    print("5 seconds passed")
end)
```

```python
# Python
task.delay(5000, lambda: print("5 seconds passed"))
```

#### `async(func)`

创建异步任务。

**参数:**
- `func` (function): 异步函数

**返回:** Task 对象

**示例:**
```lua
-- Lua
local asyncTask = task.async(function()
    -- 执行耗时操作
    local result = someLongOperation()
    return result
end)

-- 获取结果
local result = asyncTask:get()
```

```python
# Python
async_task = task.async(lambda: some_long_operation())
result = async_task.get()
```

---

## Notify - 通知系统

通知系统提供桌面通知功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import notify
```

### 函数列表

#### `send(title, message, options)`

发送桌面通知。

**参数:**
- `title` (string): 通知标题
- `message` (string): 通知内容
- `options` (table/object): 通知选项（可选）

**选项:**
```lua
{
    icon = "path/to/icon.png",    -- 图标路径
    timeout = 5000,               -- 显示时长（毫秒）
    sound = true,                 -- 是否播放提示音
    onClick = function()          -- 点击回调
        print("Notification clicked")
    end
}
```

**示例:**
```lua
-- Lua
notify.send("任务完成", "自动化任务已成功完成", {
    icon = "success.png",
    timeout = 3000,
    sound = true,
    onClick = function()
        print("User clicked notification")
    end
})
```

```python
# Python
notify.send("任务完成", "自动化任务已成功完成", {
    "icon": "success.png",
    "timeout": 3000,
    "sound": True,
    "onClick": lambda: print("User clicked notification")
})
```

#### `update(id, title, message)`

更新现有通知。

**参数:**
- `id` (number): 通知 ID
- `title` (string): 新标题
- `message` (string): 新内容

**示例:**
```lua
-- Lua
local notification = notify.send("处理中...", "请稍候")
task.delay(2000, function()
    notify.update(notification, "处理完成", "任务已完成")
end)
```

#### `clear(id)`

清除通知。

**参数:**
- `id` (number): 通知 ID（可选，不提供则清除所有）

**示例:**
```lua
-- Lua
notify.clear()  -- 清除所有通知
notify.clear(notificationId)  -- 清除特定通知
```

---

## FSM - 状态机

有限状态机模块提供状态管理功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import fsm
```

### 函数列表

#### `create(config)`

创建状态机。

**参数:**
- `config` (table/object): 状态机配置

**配置:**
```lua
{
    initial = "idle",  -- 初始状态

    states = {
        idle = {
            enter = function()
                print("Enter idle state")
            end,
            exit = function()
                print("Exit idle state")
            end
        },

        running = {
            enter = function()
                print("Enter running state")
            end
        },

        paused = {}
    },

    transitions = {
        {from = "idle", to = "running", event = "start"},
        {from = "running", to = "paused", event = "pause"},
        {from = "paused", to = "running", event = "resume"},
        {from = "running", to = "idle", event = "stop"},
        {from = "paused", to = "idle", event = "stop"}
    }
}
```

**返回:** FSM 对象

**示例:**
```lua
-- Lua
local gameState = fsm.create({
    initial = "menu",

    states = {
        menu = {
            enter = function()
                ui.showMenu()
            end
        },
        playing = {
            enter = function()
                game.start()
            end,
            update = function(dt)
                game.update(dt)
            end
        },
        paused = {
            enter = function()
                ui.showPauseMenu()
            end
        },
        gameover = {
            enter = function()
                ui.showGameOver()
            end
        }
    },

    transitions = {
        {from = "menu", to = "playing", event = "start"},
        {from = "playing", to = "paused", event = "pause"},
        {from = "paused", to = "playing", event = "resume"},
        {from = "paused", to = "menu", event = "quit"},
        {from = "playing", to = "gameover", event = "die"},
        {from = "gameover", to = "menu", event = "restart"}
    }
})
```

```python
# Python
game_state = fsm.create({
    "initial": "menu",
    "states": {
        "menu": {
            "enter": lambda: ui.show_menu()
        },
        "playing": {
            "enter": lambda: game.start(),
            "update": lambda dt: game.update(dt)
        },
        "paused": {
            "enter": lambda: ui.show_pause_menu()
        }
    },
    "transitions": [
        {"from": "menu", "to": "playing", "event": "start"},
        {"from": "playing", "to": "paused", "event": "pause"},
        {"from": "paused", "to": "playing", "event": "resume"}
    ]
})
```

#### `transition(fsm, event)`

触发状态转换。

**参数:**
- `fsm` (FSM): 状态机对象
- `event` (string): 事件名称

**返回:** boolean - 转换是否成功

**示例:**
```lua
-- Lua
fsm.transition(gameState, "start")  -- menu -> playing
fsm.transition(gameState, "pause")  -- playing -> paused
```

```python
# Python
fsm.transition(game_state, "start")  # menu -> playing
fsm.transition(game_state, "pause")  # playing -> paused
```

#### `getState(fsm)`

获取当前状态。

**参数:**
- `fsm` (FSM): 状态机对象

**返回:** string - 当前状态名称

**示例:**
```lua
-- Lua
print("Current state:", fsm.getState(gameState))
```

```python
# Python
print(f"Current state: {fsm.get_state(game_state)}")
```

#### `canTransition(fsm, event)`

检查是否可以转换。

**参数:**
- `fsm` (FSM): 状态机对象
- `event` (string): 事件名称

**返回:** boolean

**示例:**
```lua
-- Lua
if fsm.canTransition(gameState, "pause") then
    fsm.transition(gameState, "pause")
end
```

```python
# Python
if fsm.can_transition(game_state, "pause"):
    fsm.transition(game_state, "pause")
```

---

## HTTP - 网络请求

HTTP 模块提供网络请求功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import http
```

### 函数列表

#### `get(url, options)`

发送 GET 请求。

**参数:**
- `url` (string): 请求 URL
- `options` (table/object): 请求选项（可选）

**选项:**
```lua
{
    headers = {              -- 请求头
        Authorization = "Bearer token"
    },
    params = {               -- 查询参数
        page = 1,
        limit = 10
    },
    timeout = 5000           -- 超时时间（毫秒）
}
```

**返回:** Response 对象

**示例:**
```lua
-- Lua
local response = http.get("https://api.example.com/data", {
    headers = {
        Authorization = "Bearer mytoken"
    },
    params = {
        page = 1
    }
})

if response.ok then
    local data = json.decode(response.text)
    print("Received data:", data)
else
    print("Error:", response.status)
end
```

```python
# Python
response = http.get("https://api.example.com/data", {
    "headers": {
        "Authorization": "Bearer mytoken"
    },
    "params": {
        "page": 1
    }
})

if response.ok:
    data = json.decode(response.text)
    print(f"Received data: {data}")
else:
    print(f"Error: {response.status}")
```

#### `post(url, data, options)`

发送 POST 请求。

**参数:**
- `url` (string): 请求 URL
- `data` (table/object/string): 请求数据
- `options` (table/object): 请求选项（可选）

**示例:**
```lua
-- Lua
local response = http.post("https://api.example.com/submit", {
    name = "Player",
    score = 1000
}, {
    headers = {
        ["Content-Type"] = "application/json"
    }
})
```

```python
# Python
response = http.post("https://api.example.com/submit", {
    "name": "Player",
    "score": 1000
}, {
    "headers": {
        "Content-Type": "application/json"
    }
})
```

#### `put(url, data, options)`

发送 PUT 请求。

**参数:**
- `url` (string): 请求 URL
- `data` (table/object/string): 请求数据
- `options` (table/object): 请求选项（可选）

**示例:**
```lua
-- Lua
local response = http.put("https://api.example.com/update/1", {
    status = "completed"
})
```

#### `delete(url, options)`

发送 DELETE 请求。

**参数:**
- `url` (string): 请求 URL
- `options` (table/object): 请求选项（可选）

**示例:**
```lua
-- Lua
local response = http.delete("https://api.example.com/items/1")
```

#### `request(method, url, options)`

发送自定义请求。

**参数:**
- `method` (string): HTTP 方法
- `url` (string): 请求 URL
- `options` (table/object): 请求选项

**示例:**
```lua
-- Lua
local response = http.request("PATCH", "https://api.example.com/items/1", {
    data = {status = "in_progress"},
    headers = {["Content-Type"] = "application/json"}
})
```

### Response 对象

| 属性 | 类型 | 说明 |
|------|------|------|
| `ok` | boolean | 请求是否成功 |
| `status` | number | HTTP 状态码 |
| `text` | string | 响应文本 |
| `headers` | table | 响应头 |

---

## Config - 配置

配置模块提供配置管理功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import config
```

### 函数列表

#### `load(path)`

加载配置文件。

**参数:**
- `path` (string): 配置文件路径（支持 .json, .ini）

**返回:** 配置对象

**示例:**
```lua
-- Lua
local conf = config.load("config.json")
print(conf.server.host)
print(conf.server.port)
```

```python
# Python
conf = config.load("config.json")
print(conf["server"]["host"])
print(conf["server"]["port"])
```

#### `save(config, path)`

保存配置到文件。

**参数:**
- `config` (table/object): 配置对象
- `path` (string): 保存路径

**示例:**
```lua
-- Lua
local conf = {
    server = {
        host = "localhost",
        port = 8080
    },
    debug = true
}
config.save(conf, "config.json")
```

```python
# Python
conf = {
    "server": {
        "host": "localhost",
        "port": 8080
    },
    "debug": True
}
config.save(conf, "config.json")
```

#### `get(key, default)`

获取配置值。

**参数:**
- `key` (string): 配置键（支持点号分隔的路径）
- `default` (any): 默认值（可选）

**返回:** 配置值

**示例:**
```lua
-- Lua
local port = config.get("server.port", 8080)
local debug = config.get("debug", false)
```

```python
# Python
port = config.get("server.port", 8080)
debug = config.get("debug", False)
```

#### `set(key, value)`

设置配置值。

**参数:**
- `key` (string): 配置键
- `value` (any): 配置值

**示例:**
```lua
-- Lua
config.set("server.port", 9090)
config.set("debug", true)
```

```python
# Python
config.set("server.port", 9090)
config.set("debug", True)
```

#### `merge(config1, config2)`

合并两个配置。

**参数:**
- `config1` (table/object): 基础配置
- `config2` (table/object): 覆盖配置

**返回:** 合并后的配置

**示例:**
```lua
-- Lua
local default = {theme = "dark", language = "en"}
local user = {language = "zh"}
local merged = config.merge(default, user)
-- merged = {theme = "dark", language = "zh"}
```

```python
# Python
default = {"theme": "dark", "language": "en"}
user = {"language": "zh"}
merged = config.merge(default, user)
# merged = {"theme": "dark", "language": "zh"}
```

---

## 🔗 相关文档

- [核心 API](core.md)
- [数据持久化 API](data.md)
- [序列化 API](serialize.md)
- [调试 API](debugger.md)
- [快速开始](../getting-started.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
