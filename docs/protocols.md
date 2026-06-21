# Wingman 通信协议规范

> 本文档描述 Wingman 三条通信链路的权威协议规范，依据代码实现（2026-06-20 校准）。
> 历史迁移说明见 [remote_protocol.md](./remote_protocol.md)（已标为历史文档）。
> 架构硬约束见 [architecture-decisions.md](./architecture-decisions.md)。

## 三条链路总览

```
┌──────────────┐   ① Local IPC    ┌──────────┐
│ Tauri GUI    │ ───────────────▶ │ Runtime  │  (C++ agent，本机)
│ (Svelte)     │ ◀─────────────── │          │
└──────────────┘   JSON envelope  └──────────┘
        │                                │
        │ invoke(Rust)                   │ ② Agent TCP（outbound）
        ▼                                ▼
┌──────────────┐   ③ WebSocket   ┌──────────┐
│ Dashboard    │ ───────────────▶│ Go       │ ◀─── Runtime 主动连接
│ (React/Umi)  │ ◀─────────────── │ Orchestrator│
└──────────────┘  JWT 鉴权        └──────────┘
```

约束：Runtime 不开 HTTP/WebSocket 控制面；Dashboard 只连 Go server；Local TCP IPC 仅 debug fallback。

---

## ① Local IPC（Tauri GUI ↔ Runtime）

本地控制通道，传输层为 Named Pipe（Windows）/ Unix Domain Socket（macOS/Linux）。

### 帧格式

```
[uint32 LE payload_length] [JSON envelope bytes]
```

### JSON envelope

```json
{
  "type": 0,
  "method": "system.getStatus",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

`type` 枚举（`IpcMessageType`）：

| 值 | 含义 |
|----|------|
| 0 | request（GUI → runtime） |
| 1 | response（runtime → GUI） |
| 2 | event（预留，本地路径用 drain 代替，见下） |
| 3 | error |

> **事件下发**：本地路径采用 **pull 模型**——runtime 把事件缓冲到 `EventBuffer`，GUI 轮询 `events.drain` 拉取（避免 Windows 阻塞 IO 下 type=2 帧与 response 帧错位）。详见 architecture-decisions.md「Runtime-to-UI Event Delivery」。

### 请求/响应 data 包装

runtime 的 RPC dispatcher 对 handler 返回值统一包装：

```json
{ "type": "response", "id": "1", "data": { "success": true, "result": { ... } } }
```

GUI 侧从 `data.result` 取业务字段（如 `data.result.paused`）。

### 已注册方法

| 方法 | 说明 |
|------|------|
| `system.getStatus` / `getVersion` | 状态/版本 |
| `system.togglePause` / `pauseAll` / `resumeAll` / `stopAll` / `isPaused` | 脚本暂停控制 |
| `script.list` / `script.start` / `script.stop` | 脚本管理 |
| `trigger.list` / `add` / `remove` / `update` / `toggle` | 触发器管理 |
| `screen.listMonitors` | 显示器枚举 |
| `screenshot.capture` | 截图（可选 `displayId`） |
| `events.drain` | 拉取缓冲事件（`log.line` / `trigger.fired` / 预留 `screenshot.frame`） |

`events.drain` 响应：`{ events: [{method, payload, timestamp}], remaining: N }`。

---

## ② Agent TCP（Runtime ↔ Go Orchestrator）

Runtime 作为 **outbound agent** 主动连接 Go server（默认 `127.0.0.1:8888`）。Go server 监听，不反向拨入。

### 帧格式（16 字节头 + JSON 体）

```
MessageHeader (16 bytes, 小端):
  [0:4]   uint32 Length       体字节数
  [4:8]   uint32 Sequence     请求/响应配对
  [8]     uint8  Type         1=Request, 2=Response, 3=Notify, 4=Error
  [9:12]  3 字节 padding
  [12:16] uint32 Reserved
Body: JSON（最大 16 MiB）
```

### Notify（runtime → server，无需响应）

| type 字段 | 说明 |
|-----------|------|
| `agent.register` | `{agentId, hostname}` → server 回 `agent.register_ack` |
| `agent.heartbeat` | `{status, resources}` → server 更新 LastSeen/状态 |
| `agent.event` | 通用事件 |
| 原始 `PING`（4 字节体） | server 回 `PONG` + `UpdateHeartbeat` |

### Request（server → runtime，需 Response）

| method | 用途 |
|--------|------|
| `run_script` | `{path, timeout, ...params}` |
| `stop_script` | `{script_id}` |
| `get_status` | 查询状态 |
| `list_windows` | 枚举窗口 |
| `system.shutdown` | 关闭 agent |

Response：`{success, method, message?, error?, data?}`，`sequence` 与请求一致。

### 心跳与超时

- runtime：每 `heartbeat_interval`（默认 30s）发 `agent.heartbeat`
- server：`StartHeartbeatCheck` 每 30s 检查，超过 `heartbeat`（默认 90s）判定 offline 并广播 `disconnected`
- runtime 重连：指数退避（base `reconnect_interval`=5s，cap `max_reconnect_interval`=60s，加抖动），连接成功重置计数

---

## ③ Dashboard WebSocket（Dashboard ↔ Go Orchestrator）

升级端点 `GET /ws?token=<JWT>`（或 `Authorization: Bearer`），同源校验防 CSWSH。

### 服务端→客户端消息

```json
{ "type": "agent", "event": "connected", "data": {...}, "timestamp": 1715299200 }
```

| `type` | `event` | 说明 |
|--------|---------|------|
| `connected` | — | 连接欢迎（含 connectionId） |
| `agent` | `connected` / `disconnected` / `status_changed` | Agent 生命周期 |
| `workflow` | `submitted` / `status_changed` / `progress` | 工作流进度 |
| `debugger` | * | 调试事件（当前直连模式，不中转） |
| `screenshot` | — | 截图广播 |
| `ping` | — | 服务端 ping，客户端回 pong |

### 客户端→服务端消息

| type | 说明 |
|------|------|
| `pong` | 心跳响应 |
| `join_room` / `leave_room` / `room_message` | 房间（`BroadcastToRoom`） |

### 房间

`Hub` 维护 `rooms map[roomID]map[connID]*Connection`，支持按房间广播（`JoinRoom`/`LeaveRoom`/`BroadcastToRoom`）。每个连接 `Send` 通道缓冲 256，慢消费连接被清理。

---

## 鉴权

- **Dashboard HTTP/WS**：JWT（HS256，密钥 `WINGMAN_JWT_SECRET` ≥32 字符），15 分钟过期。RBAC：`admin`/`operator`/`viewer` 角色 + `resource:action` 权限码。
- **Agent TCP**：暂无应用层鉴权（依赖网络边界）；agent 注册即纳管。
- **Local IPC**：本机隐式信任（Named Pipe/UDS 本地访问）。
