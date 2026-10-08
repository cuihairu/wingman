# Wingman 远程控制协议文档

> [**历史文档**：本文记录旧 JSON-RPC 协议的迁移说明。当前权威协议规范见 [protocols.md](./protocols.md)。]
>
> 注意：本文早前描述的「Protobuf 序列化」与实际实现不符——Agent TCP 实际使用 **16 字节头 + JSON 体**（见 protocols.md ②）。曾预留的 `protobuf/` 目录从未接入链路，已于 2026-09-22 移除。

## 协议演进说明

> **已废弃**: 旧的 JSON-RPC 风格远程控制协议（基于 `wingman::RemoteControlServer` / `wingman::RemoteControlClient`，默认端口 9999）已被移除。
> `wingman::RemoteControlServer` 和 `wingman::RemoteControlClient` 类已从核心库中删除。
> Runtime 的 `serve` 命令（被动监听模式 PassiveMode）也已移除。

## 当前架构

Runtime 现在作为**主动 Agent** 运行，通过 outbound transport 连接 Go 编排器。Runtime 不作为被动 HTTP/WebSocket/TCP 控制服务器。

### 1. Transport TCP（Agent 到编排器）

Agent 通过 `wingman::runtime::RemoteClient`（基于 transport 层的 TCP）主动连接到 Go 编排器（orchestrator），通信采用 **16 字节头 + JSON 体**（Header + Body），而非旧的 JSON-RPC。

```
Agent (Runtime)  ──主动连接──>  Go Orchestrator (TCP Server)
                               ├── 任务下发
                               ├── 状态上报
                               ├── 心跳保活
                               └── 工作流编排
```

关键实现文件：
- `libs/agentcore/src/remote_client.cpp` - 远程客户端（`wingman::runtime::RemoteClient`，桌面 runtime 与 Android agent 同源），主动连接编排器
- `libs/transport/` - 网络传输层

### 2. Local IPC（本地 GUI 控制）

Tauri/Svelte GUI（`apps/gui/`）通过 Tauri `invoke()` 调用 Rust backend，Rust backend 通过本地 IPC 控制 runtime，无需经过网络协议。

```
Tauri GUI (Svelte)  ──invoke──>  Tauri Rust backend  ──local IPC──>  Runtime
                                                                        ├── 脚本管理
                                                                        ├── 触发器管理
                                                                        └── 核心能力调用
```

本地 IPC 默认策略：

| 平台 | 默认 IPC |
|------|----------|
| Windows | Named Pipe |
| macOS/Linux | Unix Domain Socket |
| 全平台 | Local TCP 仅显式 debug fallback |

### 3. Web Dashboard（React/Umi）

Dashboard（`orchestrator/dashboard/`）通过 WebSocket 连接到 Go 编排器，由编排器转发命令到各 Agent。

```
Web Dashboard (React/Umi)  ──WebSocket──>  Go Orchestrator  ──Transport TCP──>  Agent
```

## 旧协议动作名（历史存档，非现行接口）

下表为旧 JSON-RPC 协议的动作名历史存档，仅供迁移对照。这些动作名在现行
链路中**均不存在**：现行 server→agent 命令是 Request 帧的 `method` 字段
（`run_script`/`stop_script`/`get_status`/`list_windows`/
`screenshot.capture`/`system.shutdown`/`trigger.*`，权威命名见
[protocols.md](./protocols.md)）；runtime 本地能力走本地 IPC RPC
（`screenshot.capture`、`script.*`、`trigger.*`、`macro.*` 等）；像素级
能力以脚本 API（`wingman.screen.*` / `wingman.vision.*`）形态提供，无独立
RPC。

| 旧动作（已废弃） | 现行对应 |
|------|------|
| ping / get_version | Dashboard REST/WS（Go 编排器侧健康与版本信息），无同名 agent 命令 |
| capture_screen | `screenshot.capture`（server→agent 命令与本地 IPC RPC 同名） |
| get_pixel / find_color / find_image | 脚本 API `wingman.screen.getPixel` / `wingman.vision.findColor` / `wingman.vision.findImage` |
| click / move / key / type_text | 脚本 API `wingman.input.*`（click/move/key/type 等） |
| list_triggers | `trigger.list` |
| add_trigger | `trigger.add` |
| remove_trigger | `trigger.remove` |
| enable_trigger / disable_trigger | `trigger.toggle` |
| record_macro / stop_macro_recording | `macro.start` / `macro.stop` |
| play_macro | `macro.play` |

## 迁移指南

### 从旧 RemoteControlServer 迁移

1. **服务端模式已移除** - Runtime 不再支持 `serve` 命令（被动监听）。如需集中控制多个 Agent，请使用 Go 编排器（`orchestrator/server/`）。

2. **连接方向变更** - 旧模式中 Runtime 被动等待外部连接；新模式中 Runtime 主动连接到编排器。

3. **协议变更** - 旧的 JSON-RPC over raw TCP 已替换为 16 字节头 + JSON over transport TCP。消息格式见 [protocols.md](./protocols.md)。（曾预留的 `protobuf/` 目录与 `libs/proto` 已于 2026-09-22 移除——该层从未接入实际链路。）

4. **客户端库** - `wingman::RemoteControlClient` 已移除。外部工具应通过 Go 编排器的 REST/WebSocket API 间接控制 Agent。

5. **GUI 本地访问** - 如果只需要本地控制，使用 Tauri GUI（`apps/gui/`）通过本地 IPC 控制 runtime，无需任何网络通信。
