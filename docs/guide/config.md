# 配置系统

Wingman 使用 JSON 格式的配置文件来存储应用程序设置。配置文件在首次运行时自动创建。

## 配置文件位置

配置文件默认保存在 `config/config.json`（相对于可执行文件目录）。

## 配置结构

```json
{
  "server": {
    "host": "localhost",
    "port": 9527,
    "username": "",
    "password": "",
    "autoConnect": false,
    "serverControlled": false
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
| `port` | number | `9527` | 服务器端口 (1-65535) |
| `username` | string | `""` | 登录用户名（可选） |
| `password` | string | `""` | 登录密码（可选，明文存储） |
| `autoConnect` | boolean | `false` | 启动时自动连接服务器 |
| `serverControlled` | boolean | `false` | **服务器控制模式**：允许远程下发脚本控制客户端 |

### Tray (托盘配置)——未实现（计划中）

配置结构中**没有** `tray` 节。`TrayConfig` 及其相关字段（`minimizeToTray` / `iconPath` / `menuItems` / 图标状态等）尚未实现，属规划项，此处不提供配置说明。

脚本侧如需控制托盘展示，走 `notify` 模块的托盘函数（GUI 在线时生效）：

```lua
local wingman = require("wingman")

wingman.notify.trayShow()              -- 显示托盘
wingman.notify.trayHide()              -- 隐藏托盘
wingman.notify.traySetBadge("3")       -- 设置角标（nil 清除）
wingman.notify.traySetTooltip("运行中") -- 设置提示文本（无参清除）
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

// 自动运行配置
AutoRunConfig autoRun = config.getAutoRunConfig();
autoRun.enabled = true;
config.setAutoRunConfig(autoRun);

// 心跳配置
HeartbeatConfig heartbeat = config.getHeartbeatConfig();
heartbeat.intervalSeconds = 30;
config.setHeartbeatConfig(heartbeat);

// 游戏配置
auto games = config.getGameConfigList();
config.addGameConfig(game);              // GameConfig 结构见上表
config.removeGameConfig("我的游戏");

// 通用键值对访问
config.set("custom_key", "custom_value");
auto value = config.get("custom_key");
```

## Lua API

Lua `config` 模块只暴露 5 个函数：`get` / `set` / `remove` / `save` / `load`（通用键值对，值以字符串存取）。server / autoRun / heartbeat / games 等结构化配置仅 C++ `ConfigManager` 提供访问器（见上节），脚本侧没有对应的结构化 API。

```lua
local wingman = require("wingman")

-- 通用键值对（值以字符串存取）
wingman.config.set("myKey", "myValue")
local value = wingman.config.get("myKey")
print(value)

-- 删除键
wingman.config.remove("myKey")

-- 落盘 / 重载
wingman.config.save()
wingman.config.load()
```

> 注意：`repeat` 是 Lua 保留字，不能写作 `autoRun.repeat` 这样的字段访问；如自行用 JSON 解析读 autoRun 配置，字段需写作 `autoRun["repeat"]`。

## 节点模块 (node)

节点模块提供了节点状态管理和心跳功能。

### node.createHeartbeat()

创建心跳数据。

```lua
local wingman = require("wingman")

local heartbeat = wingman.node.createHeartbeat()
-- {
--   json = "{...}",      -- 完整的 JSON 数据
--   nodeId = "node-abc123",
--   version = "0.1.0"
-- }
```

### node.sendHeartbeat(table)

发送心跳。**注意：当前实现是本地记录桩——只记日志、直接返回，不向服务器发送任何网络消息**（真正的网络心跳由 runtime agent 自身维护）。

```lua
local heartbeat = wingman.node.createHeartbeat()
wingman.node.sendHeartbeat(heartbeat)  -- 本地记录桩，无网络发送
```

### node.getWindows()

获取所有窗口列表（用于汇报游戏窗口状态）。

```lua
local windows = wingman.node.getWindows()
for i, win in ipairs(windows) do
    print(win.title)
    print(win.handle)
    print(win.isForeground)
    -- bounds: {x, y, width, height}
end
```

## 运行模式

### 1. 独立模式（默认）

脚本与触发器在客户端本地执行，不依赖服务器。

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

不存在 `WINGMAN_SERVER_*` 之类的配置覆盖变量。当前真实生效的环境变量：

| 环境变量 | 作用 |
|----------|--------|
| `WINGMAN_CONFIG_DIR` | 重定向配置目录（默认 `config`，多用于测试隔离） |
| `WINGMAN_PACK_PASSWORD` | `build` 加密打包的口令来源（避免口令进命令行历史） |
| `WINGMAN_SCRIPT_PASSWORD` | 运行加密打包产物时提供解包口令 |
| `WINGMAN_ADMIN_PASSWORD` | Go server 首次启动时引导创建 admin 账号（未设置则不建号） |

## 安全提示

⚠️ **密码明文存储**：当前配置文件中的密码以明文形式存储。在生产环境中，建议：
- 使用环境变量存储敏感信息
- 实现密码加密存储功能
- 使用 token 认证代替用户名密码
