> [中文](remote_protocol.md) | **English**

# Wingman Remote Control Protocol Document

> [**Historical document**: This document records the migration notes for the legacy JSON-RPC protocol. The current authoritative protocol specification is in [protocols.md](./protocols.md).]
>
> Note: The "Protobuf serialization" described earlier in this document does not match the actual implementation — the Agent TCP actually uses a **16-byte header + JSON body** (see protocols.md ②). The `protobuf/` directory that was reserved for this was never wired into the link and was removed on 2026-09-22.

## Protocol Evolution Notes

> **Deprecated**: The legacy JSON-RPC style remote control protocol (based on `wingman::RemoteControlServer` / `wingman::RemoteControlClient`, default port 9999) has been removed.
> The `wingman::RemoteControlServer` and `wingman::RemoteControlClient` classes have been deleted from the core library.
> The Runtime's `serve` command (passive listening mode PassiveMode) has also been removed.

## Current Architecture

The Runtime now runs as an **active Agent**, connecting to the Go orchestrator via outbound transport. The Runtime is not a passive HTTP/WebSocket/TCP control server.

### 1. Transport TCP (Agent to Orchestrator)

The Agent actively connects to the Go orchestrator via `wingman::runtime::RemoteClient` (based on the TCP transport layer); communication uses a **16-byte header + JSON body** (Header + Body), not the legacy JSON-RPC.

```
Agent (Runtime)  ──主动连接──>  Go Orchestrator (TCP Server)
                               ├── 任务下发
                               ├── 状态上报
                               ├── 心跳保活
                               └── 工作流编排
```

Key implementation files:
- `libs/agentcore/src/remote_client.cpp` - the remote client (`wingman::runtime::RemoteClient`, shared source for the desktop runtime and the Android agent), actively connects to the orchestrator
- `libs/transport/` - the network transport layer

### 2. Local IPC (Local GUI Control)

The Tauri/Svelte GUI (`apps/gui/`) calls the Rust backend via Tauri `invoke()`; the Rust backend controls the runtime via local IPC, with no network protocol involved.

```
Tauri GUI (Svelte)  ──invoke──>  Tauri Rust backend  ──local IPC──>  Runtime
                                                                        ├── 脚本管理
                                                                        ├── 触发器管理
                                                                        └── 核心能力调用
```

Default local IPC policy:

| Platform | Default IPC |
|------|----------|
| Windows | Named Pipe |
| macOS/Linux | Unix Domain Socket |
| All platforms | Local TCP only as an explicit debug fallback |

### 3. Web Dashboard (React/Umi)

The Dashboard (`orchestrator/dashboard/`) connects to the Go orchestrator via WebSocket, and the orchestrator forwards commands to each Agent.

```
Web Dashboard (React/Umi)  ──WebSocket──>  Go Orchestrator  ──Transport TCP──>  Agent
```

## Legacy Protocol Action Names (Historical Archive, Not Current Interfaces)

The following table is a historical archive of the legacy JSON-RPC protocol action names, for migration reference only. None of these action names exist in the current
link: the current server→agent commands are the `method` field of the Request frame
(`run_script`/`stop_script`/`get_status`/`list_windows`/
`screenshot.capture`/`system.shutdown`/`trigger.*`; see [protocols.md](./protocols.md) for the authoritative naming); runtime local capabilities go through local IPC RPC
(`screenshot.capture`, `script.*`, `trigger.*`, `macro.*`, etc.); pixel-level
capabilities are provided in the form of the script API (`wingman.screen.*` / `wingman.vision.*`), with no independent
RPC.

| Legacy action (deprecated) | Current counterpart |
|------|------|
| ping | No agent command with the same name. Keep-alive is handled on the Go orchestrator side: `GET /api/v1/health` (requires Bearer, returns `{"status":"ok"}`) and the **protocol-level** Ping/Pong of the Dashboard WS (30s interval, not JSON frames, see [protocols.md](protocols.md) §③) |
| get_version | No agent command with the same name, and no version query endpoint on the orchestrator side (`/api/v1/health` only returns status, `/api/v1/status` only aggregates scripts/windows/resources). Version information only exists in the auto-update service plane: `GET /api/v1/update/latest` (public read-only manifest, containing version/url/sha256) |
| capture_screen | `screenshot.capture` (same name for the server→agent command and the local IPC RPC) |
| get_pixel / find_color / find_image | Script API `wingman.screen.getPixel` / `wingman.vision.findColor` / `wingman.vision.findImage` |
| click / move / key / type_text | Script API `wingman.input.*` (click/move/key/type, etc.) |
| list_triggers | `trigger.list` |
| add_trigger | `trigger.add` |
| remove_trigger | `trigger.remove` |
| enable_trigger / disable_trigger | `trigger.toggle` |
| record_macro / stop_macro_recording | `macro.start` / `macro.stop` |
| play_macro | `macro.play` |

## Migration Guide

### Migrating from the Legacy RemoteControlServer

1. **Server mode removed** - The Runtime no longer supports the `serve` command (passive listening). If you need centralized control of multiple Agents, use the Go orchestrator (`orchestrator/server/`).

2. **Connection direction changed** - In the legacy model the Runtime passively waited for external connections; in the new model the Runtime actively connects to the orchestrator.

3. **Protocol changed** - The legacy JSON-RPC over raw TCP has been replaced by a 16-byte header + JSON over transport TCP. See [protocols.md](./protocols.md) for the message format. (The reserved `protobuf/` directory and `libs/proto` were removed on 2026-09-22 — that layer was never wired into the actual link.)

4. **Client library** - `wingman::RemoteControlClient` has been removed. External tools should indirectly control Agents through the Go orchestrator's REST/WebSocket API.

5. **GUI local access** - If you only need local control, use the Tauri GUI (`apps/gui/`) to control the runtime via local IPC, with no network communication at all.
