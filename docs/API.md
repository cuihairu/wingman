# Wingman Runtime API 文档

> **已废弃/待迁移**: 本文记录的是旧 runtime WebSocket RPC 形态，不是当前目标架构。Runtime 不应提供 WebSocket/HTTP server 作为本地 UI 或远程控制面。
>
> 当前约束见 `docs/architecture-decisions.md`：
>
> - 远程编排: `runtime agent -> outbound transport -> Go orchestrator -> dashboard`
> - 本地单机 UI: `Tauri UI -> Tauri Rust backend -> local IPC -> runtime`
>
> 后续应将本文迁移为 Go orchestrator API 和 local IPC command API 文档。

Wingman Runtime 的本地控制面应通过本地 IPC 提供，供 Tauri UI 调用。

## 启动服务器

```bash
# 默认配置
wingman-runtime start

# 指定配置文件
wingman-runtime start --config agent.toml

# 本地 GUI / 单机模式，启动本地 IPC
wingman-runtime start --standalone
```

> 远程 Agent 使用 `agent.toml` 中的 `server_ip` / `server_port` 连接 Go orchestrator。本地 GUI 使用 `--standalone` 启动 runtime local IPC。

## 本地 IPC 命令接口

IPC 传输应使用长度前缀帧，例如 `uint32 length + JSON payload`。具体 transport 按平台自动选择：Windows 默认 Named Pipe，macOS/Linux 默认 Unix Domain Socket。

### 消息格式

**请求消息:**
```json
{
  "type": "call",
  "method": "script.list",
  "params": {}
}
```

**成功响应:**
```json
{
  "type": "response",
  "data": {
    "success": true,
    "result": { ... }
  }
}
```

**错误响应:**
```json
{
  "type": "response",
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
{ "type": "call", "method": "system.getStatus", "params": {} }
```

#### system.getVersion
获取版本信息

```json
{ "type": "call", "method": "system.getVersion", "params": {} }
```

#### trigger.list
列出所有触发器

```json
{ "type": "call", "method": "trigger.list", "params": {} }
```

#### trigger.add
添加触发器

```json
{
  "type": "call",
  "method": "trigger.add",
  "params": {
    "config": { ... }
  }
}
```

#### trigger.remove
删除触发器

```json
{
  "type": "call",
  "method": "trigger.remove",
  "params": {
    "id": "trigger-id"
  }
}
```

#### trigger.update
更新触发器

```json
{
  "type": "call",
  "method": "trigger.update",
  "params": {
    "id": "trigger-id",
    "config": { ... }
  }
}
```

#### trigger.toggle
启用/禁用触发器

```json
{
  "type": "call",
  "method": "trigger.toggle",
  "params": {
    "id": "trigger-id"
  }
}
```

#### script.list
列出所有可用脚本

```json
{ "type": "call", "method": "script.list", "params": {} }
```

#### script.start
启动指定的脚本

```json
{
  "type": "call",
  "method": "script.start",
  "params": {
    "id": "script-id"
  }
}
```

#### script.stop
停止运行中的脚本

```json
{
  "type": "call",
  "method": "script.stop",
  "params": {
    "id": "script-id"
  }
}
```

### 心跳/Ping
```json
{ "type": "ping" }
```

**Pong 响应:**
```json
{ "type": "pong", "timestamp": 1715299200000 }
```

## 使用示例

### JavaScript (Tauri)
```javascript
import { invoke } from '@tauri-apps/api/core';

const status = await invoke('call_command', {
  method: 'system.getStatus',
  params: {}
});
```
