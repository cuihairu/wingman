# Wingman Runtime Local IPC API

> 本文记录当前 runtime local IPC 控制面。Runtime 不应提供 WebSocket/HTTP server 作为本地 UI 或远程控制面。
>
> 当前约束见 `docs/architecture-decisions.md`：
>
> - 远程编排: `runtime agent -> outbound transport -> Go orchestrator -> dashboard`
> - 本地单机 UI: `Tauri UI -> Tauri Rust backend -> local IPC -> runtime`
>
> Go orchestrator 的远程 API 应独立记录；不要把 dashboard/browser WebSocket 协议混入 runtime local IPC。

Wingman Runtime 的本地控制面应通过本地 IPC 提供，供 Tauri UI 调用。

## 启动 runtime

```bash
# 默认配置
wingman-runtime start

# 指定配置文件
wingman-runtime start --config agent.toml

# 本地 GUI / 单机模式，启动本地 IPC listener
wingman-runtime start --standalone
```

> 远程 Agent 使用 `agent.toml` 中的 `server_ip` / `server_port` 连接 Go orchestrator。本地 GUI 使用 `--standalone` 启动 runtime local IPC。

## 本地 IPC 命令接口

IPC 传输使用长度前缀帧：`uint32 little-endian length + JSON envelope`。具体 transport 按平台自动选择：Windows 默认 Named Pipe，macOS/Linux 默认 Unix Domain Socket。

当前 IPC envelope 由 GUI Rust backend 和 C++ runtime 共享。不要把这里改成 WebSocket JSON-RPC，也不要为本地 UI 增加 HTTP/WebSocket server。

### 消息格式

**Wire 请求 envelope:**
```json
{
  "type": 0,
  "method": "script.list",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`type` 当前使用数字枚举：

- `0`: request
- `1`: response
- `2`: event
- `3`: error

**Wire 响应 envelope:**
```json
{
  "type": 1,
  "method": "script.list",
  "payload": {
    "type": "response",
    "id": "1",
    "data": {
      "success": true,
      "result": {}
    }
  },
  "id": 1,
  "timestamp": 1715299200100
}
```

GUI Rust backend 返回给 Tauri command 的是 envelope 内的 `payload`。

**Dispatcher 错误 payload:**
```json
{
  "type": "response",
  "id": "1",
  "data": {
    "success": false,
    "error": "错误信息"
  }
}
```

### 支持的 RPC 方法

#### system.getStatus
获取系统状态

```json
{ "type": 0, "method": "system.getStatus", "payload": {}, "id": 1, "timestamp": 1715299200000 }
```

#### system.getVersion
获取版本信息

```json
{ "type": 0, "method": "system.getVersion", "payload": {}, "id": 2, "timestamp": 1715299200000 }
```

#### trigger.list
列出所有触发器

```json
{ "type": 0, "method": "trigger.list", "payload": {}, "id": 3, "timestamp": 1715299200000 }
```

#### trigger.add
添加触发器

```json
{
  "type": 0,
  "method": "trigger.add",
  "payload": {
    "config": {
      "name": "Detect image",
      "enabled": true,
      "condition": {
        "type": "ImageFound",
        "value": "assets/button.png",
        "interval": 1000,
        "tolerance": 10
      },
      "actions": []
    }
  },
  "id": 4,
  "timestamp": 1715299200000
}
```

#### trigger.remove
删除触发器

```json
{
  "type": 0,
  "method": "trigger.remove",
  "payload": {
    "id": "trigger-id"
  },
  "id": 5,
  "timestamp": 1715299200000
}
```

#### trigger.update
更新触发器

```json
{
  "type": 0,
  "method": "trigger.update",
  "payload": {
    "id": "trigger-id",
    "config": {
      "name": "Updated trigger",
      "enabled": true
    }
  },
  "id": 6,
  "timestamp": 1715299200000
}
```

#### trigger.toggle
启用/禁用触发器

```json
{
  "type": 0,
  "method": "trigger.toggle",
  "payload": {
    "id": "trigger-id"
  },
  "id": 7,
  "timestamp": 1715299200000
}
```

#### script.list
列出所有可用脚本

```json
{ "type": 0, "method": "script.list", "payload": {}, "id": 8, "timestamp": 1715299200000 }
```

#### script.start
启动指定的脚本

```json
{
  "type": 0,
  "method": "script.start",
  "payload": {
    "path": "scripts/example.lua"
  },
  "id": 9,
  "timestamp": 1715299200000
}
```

#### script.stop
停止运行中的脚本

```json
{
  "type": 0,
  "method": "script.stop",
  "payload": {
    "scriptId": "script-id"
  },
  "id": 10,
  "timestamp": 1715299200000
}
```

### 心跳/Ping

当前 runtime local IPC 未实现独立 ping/pong。GUI 应使用 `system.getStatus` 作为连接健康检查。

## 使用示例

### JavaScript (Tauri)
```javascript
import { invoke } from '@tauri-apps/api/core';

await invoke('connect_ipc', { endpoint: 'wingman' });
const status = await invoke('get_system_status');
```

当前 GUI 已使用专门的 Tauri commands，例如 `connect_ipc`、`get_system_status`、`get_scripts`。新增 UI 功能应优先复用这些 commands，或新增 Tauri command 通过 Rust IPC client 调用 runtime。
