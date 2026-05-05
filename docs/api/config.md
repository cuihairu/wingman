# config 模块

配置管理模块，用于读写应用程序配置。

## 函数

### config.getServer()

获取服务器配置。

**返回值：** table

```lua
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

### config.setServer(table)

设置服务器配置。

**参数：**
- `table` - 服务器配置表

```lua
config.setServer({
    host = "192.168.1.100",
    port = 9000,
    username = "admin",
    password = "secret",
    autoConnect = true,
    serverControlled = false
})
```

### config.getTray()

获取托盘配置。

**返回值：** table

```lua
local tray = config.getTray()
-- {
--   minimizeToTray = true,
--   startMinimized = false,
--   showNotifications = true
-- }
```

### config.setTray(table)

设置托盘配置。

**参数：**
- `table` - 托盘配置表

```lua
config.setTray({
    minimizeToTray = true,
    startMinimized = true,
    showNotifications = false
})
```

### config.getAutoRun()

获取自动运行配置。

**返回值：** table

```lua
local autoRun = config.getAutoRun()
-- {
--   enabled = false,
--   scriptPath = "",
--   delaySeconds = 0,
--   repeat = false,
--   repeatInterval = 0
-- }
```

### config.setAutoRun(table)

设置自动运行配置。

**参数：**
- `table` - 自动运行配置表

```lua
config.setAutoRun({
    enabled = true,
    scriptPath = "scripts/auto_task.lua",
    delaySeconds = 5,
    repeat = true,
    repeatInterval = 60
})
```

### config.get(key)

获取自定义配置值。

**参数：**
- `key` (string) - 配置键名

**返回值：** string | nil

```lua
local value = config.get("myKey")
if value then
    print(value)
end
```

### config.set(key, value)

设置自定义配置值。

**参数：**
- `key` (string) - 配置键名
- `value` (string|number|boolean) - 配置值

```lua
config.set("myKey", "myValue")
config.set("count", 42)
config.set("enabled", true)
```

### config.remove(key)

删除自定义配置值。

**参数：**
- `key` (string) - 配置键名

**返回值：** boolean - 是否成功删除

```lua
local removed = config.remove("myKey")
```

## 示例

### 读取并显示当前配置

```lua
local server = config.getServer()
print("服务器: " .. server.host .. ":" .. server.port)
print("自动连接: " .. tostring(server.autoConnect))
print("服务器控制: " .. tostring(server.serverControlled))

local autoRun = config.getAutoRun()
print("自动运行: " .. tostring(autoRun.enabled))
if autoRun.enabled then
    print("脚本: " .. autoRun.scriptPath)
    print("延迟: " .. autoRun.delaySeconds .. "秒")
end
```

### 启用服务器控制模式

```lua
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

### 配置自动运行脚本

```lua
-- 设置启动 5 秒后自动运行脚本，每 60 秒重复一次
config.setAutoRun({
    enabled = true,
    scriptPath = "scripts/auto_farm.lua",
    delaySeconds = 5,
    repeat = true,
    repeatInterval = 60
})

print("已配置自动运行脚本")
```

### 自定义配置存储

```lua
-- 存储游戏配置
config.set("game.character", "战士")
config.set("game.server", "电信一区")
config.set("game.autoPotion", true)

-- 读取游戏配置
local character = config.get("game.character")
local server = config.get("game.server")
local autoPotion = config.get("game.autoPotion")

print("角色: " .. character)
print("服务器: " .. server)
print("自动喝药: " .. tostring(autoPotion == "true"))
```
