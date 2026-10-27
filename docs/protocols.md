> [中文](zh/protocols.md) | **English**

# Wingman Communication Protocol Specification

> This document describes the protocol specification of Wingman's three communication links, based on the code implementation (calibrated 2026-10-04).
> Migration notes for the history are in [remote_protocol.md](./remote_protocol.md) (marked as a historical document).
> Architecture hard constraints are in [architecture-decisions.md](./architecture-decisions.md).

## Overview of the Three Links

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

Constraints: the runtime opens no HTTP/WebSocket control plane; the Dashboard connects only to the Go server; Local TCP IPC is only a debug fallback.

---

## ① Local IPC (Tauri GUI → Runtime)

The local control channel; the transport layer is a Named Pipe (Windows) / Unix Domain Socket (macOS/Linux).

### Frame Format

```
[uint32 LE payload_length] [JSON envelope bytes]
```

### JSON Envelope

```json
{
  "type": 0,
  "method": "system.getStatus",
  "payload": {},
  "id": 1,
  "timestamp": 1715299200000
}
```

The `type` enumeration (`IpcMessageType`):

| Value | Meaning |
|----|------|
| 0 | request (GUI → runtime) |
| 1 | response (runtime → GUI) |
| 2 | event (reserved; the local path uses drain instead, see below) |
| 3 | error |

> **Event delivery**: the local path uses a **pull model** — the runtime buffers events into `EventBuffer` and the GUI polls `events.drain` to pull them (avoiding misalignment between type=2 frames and response frames under Windows blocking IO). See "Runtime-to-UI Event Delivery" in architecture-decisions.md.

### Request/Response data Wrapping

The runtime's RPC dispatcher uniformly wraps handler return values:

```json
{ "type": "response", "id": "1", "data": { "success": true, "result": { ... } } }
```

The GUI side reads business fields from `data.result` (e.g. `data.result.paused`).

### Registered Methods

31 unique methods (26 registered in `apps/agent/src/rpc/handlers/`, 5 `trigger.*` registered in `lib/wingman/src/rpc/handlers/trigger_handler.cpp`):

| Method | Description |
|------|------|
| `system.getStatus` / `getVersion` | status/version |
| `system.togglePause` / `pauseAll` / `resumeAll` / `stopAll` / `isPaused` | script pause control |
| `script.list` / `start` / `stop` / `pause` / `resume` / `restart` / `unload` | script management |
| `trigger.list` / `add` / `remove` / `update` / `toggle` | trigger management |
| `screen.listMonitors` | monitor enumeration |
| `screenshot.capture` | screenshot (optional `displayId`, `region`) |
| `events.drain` | drain buffered events |
| `macro.start` / `stop` / `play` / `status` / `save` / `load` / `clear` | macro recording and playback |
| `config.getRemote` / `setRemote` | read/write of the remote registration config (local IPC only; the Go server side cannot change it) |

`events.drain` response: `{ events: [{method, payload, timestamp}], remaining: N, dropped: N }`. The event method set: `tray.show`/`tray.hide`/`tray.badge`/`tray.tooltip`, `trigger.fired`/`trigger.action`, `systemwatch.process`/`systemwatch.window`/`systemwatch.error`, `filewatcher.changed`/`filewatcher.error`, `macro.state`/`macro.recorded`, `script.state_changed`, `script.output`, `connection.ipc_client`, `connection.state_changed`. Request parameter `max` (default 500).

---

## ② Agent TCP (Runtime → Go Orchestrator)

The runtime acts as an **outbound agent** and actively connects to the Go server (default `127.0.0.1:8888`). The Go server listens and never dials back.

### Frame Format (16-byte header + JSON body)

```
MessageHeader (16 bytes, 小端):
  [0:4]   uint32 Length       体字节数
  [4:8]   uint32 Sequence     请求/响应配对
  [8]     uint8  Type         1=Request, 2=Response, 3=Notify, 4=Error
  [9:12]  3 字节 padding
  [12:16] uint32 Reserved
Body: JSON（最大 16 MiB）
```

### Notify (runtime → server, no response needed)

| type field | Description |
|-----------|------|
| `agent.register` | `{agentId, hostname, platform, capabilities, token?}` → server replies `agent.register_ack`. `platform` is currently `"desktop"` (Android uses a different value); `capabilities` is the device capability vocabulary (`screen.capture`, `input.mouse`, `window.enumerate`, etc., aligned with agent.cpp `desktopCapabilities()` and the server-side KnownCapabilities); `token` is carried only when a registration token is configured |
| `agent.heartbeat` | `{status, resources}` → server updates LastSeen/status |
| `agent.event` | generic events (`trigger_fired` / `script_state` / `script_output`; `log.line` and connection-type events are not forwarded) |
| raw `PING` (4-byte body) | server replies `PONG` + `UpdateHeartbeat` |
| `inbox.register` | `{agentId?}` (defaults to the registered agentId) → server replies `inbox.register_ack {success, agentId}` |
| `inbox.heartbeat` | `{agentId?}` → refreshes the registry heartbeat, replies `inbox.heartbeat_ack {timestamp}` |
| `inbox.ack` | `{msgId, agentId?}` → marks an inbox message as acknowledged (`GetMessages` only returns unacknowledged messages) |
| `inbox.report` | `{msgId, agentId?, result{}}` → records the message execution result |
| `team.join` | `{teamId, memberId, agentId?}` (agentId defaults to the registered agentId) → joins the team and broadcasts `team.joined` (the new member) / `team.member_joined` (the remaining members); on failure replies `team.error {error}` |
| `team.leave` | `{teamId, memberId?, agentId?}` → when memberId is absent the member is looked up in reverse by agentID; broadcasts `team.left`/`team.member_left` |
| `team.vote_create` | `{teamId, proposerId, subject, timeout?(ms)}` → timeout defaults to 30000; starts a vote, members receive `team.vote_started`, and `team.vote_ended` when it closes; on failure replies `team.error {error}` |
| `team.vote_cast` | `{voteId, memberId, response}` → counted toward the vote |
| `team.status_report` | `{teamId, memberId, status{}}` → forwarded as an inbox message to all members except the reporter |
| `team.broadcast` | `{teamId, memberId, message{}}` → forwarded as `team.broadcast_received {teamId, senderId, message}` to all members except the sender |
| `proxy.data` / `proxy.close` / `proxy.error` | relay frames for the remote proxy (Guacamole); `data` is base64, routed by `proxyId` (see `docs/remote-gateway-guacamole-design.md`) |

### Notify (server → runtime, pushed proactively)

| type field | Description |
|-----------|------|
| `agent.register_ack` | registration reply (`{success, agentId, ...}`) |
| `inbox.message` | inbox delivery frame: `{msgId, messageType, payload, timestamp}`. Pushed in real time while online; when the agent is offline it stays in the server's in-memory buffer (cleared on disconnect). `messageType` values: team collaboration `team.joined` / `team.member_joined` / `team.left` / `team.member_left` / `team.vote_started` / `team.vote_ended` / `team.status_report` / `team.broadcast_received`, plus custom types pushed at team scope (server trigger: `POST /api/teams/:teamId/messages`, permission `agents:manage`, members without a bound device are skipped). The desktop runtime consumes them through the apps/agent bridge: team collaboration type payloads are converted to `team.inbound` events driving the team module state machine (`wingman.team.on()` event subscription), custom types reach scripts as `team.<messageType>` events (`on("<messageType>")` subscription), and are acknowledged automatically via `inbox.ack` after handling. Polling consumption on the script side goes through `wingman.inbox.*` (separate connection + `connect`/`consume`/`ack`/`report`, see docs/api/inbox.md) and does not use this bridge. The Android runtime does not consume this frame |
| `inbox.register_ack` / `inbox.heartbeat_ack` | inbox registration/heartbeat replies (payloads in the table above) |
| `team.error` | `{error}`, the failure reason for team.join / team.vote_create |

### Request (server → runtime, Response required)

| method | Purpose |
|--------|------|
| `run_script` | `{path, content, contentSha256, language?}` — content is the script content inlined (agents without a server-side filesystem, such as Android, depend on it); contentSha256 is the content fingerprint (script version management, Execution records use the same convention as scriptSha256); workflow-engine dispatch additionally carries `timeout` |
| `stop_script` | `{script_id}` |
| `get_status` | query status |
| `list_windows` | enumerate windows |
| `screenshot.capture` | screenshot; Dispatcher Reuse forwards to the local IPC `screenshot.capture` handler (same parameters, same returns, see §① and `docs/API.md`); requires `WINGMAN_ENABLE_VISION` (OpenCV) |
| `trigger.list` / `add` / `remove` / `update` / `toggle` | trigger management; Dispatcher Reuse forwards to the local IPC `trigger.*` handlers (same parameters, same returns; field and type vocabulary see `docs/API.md`); accessed by the Dashboard through the Go server passthrough |
| `asset.sync` | `{name, contentBase64, sha256}` — distribution of assets such as template images; the agent writes them to the local asset directory (`<scriptDir>/assets/<name>`), and the response data carries the written path and byte count; the name allowlist is `[A-Za-z0-9][A-Za-z0-9._-]{0,127}` (the same convention on both ends), providing sha256 enables strict verification, single asset ≤1MB (server trigger: `POST /api/assets/sync`, permission `assets:manage`) |
| `system.shutdown` | shut down the agent |

> The 8 rows above are the complete dispatch of `Agent::handleRemoteCommand` (`apps/agent/src/agent.cpp`): `run_script` / `system.shutdown` / `list_windows` / `get_status` / `screenshot.capture` / the `trigger.` prefix (covering the 5 methods in the table above) / the `asset.` prefix (currently only `asset.sync` is registered) / `stop_script`. The three dotted categories go through Dispatcher Reuse (the same set of RPC handlers shared with local IPC, see `dispatchViaRpcDispatcher`). The Android on-device side is not a full implementation: `apps/android/cpp/agent/android_agent.cpp` only dispatches the five categories `run_script` / `stop_script` / `screenshot.capture` / the `asset.` prefix / `system.shutdown`, with no `get_status` / `list_windows` / `trigger.` branches (and no local IPC surface at all).

Response: `{success, method, message?, error?, data?}`, with `sequence` identical to the request.

### Heartbeat and Timeouts

- runtime: sends `agent.heartbeat` every `heartbeat_interval` (default 30s)
- server: `StartHeartbeatCheck` checks every 30s; exceeding `heartbeat` (default 90s) is judged offline and broadcasts `disconnected`
- runtime reconnect: exponential backoff (base `reconnect_interval`=5s, cap `max_reconnect_interval`=60s, plus jitter), counter reset on a successful connection

---

## ③ Dashboard WebSocket (Dashboard → Go Orchestrator)

The upgrade endpoint is `GET /ws?token=<JWT>` (or `Authorization: Bearer`), with same-origin verification to prevent CSWSH.

### Server → Client Messages

```json
{ "type": "agent", "event": "connected", "data": {...}, "timestamp": 1715299200 }
```

> The position of the event field is inconsistent: `agent` / `debugger` / `room` use a top-level `event`; `workflow` sets only the top-level `type`, with the event name embedded in `data.event` (e.g. `{type:"workflow", data:{event:"submitted", ...}}`), and the receiver reads `data.event`.

| `type` | `event` | Description |
|--------|---------|------|
| `connected` | — | connection welcome (includes connectionId) |
| `agent` | `connected` / `disconnected` / `status_changed` | Agent lifecycle (top-level `event`) |
| `workflow` | `submitted` / `status_changed` / `progress` | workflow progress (the event is in `data.event`, not top level) |
| `debugger` | * | debug events (top-level `event`; currently in direct-connect mode, not relayed) |
| `screenshot` | — | screenshot broadcast |
| `room` | `message` | room message broadcast (triggered by the client's `room_message`, includes `roomId` and `data`) |
| `ping` | — | server heartbeat: WS protocol-level `PingMessage` (one every 30s, not a JSON message); the client replies with `PongMessage` (automatic at the protocol level) or JSON `{type:"pong"}` |

### Client → Server Messages

| type | Description |
|------|------|
| `pong` | heartbeat reply |
| `join_room` / `leave_room` / `room_message` | rooms (`BroadcastToRoom`) |

### Rooms

`Hub` maintains `rooms map[roomID]map[connID]*Connection`, supporting per-room broadcast (`JoinRoom`/`LeaveRoom`/`BroadcastToRoom`). Each connection's `Send` channel buffers 256; slow-consuming connections are cleaned up. When a client `room_message` arrives, the server broadcasts `{type:"room", event:"message", roomId, data, timestamp}` to the other connections in the same room (`BroadcastToRoom`; when connections in the room have full channels the message is silently dropped, and they are not cleaned up).

---

## Authentication

- **Dashboard HTTP/WS**: JWT (HS256, secret `WINGMAN_JWT_SECRET` ≥32 characters), 15-minute expiry. RBAC: `admin`/`operator`/`viewer` roles + `resource:action` permission codes. The login endpoint has 429 rate limiting.
- **Agent TCP**: registration token allowlist (server-side `WINGMAN_AGENT_TOKENS` environment variable; the agent carries `token` in `agent.register`, and the server rejects registration when verification fails). By default verification is off when unconfigured and every registration is accepted; it relies on the network boundary.
- **Local IPC**: implicit trust of the local machine (local access to Named Pipe/UDS).