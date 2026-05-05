# 配置系统

Wingman 使用 JSON 格式的配置文件来存储应用程序设置。配置文件在首次运行时自动创建。

## 配置文件位置

配置文件默认保存在 `config/config.json`（相对于可执行文件目录）。

## 配置结构

```json
{
  "server": {
    "host": "localhost",
    "port": 8080,
    "username": "",
    "password": "",
    "autoConnect": false,
    "serverControlled": false
  },
  "tray": {
    "minimizeToTray": true,
    "startMinimized": false,
    "showNotifications": true
  },
  "autoRun": {
    "enabled": false,
    "scriptPath": "",
    "delaySeconds": 0,
    "repeat": false,
    "repeatInterval": 0
  }
}
```

## 配置项说明

### Server (服务器配置)

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `host` | string | `"localhost"` | 服务器地址 |
| `port` | number | `8080` | 服务器端口 (1-65535) |
| `username` | string | `""` | 登录用户名（可选） |
| `password` | string | `""` | 登录密码（可选，明文存储） |
| `autoConnect` | boolean | `false` | 启动时自动连接服务器 |
| `serverControlled` | boolean | `false` | **服务器控制模式**：允许远程下发脚本控制客户端 |

### Tray (托盘配置)

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `minimizeToTray` | boolean | `true` | 最小化到托盘而非任务栏 |
| `startMinimized` | boolean | `false` | 启动时最小化到托盘 |
| `showNotifications` | boolean | `true` | 显示托盘通知 |
| `iconPath` | string | `""` | 默认托盘图标路径（.ico 文件） |
| `tooltip` | string | `"Wingman"` | 托盘图标提示文本 |
| `iconNormal` | string | `""` | 正常状态图标（正在运行） |
| `iconIdle` | string | `""` | 空闲状态图标（变灰，无任务） |
| `iconBusy` | string | `""` | 忙碌状态图标 |
| `iconError` | string | `""` | 错误状态图标 |
| `menuItems` | array | 见下方 | 托盘菜单项配置 |

#### 图标状态

托盘图标支持多种状态，可根据程序运行情况动态切换：

| 状态值 | 名称 | 说明 |
|-------|------|------|
| `0` | normal | 正常状态（正在运行任务） |
| `1` | idle | 空闲状态（无任务，显示灰色图标） |
| `2` | disabled | 禁用状态 |
| `3` | busy | 忙碌状态（处理中） |
| `4` | error | 错误状态 |

#### 菜单项配置 (menuItems)

每个菜单项是一个对象，包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 菜单项唯一标识 |
| `label` | string | 显示文本 |
| `actionType` | number | 动作类型：0=无, 1=命令, 2=Lua, 3=启动游戏, 4=HTTP请求, 5=回调 |
| `action` | string | 动作参数（命令路径/Lua脚本/游戏名/URL等） |
| `enabled` | boolean | 是否启用 |
| `isSeparator` | boolean | 是否为分隔符 |
| `subitems` | array | 子菜单项（仅用于子菜单类型） |

**动作类型说明：**

- `0` (none): 无动作，仅用于显示
- `1` (command): 执行系统命令，`action` 为命令字符串
- `2` (lua): 执行 Lua 脚本，`action` 为脚本代码
- `3` (startGame): 启动游戏，`action` 为游戏名称
- `4` (http): 发送 HTTP 请求，`action` 为 URL
- `5` (callback): 内部回调，用于预设菜单项（如退出、查看配置等）

**配置示例：**

```json
{
  "tray": {
    "minimizeToTray": true,
    "iconPath": "assets/myicon.ico",
    "iconNormal": "assets/icon_normal.ico",
    "iconIdle": "assets/icon_gray.ico",
    "iconBusy": "assets/icon_busy.ico",
    "tooltip": "我的游戏助手",
    "menuItems": [
      {
        "id": "game1",
        "label": "启动游戏1",
        "actionType": 3,
        "action": "游戏1",
        "enabled": true
      },
      {
        "id": "sep1",
        "isSeparator": true
      },
      {
        "id": "exit",
        "label": "退出",
        "actionType": 5,
        "enabled": true
      }
    ]
  }
}
```

### AutoRun (自动运行配置)

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enabled` | boolean | `false` | 是否启用自动运行脚本 |
| `scriptPath` | string | `""` | 脚本文件路径（相对于可执行文件） |
| `delaySeconds` | number | `0` | 启动延迟（秒） |
| `repeat` | boolean | `false` | 是否重复运行 |
| `repeatInterval` | number | `0` | 重复间隔（秒），0 表示脚本自己控制循环 |

### Heartbeat (心跳配置)

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enabled` | boolean | `true` | 是否启用心跳 |
| `intervalSeconds` | number | `30` | 心跳间隔（秒） |
| `timeoutSeconds` | number | `90` | 超时时间（秒），服务器超过此时间未收到心跳认为节点离线 |

### Game (游戏配置)

游戏配置用于管理需要自动启动和控制的游戏。可以配置多个游戏，每个游戏可以关联自动运行脚本。

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `name` | string | - | 游戏名称（唯一标识） |
| `path` | string | - | 游戏可执行文件路径（绝对路径） |
| `args` | string | `""` | 启动参数 |
| `workingDir` | string | - | 工作目录（可选，默认为游戏所在目录） |
| `autoStart` | boolean | `false` | 是否自动启动游戏 |
| `scriptPath` | string | `""` | 关联的自动脚本路径 |
| `windowTitle` | string | `""` | 窗口标题（用于检测游戏窗口） |
| `delaySeconds` | number | `5` | 游戏启动后等待时间（秒） |
| `autoRestart` | boolean | `false` | 游戏关闭后是否自动重启 |
| `restartDelay` | number | `10` | 重启延迟（秒） |
| `maxRestarts` | number | `3` | 最大重启次数（0 = 无限） |

## C++ API

```cpp
#include "wingman/config.hpp"

// 创建配置管理器
ConfigManager config("config");

// 获取服务器配置
ServerConfig server = config.getServerConfig();
std::cout << "服务器: " << server.host << ":" << server.port << std::endl;

// 设置服务器配置
server.host = "192.168.1.100";
server.port = 9000;
config.setServerConfig(server);

// 托盘配置
TrayConfig tray = config.getTrayConfig();
tray.startMinimized = true;
config.setTrayConfig(tray);

// 通用键值对访问
config.set("custom_key", "custom_value");
auto value = config.get("custom_key");
```

## Lua API

```lua
-- 获取服务器配置
local server = config.getServer()
print("服务器: " .. server.host .. ":" .. server.port)
print("服务器控制模式: " .. tostring(server.serverControlled))

-- 设置服务器配置
server.host = "192.168.1.100"
server.port = 9000
server.autoConnect = true
server.serverControlled = true  -- 启用服务器控制模式
config.setServer(server)

-- 托盘配置
local tray = config.getTray()
tray.startMinimized = true
tray.iconPath = "assets/myicon.ico"
tray.tooltip = "我的游戏助手"

-- 配置托盘菜单项
tray.menuItems = {
    {
        id = "help",
        label = "帮助",
        actionType = 0,  -- 无动作
        enabled = true,
        isSeparator = false
    },
    {
        id = "sep1",
        isSeparator = true
    },
    {
        id = "start_game",
        label = "启动游戏",
        actionType = 3,  -- startGame
        action = "我的游戏",
        enabled = true
    },
    {
        id = "custom_script",
        label = "运行自定义脚本",
        actionType = 1,  -- command
        action = "notepad.exe",
        enabled = true
    },
    {
        id = "sep2",
        isSeparator = true
    },
    {
        id = "exit",
        label = "退出",
        actionType = 5,  -- callback
        enabled = true
    }
}
config.setTray(tray)

-- 托盘图标状态切换
local icon = tray.get("main")  -- 获取托盘图标

-- 状态值：0=normal, 1=idle(灰), 2=disabled, 3=busy, 4=error
icon:setIconState(0)  -- 正常运行状态
icon:setIconState(1)  -- 空闲状态（图标变灰）

-- 获取当前状态
local state = icon:getIconState()
print("当前图标状态: " .. state)

-- 自动运行配置
local autoRun = config.getAutoRun()
autoRun.enabled = true
autoRun.scriptPath = "scripts/auto_task.lua"
autoRun.delaySeconds = 5
autoRun.repeat = true
autoRun.repeatInterval = 60
config.setAutoRun(autoRun)

-- 心跳配置
local heartbeat = config.getHeartbeat()
heartbeat.enabled = true
heartbeat.intervalSeconds = 30  -- 每 30 秒发送一次心跳
config.setHeartbeat(heartbeat)

-- 游戏配置
local games = config.getGames()
for i, game in ipairs(games) do
    print("游戏: " .. game.name)
    print("路径: " .. game.path)
end

-- 添加游戏配置
local newGame = {
    name = "我的游戏",
    path = "C:\\Games\\MyGame\\game.exe",
    args = "-windowed -nosplash",
    workingDir = "C:\\Games\\MyGame",
    autoStart = true,
    scriptPath = "scripts/my_game_script.lua",
    windowTitle = "MyGame",
    delaySeconds = 5,
    autoRestart = true,
    restartDelay = 10,
    maxRestarts = 3
}
config.addGame(newGame)

-- 启动游戏
game.startGame("我的游戏")

-- 通用键值对
config.set("myKey", "myValue")
local value = config.get("myKey")
```

## 节点模块 (node)

节点模块提供了节点状态管理和心跳功能。

### node.createHeartbeat()

创建心跳数据。

```lua
local heartbeat = node.createHeartbeat()
-- {
--   json = "{...}",      -- 完整的 JSON 数据
--   nodeId = "node-abc123",
--   version = "0.1.0"
-- }
```

### node.sendHeartbeat(table)

发送心跳到服务器（需要启用服务器控制模式）。

```lua
local heartbeat = node.createHeartbeat()
node.sendHeartbeat(heartbeat)
```

### node.getWindows()

获取所有窗口列表（用于汇报游戏窗口状态）。

```lua
local windows = node.getWindows()
for i, win in ipairs(windows) do
    print(win.title)
    print(win.handle)
    print(win.isForeground)
    -- bounds: {x, y, width, height}
end
```

## 运行模式

### 1. 独立模式（默认）

客户端完全自主控制，不受服务器影响。

```json
{
  "server": {
    "serverControlled": false
  }
}
```

### 2. 服务器控制模式

启用后，客户端会：
- 接受服务器下发的脚本并执行
- 向服务器报告执行状态
- 接受服务器的启停控制

```json
{
  "server": {
    "serverControlled": true,
    "autoConnect": true
  }
}
```

⚠️ **安全警告**：服务器控制模式下，客户端会执行服务器下发的任意 Lua 代码。请确保：
- 只连接可信任的服务器
- 使用强密码或 token 认证
- 考虑使用 TLS 加密通信

## 环境变量

部分配置可以通过环境变量覆盖：

| 环境变量 | 配置项 |
|----------|--------|
| `WINGMAN_SERVER_HOST` | server.host |
| `WINGMAN_SERVER_PORT` | server.port |
| `WINGMAN_SERVER_USERNAME` | server.username |
| `WINGMAN_SERVER_PASSWORD` | server.password |

## 安全提示

⚠️ **密码明文存储**：当前配置文件中的密码以明文形式存储。在生产环境中，建议：
- 使用环境变量存储敏感信息
- 实现密码加密存储功能
- 使用 token 认证代替用户名密码
