# Wingman 通信协议规范

> 本文档描述 Wingman 三条通信链路的协议规范，依据代码实现（2026-10-04 校准）。
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

## ① Local IPC（Tauri GUI → Runtime）

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

31 个唯一方法（26 个注册于 `apps/agent/src/rpc/handlers/`，5 个 `trigger.*` 注册于 `lib/wingman/src/rpc/handlers/trigger_handler.cpp`）：

| 方法 | 说明 |
|------|------|
| `system.getStatus` / `getVersion` | 状态/版本 |
| `system.togglePause` / `pauseAll` / `resumeAll` / `stopAll` / `isPaused` | 脚本暂停控制 |
| `script.list` / `start` / `stop` / `pause` / `resume` / `restart` / `unload` | 脚本管理 |
| `trigger.list` / `add` / `remove` / `update` / `toggle` | 触发器管理 |
| `screen.listMonitors` | 显示器枚举 |
| `screenshot.capture` | 截图（可选 `displayId`、`region`） |
| `events.drain` | 拉取缓冲事件 |
| `macro.start` / `stop` / `play` / `status` / `save` / `load` / `clear` | 宏录制与回放 |
| `config.getRemote` / `setRemote` | 远程注册配置读写（仅本地 IPC，Go server 侧改不了） |

`events.drain` 响应：`{ events: [{method, payload, timestamp}], remaining: N, dropped: N }`。事件 method 集合：`tray.show`/`tray.hide`/`tray.badge`/`tray.tooltip`、`trigger.fired`/`trigger.action`、`systemwatch.process`/`systemwatch.window`/`systemwatch.error`、`filewatcher.changed`/`filewatcher.error`、`macro.state`/`macro.recorded`、`script.state_changed`、`script.output`、`connection.ipc_client`、`connection.state_changed`。请求参数 `max`（默认 500）。

---

## ② Agent TCP（Runtime → Go Orchestrator）

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
| `agent.register` | `{agentId, hostname, platform, capabilities, token?}` → server 回 `agent.register_ack`。`platform` 当前为 `"desktop"`（Android 端另有值）；`capabilities` 为设备能力词汇（`screen.capture`、`input.mouse`、`window.enumerate` 等，见 agent.cpp `desktopCapabilities()` 与 server 侧 KnownCapabilities 对齐）；`token` 仅在配置了注册令牌时携带 |
| `agent.heartbeat` | `{status, resources}` → server 更新 LastSeen/状态 |
| `agent.event` | 通用事件（`trigger_fired` / `script_state` / `script_output`；`log.line` 与连接类事件不转发） |
| 原始 `PING`（4 字节体） | server 回 `PONG` + `UpdateHeartbeat` |
| `inbox.register` | `{agentId?}`（缺省取注册 agentId）→ server 回 `inbox.register_ack {success, agentId}` |
| `inbox.heartbeat` | `{agentId?}` → 刷新注册表心跳，回 `inbox.heartbeat_ack {timestamp}` |
| `inbox.ack` | `{msgId, agentId?}` → 标记收件箱消息已确认（`GetMessages` 只回未确认消息） |
| `inbox.report` | `{msgId, agentId?, result{}}` → 记录消息执行结果 |
| `team.join` | `{teamId, memberId, agentId?}`（agentId 缺省取注册 agentId）→ 入团并向成员广播 `team.joined`（新成员）/`team.member_joined`（其余成员）；失败回 `team.error {error}` |
| `team.leave` | `{teamId, memberId?, agentId?}` → memberId 缺省按 agentID 反查成员；广播 `team.left`/`team.member_left` |
| `team.vote_create` | `{teamId, proposerId, subject, timeout?(ms)}` → timeout 缺省 30000；发起投票，成员收 `team.vote_started`，截止后收 `team.vote_ended`；失败回 `team.error {error}` |
| `team.vote_cast` | `{voteId, memberId, response}` → 计入投票 |
| `team.status_report` | `{teamId, memberId, status{}}` → 以收件箱消息转发给除上报者外的全部成员 |
| `team.broadcast` | `{teamId, memberId, message{}}` → 以 `team.broadcast_received {teamId, senderId, message}` 转发给除发送者外的全部成员 |
| `proxy.data` / `proxy.close` / `proxy.error` | 远程代理（Guacamole）中继帧，`data` 为 base64，按 `proxyId` 路由（见 `docs/remote-gateway-guacamole-design.md`） |

### Notify（server → runtime，主动下发）

| type 字段 | 说明 |
|-----------|------|
| `agent.register_ack` | 注册应答（`{success, agentId, ...}`） |
| `inbox.message` | 收件箱投递帧：`{msgId, messageType, payload, timestamp}`。在线实时推送；agent 离线时留在 server 内存缓冲（断连即清）。`messageType` 取值：团队协同面 `team.joined` / `team.member_joined` / `team.left` / `team.member_left` / `team.vote_started` / `team.vote_ended` / `team.status_report` / `team.broadcast_received`，以及团队范围推送的自定义类型（server 触发：`POST /api/teams/:teamId/messages`，权限 `agents:manage`，未绑定设备的成员跳过）。桌面 runtime 经 apps/agent 桥消费：团队协同类型载荷转 `team.inbound` 事件驱动 team 模块状态机（`wingman.team.on()` 事件订阅），自定义类型以 `team.<messageType>` 事件触达脚本（`on("<messageType>")` 订阅），处理完自动经 `inbox.ack` 确认。脚本侧轮询消费走 `wingman.inbox.*`（独立连接 + `connect`/`consume`/`ack`/`report`，见 docs/api/inbox.md），不经此桥。Android runtime 不消费此帧 |
| `inbox.register_ack` / `inbox.heartbeat_ack` | 收件箱注册/心跳应答（载荷见上表） |
| `team.error` | `{error}`，team.join / team.vote_create 失败原因 |

### Request（server → runtime，需 Response）

| method | 用途 |
|--------|------|
| `run_script` | `{path, content, contentSha256, language?}` —— content 为脚本内容内联（Android 等无服务器文件系统的 agent 依赖）；contentSha256 为内容指纹（脚本版本管理，Execution 记录同口径 scriptSha256）；workflow 引擎下发另带 `timeout` |
| `stop_script` | `{script_id}` |
| `get_status` | 查询状态 |
| `list_windows` | 枚举窗口 |
| `screenshot.capture` | 截图，Dispatcher Reuse 转本地 IPC 的 `screenshot.capture` handler（同参同返，见 §① 与 `docs/API.md`），需 `WINGMAN_ENABLE_VISION`（OpenCV） |
| `trigger.list` / `add` / `remove` / `update` / `toggle` | 触发器管理，Dispatcher Reuse 转本地 IPC 的 `trigger.*` handler（同参同返，字段与类型词表见 `docs/API.md`）；Dashboard 经 Go server 透传访问 |
| `asset.sync` | `{name, contentBase64, sha256}` 模板图片等资产下发，agent 落盘本地资产目录（`<scriptDir>/assets/<name>`），响应 data 携带落盘路径与字节数；name 白名单 `[A-Za-z0-9][A-Za-z0-9._-]{0,127}`（两端同口径），sha256 提供即强校验，单资产 ≤1MB（server 触发：`POST /api/assets/sync`，权限 `assets:manage`） |
| `system.shutdown` | 关闭 agent |

> 上表 8 行即 `Agent::handleRemoteCommand` 的全部分派（`apps/agent/src/agent.cpp`）：`run_script` / `system.shutdown` / `list_windows` / `get_status` / `screenshot.capture` / `trigger.` 前缀（覆盖上表 5 个 method） / `asset.` 前缀（当前只注册 `asset.sync`） / `stop_script`。带点号的三类走 Dispatcher Reuse（同一组 RPC handler 与本地 IPC 共用，见 `dispatchViaRpcDispatcher`）。Android 端侧不是全量实现：`apps/android/cpp/agent/android_agent.cpp` 只分派 `run_script` / `stop_script` / `screenshot.capture` / `asset.` 前缀 / `system.shutdown` 五类，无 `get_status` / `list_windows` / `trigger.` 分支（也没有本地 IPC 面）。

Response：`{success, method, message?, error?, data?}`，`sequence` 与请求一致。

### 心跳与超时

- runtime：每 `heartbeat_interval`（默认 30s）发 `agent.heartbeat`
- server：`StartHeartbeatCheck` 每 30s 检查，超过 `heartbeat`（默认 90s）判定 offline 并广播 `disconnected`
- runtime 重连：指数退避（base `reconnect_interval`=5s，cap `max_reconnect_interval`=60s，加抖动），连接成功重置计数

---

## ③ Dashboard WebSocket（Dashboard → Go Orchestrator）

升级端点 `GET /ws?token=<JWT>`（或 `Authorization: Bearer`），同源校验防 CSWSH。

### 服务端 → 客户端消息

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

### 客户端 → 服务端消息

| type | 说明 |
|------|------|
| `pong` | 心跳响应 |
| `join_room` / `leave_room` / `room_message` | 房间（`BroadcastToRoom`） |

### 房间

`Hub` 维护 `rooms map[roomID]map[connID]*Connection`，支持按房间广播（`JoinRoom`/`LeaveRoom`/`BroadcastToRoom`）。每个连接 `Send` 通道缓冲 256，慢消费连接被清理。

---

## 鉴权

- **Dashboard HTTP/WS**：JWT（HS256，密钥 `WINGMAN_JWT_SECRET` ≥32 字符），15 分钟过期。RBAC：`admin`/`operator`/`viewer` 角色 + `resource:action` 权限码。登录接口有 429 限流。
- **Agent TCP**：注册令牌白名单（server 侧 `WINGMAN_AGENT_TOKENS` 环境变量；agent 在 `agent.register` 里带 `token`，server 校验失败拒绝注册）。默认未配置时关闭校验、注册即纳管，靠网络边界。
- **Local IPC**：本机隐式信任（Named Pipe/UDS 本地访问）。
