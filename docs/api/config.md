# API: wingman.config

配置管理模块，用于读写应用程序配置。

## 模块概述

config 模块提供配置的读写功能，支持：
- **服务器配置** - 远程连接相关设置
- **托盘配置** - 系统托盘行为设置
- **自动运行配置** - 启动时自动执行脚本
- **自定义配置** - 存储用户自定义键值对

---

## 获取服务器配置

### get_server() / getServer()

**说明**：获取服务器配置信息。

**函数签名**：

```python
get_server() -> dict
```

```lua
getServer() -> table
```

**返回**：
- Python: 包含服务器配置的字典
- Lua: 包含服务器配置的表格

**返回结构**：
```python
{
    "host": "localhost",           # 服务器地址
    "port": 8080,                  # 服务器端口
    "username": "",                # 用户名
    "password": "",                # 密码
    "autoConnect": False,         # 是否自动连接
    "serverControlled": False     # 是否启用服务器控制
}
```

:::tabs

== Python

```python:line-numbers
from wingman import config

server = config.get_server()
print(f"服务器: {server['host']}:{server['port']}")
print(f"自动连接: {server['autoConnect']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local server = wingman.config.getServer()
print("服务器: " .. server.host .. ":" .. server.port)
print("自动连接: " .. tostring(server.autoConnect))
```

:::

---

## 设置服务器配置

### set_server(config) / setServer(config)

**说明**：设置服务器配置信息。

**函数签名**：

```python
set_server(config: dict) -> None
```

```lua
setServer(config: table) -> nil
```

**参数**：
- `config` - 包含服务器配置的字典/表格，结构与 `get_server()` 返回值相同

:::tabs

== Python

```python:line-numbers
from wingman import config

config.set_server({
    "host": "192.168.1.100",
    "port": 9000,
    "username": "admin",
    "password": "secret",
    "autoConnect": True,
    "serverControlled": False
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.config.setServer({
    host = "192.168.1.100",
    port = 9000,
    username = "admin",
    password = "secret",
    autoConnect = true,
    serverControlled = false
})
```

:::

---

## 获取托盘配置

### get_tray() / getTray()

**说明**：获取系统托盘配置信息。

**函数签名**：

```python
get_tray() -> dict
```

```lua
getTray() -> table
```

**返回**：
- Python: 包含托盘配置的字典
- Lua: 包含托盘配置的表格

**返回结构**：
```python
{
    "minimizeToTray": True,         # 最小化到托盘
    "startMinimized": False,        # 启动时最小化
    "showNotifications": True       # 显示通知
}
```

:::tabs

== Python

```python:line-numbers
from wingman import config

tray = config.get_tray()
print(f"最小化到托盘: {tray['minimizeToTray']}")
print(f"显示通知: {tray['showNotifications']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tray = wingman.config.getTray()
print("最小化到托盘: " .. tostring(tray.minimizeToTray))
print("显示通知: " .. tostring(tray.showNotifications))
```

:::

---

## 设置托盘配置

### set_tray(config) / setTray(config)

**说明**：设置系统托盘配置信息。

**函数签名**：

```python
set_tray(config: dict) -> None
```

```lua
setTray(config: table) -> nil
```

**参数**：
- `config` - 包含托盘配置的字典/表格，结构与 `get_tray()` 返回值相同

:::tabs

== Python

```python:line-numbers
from wingman import config

config.set_tray({
    "minimizeToTray": True,
    "startMinimized": True,
    "showNotifications": False
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.config.setTray({
    minimizeToTray = true,
    startMinimized = true,
    showNotifications = false
})
```

:::

---

## 获取自动运行配置

### get_auto_run() / getAutoRun()

**说明**：获取自动运行配置信息。

**函数签名**：

```python
get_auto_run() -> dict
```

```lua
getAutoRun() -> table
```

**返回**：
- Python: 包含自动运行配置的字典
- Lua: 包含自动运行配置的表格

**返回结构**：
```python
{
    "enabled": False,               # 是否启用
    "scriptPath": "",               # 脚本路径
    "delaySeconds": 0,              # 启动延迟（秒）
    "repeat": False,                # 是否重复执行
    "repeatInterval": 0             # 重复间隔（秒）
}
```

:::tabs

== Python

```python:line-numbers
from wingman import config

auto_run = config.get_auto_run()
if auto_run['enabled']:
    print(f"自动运行脚本: {auto_run['scriptPath']}")
else:
    print("自动运行未启用")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local autoRun = wingman.config.getAutoRun()
if autoRun.enabled then
    print("自动运行脚本: " .. autoRun.scriptPath)
else
    print("自动运行未启用")
end
```

:::

---

## 设置自动运行配置

### set_auto_run(config) / setAutoRun(config)

**说明**：设置自动运行配置信息。

**函数签名**：

```python
set_auto_run(config: dict) -> None
```

```lua
setAutoRun(config: table) -> nil
```

**参数**：
- `config` - 包含自动运行配置的字典/表格，结构与 `get_auto_run()` 返回值相同

:::tabs

== Python

```python:line-numbers
from wingman import config

config.set_auto_run({
    "enabled": True,
    "scriptPath": "scripts/auto_task.lua",
    "delaySeconds": 5,
    "repeat": True,
    "repeatInterval": 60
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.config.setAutoRun({
    enabled = true,
    scriptPath = "scripts/auto_task.lua",
    delaySeconds = 5,
    repeat = true,
    repeatInterval = 60
})
```

:::

---

## 获取自定义配置

### get(key) / get(key)

**说明**：获取自定义配置值。

**函数签名**：

```python
get(key: str) -> Any
```

```lua
get(key: string) -> any
```

**参数**：
- `key` - 配置键名，支持点号分隔的嵌套键（如 `"game.character"`）

**返回**：
- 配置值，不存在时返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import config

value = config.get("myKey")
if value:
    print(value)

# 支持嵌套键
character = config.get("game.character")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local value = wingman.config.get("myKey")
if value then
    print(value)
end

-- 支持嵌套键
local character = wingman.config.get("game.character")
```

:::

---

## 设置自定义配置

### set(key, value) / set(key, value)

**说明**：设置自定义配置值。

**函数签名**：

```python
set(key: str, value: Any) -> None
```

```lua
set(key: string, value: any) -> nil
```

**参数**：
- `key` - 配置键名，支持点号分隔的嵌套键
- `value` - 配置值（支持 JSON 兼容类型）

:::tabs

== Python

```python:line-numbers
from wingman import config

# 简单键值
config.set("myKey", "myValue")
config.set("count", 42)
config.set("enabled", True)

# 嵌套键
config.set("game.character", "战士")
config.set("game.server", "电信一区")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 简单键值
wingman.config.set("myKey", "myValue")
wingman.config.set("count", 42)
wingman.config.set("enabled", true)

-- 嵌套键
wingman.config.set("game.character", "战士")
wingman.config.set("game.server", "电信一区")
```

:::

---

## 删除自定义配置

### remove(key) / remove(key)

**说明**：删除自定义配置值。

**函数签名**：

```python
remove(key: str) -> bool
```

```lua
remove(key: string) -> boolean
```

**参数**：
- `key` - 配置键名

**返回**：
- 是否成功删除（键存在返回 `True`/`true`，否则返回 `False`/`false`）

:::tabs

== Python

```python:line-numbers
from wingman import config

removed = config.remove("myKey")
if removed:
    print("配置已删除")
else:
    print("配置不存在")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local removed = wingman.config.remove("myKey")
if removed then
    print("配置已删除")
else
    print("配置不存在")
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get_server()` | `getServer()` | 获取服务器配置 | 返回: {host, port, username, password, autoConnect, serverControlled} |
| `set_server(config)` | `setServer(config)` | 设置服务器配置 | config: 配置字典/表格 |
| `get_tray()` | `getTray()` | 获取托盘配置 | 返回: {minimizeToTray, startMinimized, showNotifications} |
| `set_tray(config)` | `setTray(config)` | 设置托盘配置 | config: 配置字典/表格 |
| `get_auto_run()` | `getAutoRun()` | 获取自动运行配置 | 返回: {enabled, scriptPath, delaySeconds, repeat, repeatInterval} |
| `set_auto_run(config)` | `setAutoRun(config)` | 设置自动运行配置 | config: 配置字典/表格 |
| `get(key)` | `get(key)` | 获取自定义配置 | key: 键名（支持嵌套） |
| `set(key, value)` | `set(key, value)` | 设置自定义配置 | key: 键名, value: 值 |
| `remove(key)` | `remove(key)` | 删除自定义配置 | key: 键名 |
