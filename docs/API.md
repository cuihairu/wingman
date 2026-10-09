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
wingman-agent start

# 指定配置文件
wingman-agent start --config agent.toml

# 本地 GUI / 单机模式，启动本地 IPC listener
wingman-agent start --standalone
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

当前共注册 **26 个**方法（以 `apps/agent/src/rpc/handlers/` 的 `registerHandler` 为准）：

| 分组 | 方法 |
|------|------|
| system | `getStatus` `getVersion` `isPaused` `togglePause` `pauseAll` `resumeAll` `stopAll` |
| script | `list` `start` `stop` `pause` `resume` `restart` `unload` |
| screenshot | `capture` |
| screen | `listMonitors` |
| events | `drain` |
| macro | `start` `stop` `play` `status` `save` `load` `clear` |
| config | `getRemote` `setRemote` |

> `trigger.*`（`list`/`add`/`remove`/`update`/`toggle`）不在本地 IPC 方法面——它仅由 runtime 在远程 agent 通道复用（`apps/agent/src/agent.cpp`），见 `docs/protocols.md`。
>
> 远程编排命令（如 `system.shutdown`）同样走 agent transport 通道（`apps/agent/src/agent.cpp`），不属于本地 IPC 面。

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

#### script.list
列出所有可用脚本

```json
{ "type": 0, "method": "script.list", "payload": {}, "id": 3, "timestamp": 1715299200000 }
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
  "id": 4,
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
  "id": 5,
  "timestamp": 1715299200000
}
```

#### script.pause / script.resume
暂停/恢复运行中的脚本（`scriptId` 必填）

```json
{
  "type": 0,
  "method": "script.pause",
  "payload": { "scriptId": "script-id" },
  "id": 6,
  "timestamp": 1715299200000
}
```

#### script.restart
重启已加载脚本：对已运行/已暂停的脚本 stop → start（路径复用，ID 不变）

```json
{
  "type": 0,
  "method": "script.restart",
  "payload": { "scriptId": "script-id" },
  "id": 7,
  "timestamp": 1715299200000
}
```

#### script.unload
从脚本表移除（仅对非运行中的脚本有效；运行中先 stop）

```json
{
  "type": 0,
  "method": "script.unload",
  "payload": { "scriptId": "script-id" },
  "id": 8,
  "timestamp": 1715299200000
}
```

#### system.isPaused
查询是否存在已暂停脚本，返回 `{ "paused": bool }`

```json
{ "type": 0, "method": "system.isPaused", "payload": {}, "id": 9, "timestamp": 1715299200000 }
```

#### system.togglePause
切换全局暂停：有暂停则全部恢复，否则全部暂停。返回 `{ "paused": bool, "changedScripts": int }`

```json
{ "type": 0, "method": "system.togglePause", "payload": {}, "id": 10, "timestamp": 1715299200000 }
```

#### system.pauseAll / system.resumeAll
全部暂停/全部恢复。返回 `{ "paused": bool, "changedScripts": int }`

```json
{ "type": 0, "method": "system.pauseAll", "payload": {}, "id": 11, "timestamp": 1715299200000 }
```

#### system.stopAll
全部停止。返回 `{ "stoppedScripts": int }`

```json
{ "type": 0, "method": "system.stopAll", "payload": {}, "id": 12, "timestamp": 1715299200000 }
```

#### screenshot.capture
截取屏幕为 JPEG（base64 data URL）。需要 `WINGMAN_ENABLE_VISION`（OpenCV），否则返回错误。可选参数：
- `region` - `{x, y, width, height}`；省略或空 → 整屏（或指定显示器）
- `displayId` - 显示器索引（-1/省略 → 主屏）；`region` 相对该显示器原点

返回 `{ "image": "data:image/jpeg;base64,...", "width", "height", "timestamp", "region" }`

```json
{
  "type": 0,
  "method": "screenshot.capture",
  "payload": { "displayId": 0, "region": { "x": 0, "y": 0, "width": 800, "height": 600 } },
  "id": 13,
  "timestamp": 1715299200000
}
```

#### screen.listMonitors
列出显示器，返回 `{ "monitors": [{ "id", "name", "isPrimary", "bounds" }], "primaryId": int }`

```json
{ "type": 0, "method": "screen.listMonitors", "payload": {}, "id": 14, "timestamp": 1715299200000 }
```

#### events.drain
拉取本地事件流（GUI 轮询）。事件 method 集合：`tray.show`/`tray.hide`/`tray.badge`/`tray.tooltip`（脚本托盘意图）、`trigger.fired`/`trigger.action`、`systemwatch.process`/`systemwatch.window`/`systemwatch.error`、`filewatcher.changed`/`filewatcher.error`、`macro.state`/`macro.recorded`、`script.state_changed`、`script.output`、`connection.ipc_client`。参数 `max`（默认 500，单次拉取上限）。返回：

```json
{
  "type": 0,
  "method": "events.drain",
  "payload": { "max": 500 },
  "id": 15,
  "timestamp": 1715299200000
}
```

响应 `result`：`{ "events": [...], "remaining": int, "dropped": int }`（`remaining` = 缓冲区剩余，`dropped` = 累计丢弃数）。

#### macro.start / macro.stop / macro.status / macro.clear
宏录制控制。`status` 返回 `{ "recording": bool, "paused": bool, "eventCount": int }`；其余无参数。

```json
{ "type": 0, "method": "macro.status", "payload": {}, "id": 16, "timestamp": 1715299200000 }
```

#### macro.play
回放宏。参数：`speed`（百分比，默认 100，内部钳制 ≥1）、`repeat`（次数，默认 1，钳制 ≥1）。**回放在调用线程同步执行，GUI 侧应异步调用避免阻塞**。

```json
{
  "type": 0,
  "method": "macro.play",
  "payload": { "speed": 150, "repeat": 2 },
  "id": 17,
  "timestamp": 1715299200000
}
```

#### macro.save / macro.load
按 JSON 路径保存/加载宏（`path` 必填）。返回 `{ "success": true, "eventCount": int }`

```json
{
  "type": 0,
  "method": "macro.save",
  "payload": { "path": "macros/demo.json" },
  "id": 18,
  "timestamp": 1715299200000
}
```

#### config.getRemote / config.setRemote
读取/应用远程注册配置（`serverIp` / `serverPort` / `registerToken`，仅本地 IPC，无对应远程命令）。`setRemote` 请求体为配置补丁（部分更新），成功返回应用后的完整配置；运行中生效会触发 agent 重连。

```json
{
  "type": 0,
  "method": "config.setRemote",
  "payload": { "serverIp": "192.168.1.10", "serverPort": 9000, "registerToken": "..." },
  "id": 19,
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
