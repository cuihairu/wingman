# API: wingman.config

配置管理模块，用于读写应用程序配置。

## 获取服务器配置

<CodeTabs>

:::slot python

```python
from wingman import config

server = config.get_server()
# {
#   "host": "localhost",
#   "port": 8080,
#   "username": "",
#   "password": "",
#   "autoConnect": False,
#   "serverControlled": False
# }
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local server = config.getServer()
-- {
--   host = "localhost",
--   port = 8080,
--   username = "",
--   password = "",
--   autoConnect = false,
--   serverControlled = false
-- }
```

:::

</CodeTabs>

## 设置服务器配置

<CodeTabs>

:::slot python

```python
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

:::

:::slot lua

```lua
local config = require("wingman.config")

config.setServer({
    host = "192.168.1.100",
    port = 9000,
    username = "admin",
    password = "secret",
    autoConnect = true,
    serverControlled = false
})
```

:::

</CodeTabs>

## 获取托盘配置

<CodeTabs>

:::slot python

```python
from wingman import config

tray = config.get_tray()
# {
#   "minimizeToTray": True,
#   "startMinimized": False,
#   "showNotifications": True
# }
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local tray = config.getTray()
-- {
--   minimizeToTray = true,
--   startMinimized = false,
--   showNotifications = true
-- }
```

:::

</CodeTabs>

## 设置托盘配置

<CodeTabs>

:::slot python

```python
from wingman import config

config.set_tray({
    "minimizeToTray": True,
    "startMinimized": True,
    "showNotifications": False
})
```

:::

:::slot lua

```lua
local config = require("wingman.config")

config.setTray({
    minimizeToTray = true,
    startMinimized = true,
    showNotifications = false
})
```

:::

</CodeTabs>

## 获取自动运行配置

<CodeTabs>

:::slot python

```python
from wingman import config

auto_run = config.get_auto_run()
# {
#   "enabled": False,
#   "scriptPath": "",
#   "delaySeconds": 0,
#   "repeat": False,
#   "repeatInterval": 0
# }
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local autoRun = config.getAutoRun()
-- {
--   enabled = false,
--   scriptPath = "",
--   delaySeconds = 0,
--   repeat = false,
--   repeatInterval = 0
-- }
```

:::

</CodeTabs>

## 设置自动运行配置

<CodeTabs>

:::slot python

```python
from wingman import config

config.set_auto_run({
    "enabled": True,
    "scriptPath": "scripts/auto_task.lua",
    "delaySeconds": 5,
    "repeat": True,
    "repeatInterval": 60
})
```

:::

:::slot lua

```lua
local config = require("wingman.config")

config.setAutoRun({
    enabled = true,
    scriptPath = "scripts/auto_task.lua",
    delaySeconds = 5,
    repeat = true,
    repeatInterval = 60
})
```

:::

</CodeTabs>

## 获取自定义配置值

<CodeTabs>

:::slot python

```python
from wingman import config

value = config.get("myKey")
if value:
    print(value)
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local value = config.get("myKey")
if value then
    print(value)
end
```

:::

</CodeTabs>

## 设置自定义配置值

<CodeTabs>

:::slot python

```python
from wingman import config

config.set("myKey", "myValue")
config.set("count", 42)
config.set("enabled", True)
```

:::

:::slot lua

```lua
local config = require("wingman.config")

config.set("myKey", "myValue")
config.set("count", 42)
config.set("enabled", true)
```

:::

</CodeTabs>

## 删除自定义配置值

<CodeTabs>

:::slot python

```python
from wingman import config

removed = config.remove("myKey")
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local removed = config.remove("myKey")
```

:::

</CodeTabs>

---

## 完整示例

### 读取并显示当前配置

<CodeTabs>

:::slot python

```python
from wingman import config

server = config.get_server()
print(f"服务器: {server['host']}:{server['port']}")
print(f"自动连接: {server['autoConnect']}")
print(f"服务器控制: {server['serverControlled']}")
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local server = config.getServer()
print("服务器: " .. server.host .. ":" .. server.port)
print("自动连接: " .. tostring(server.autoConnect))
print("服务器控制: " .. tostring(server.serverControlled))
```

:::

</CodeTabs>

### 启用服务器控制模式

<CodeTabs>

:::slot python

```python
from wingman import config

server = config.get_server()
server['host'] = "your-server.com"
server['port'] = 9000
server['username'] = "client"
server['password'] = "your-password"
server['autoConnect'] = True
server['serverControlled'] = True
config.set_server(server)

print("已启用服务器控制模式")
```

:::

:::slot lua

```lua
local config = require("wingman.config")

local server = config.getServer()
server.host = "your-server.com"
server.port = 9000
server.username = "client"
server.password = "your-password"
server.autoConnect = true
server.serverControlled = true
config.setServer(server)

print("已启用服务器控制模式")
```

:::

</CodeTabs>

### 自定义配置存储

<CodeTabs>

:::slot python

```python
from wingman import config

# 存储游戏配置
config.set("game.character", "战士")
config.set("game.server", "电信一区")
config.set("game.autoPotion", "true")

# 读取游戏配置
character = config.get("game.character")
server = config.get("game.server")
auto_potion = config.get("game.autoPotion")

print(f"角色: {character}")
print(f"服务器: {server}")
print(f"自动喝药: {auto_potion}")
```

:::

:::slot lua

```lua
local config = require("wingman.config")

-- 存储游戏配置
config.set("game.character", "战士")
config.set("game.server", "电信一区")
config.set("game.autoPotion", "true")

-- 读取游戏配置
local character = config.get("game.character")
local server = config.get("game.server")
local autoPotion = config.get("game.autoPotion")

print("角色: " .. character)
print("服务器: " .. server)
print("自动喝药: " .. tostring(autoPotion == "true"))
```

:::

</CodeTabs>

---

## 可用接口

### `get_server()` / `getServer()`

获取服务器配置。

### `set_server(table)` / `setServer(table)`

设置服务器配置。

### `get_tray()` / `getTray()`

获取托盘配置。

### `set_tray(table)` / `setTray(table)`

设置托盘配置。

### `get_auto_run()` / `getAutoRun()`

获取自动运行配置。

### `set_auto_run(table)` / `setAutoRun(table)`

设置自动运行配置。

### `get(key)`

获取自定义配置值。

### `set(key, value)`

设置自定义配置值。

### `remove(key)`

删除自定义配置值。
