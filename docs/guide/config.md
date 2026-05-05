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

### AutoRun (自动运行配置)

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `enabled` | boolean | `false` | 是否启用自动运行脚本 |
| `scriptPath` | string | `""` | 脚本文件路径（相对于可执行文件） |
| `delaySeconds` | number | `0` | 启动延迟（秒） |
| `repeat` | boolean | `false` | 是否重复运行 |
| `repeatInterval` | number | `0` | 重复间隔（秒），0 表示脚本自己控制循环 |

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
config.setTray(tray)

-- 自动运行配置
local autoRun = config.getAutoRun()
autoRun.enabled = true
autoRun.scriptPath = "scripts/auto_task.lua"
autoRun.delaySeconds = 5
autoRun.repeat = true
autoRun.repeatInterval = 60
config.setAutoRun(autoRun)

-- 通用键值对
config.set("myKey", "myValue")
local value = config.get("myKey")
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
