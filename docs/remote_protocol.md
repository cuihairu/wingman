# Wingman 远程控制协议文档

> ⚠️ **历史文档**：本文记录旧 JSON-RPC 协议的迁移说明。当前权威协议规范见 [protocols.md](./protocols.md)。
>
> 注意：本文早前描述的「Protobuf 序列化」与实际实现不符——Agent TCP 实际使用 **16 字节头 + JSON 体**（见 protocols.md ②）。`protobuf/` 目录为预留，未用于当前 agent 链路。

## 协议演进说明

> **已废弃**: 旧的 JSON-RPC 风格远程控制协议（基于 `wingman::RemoteControlServer` / `wingman::RemoteControlClient`，默认端口 9999）已被移除。
> `wingman::RemoteControlServer` 和 `wingman::RemoteControlClient` 类已从核心库中删除。
> Runtime 的 `serve` 命令（被动监听模式 PassiveMode）也已移除。

## 当前架构

Runtime 现在作为**主动 Agent** 运行，通过 outbound transport 连接 Go 编排器。Runtime 不作为被动 HTTP/WebSocket/TCP 控制服务器。

### 1. Transport TCP（Agent 到编排器）

Agent 通过 `wingman::runtime::RemoteClient`（基于 transport 层的 TCP）主动连接到 Go 编排器（orchestrator），通信采用 **Protobuf** 序列化格式，而非旧的 JSON-RPC。

```
Agent (Runtime)  ──主动连接──>  Go Orchestrator (TCP Server)
                               ├── 任务下发
                               ├── 状态上报
                               ├── 心跳保活
                               └── 工作流编排
```

关键实现文件：
- `apps/runtime/src/remote_client.cpp` - 远程客户端，主动连接编排器
- `libs/transport/` - 网络传输层
- `libs/proto/` - Protobuf 协议封装

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

## API 端点（由 Go 编排器提供服务）

以下 API 端点概念仍然存在，但现在由 Go 编排器（`orchestrator/server/`）提供服务，而非 C++ runtime 直接暴露。

### 系统操作

| 动作 | 说明 |
|------|------|
| ping | 健康检查 |
| get_version | 获取版本信息 |

### 屏幕操作

| 动作 | 说明 |
|------|------|
| capture_screen | 截取屏幕区域 |
| get_pixel | 获取指定坐标像素颜色 |
| find_color | 在指定区域查找颜色 |
| find_image | 在屏幕上查找图像 |

### 输入模拟

| 动作 | 说明 |
|------|------|
| click | 模拟鼠标点击 |
| move | 移动鼠标 |
| key | 模拟键盘按键 |
| type_text | 输入文本 |

### 触发器管理

| 动作 | 说明 |
|------|------|
| list_triggers | 列出所有触发器 |
| add_trigger | 添加新触发器 |
| remove_trigger | 删除触发器 |
| enable_trigger | 启用触发器 |
| disable_trigger | 禁用触发器 |

### 宏操作

| 动作 | 说明 |
|------|------|
| record_macro | 开始录制宏 |
| stop_macro_recording | 停止宏录制 |
| play_macro | 回放宏 |

## 迁移指南

### 从旧 RemoteControlServer 迁移

1. **服务端模式已移除** - Runtime 不再支持 `serve` 命令（被动监听）。如需集中控制多个 Agent，请使用 Go 编排器（`orchestrator/server/`）。

2. **连接方向变更** - 旧模式中 Runtime 被动等待外部连接；新模式中 Runtime 主动连接到编排器。

3. **协议变更** - 旧的 JSON-RPC over raw TCP 已替换为 Protobuf over transport TCP。消息格式定义在 `protobuf/` 目录。

4. **客户端库** - `wingman::RemoteControlClient` 已移除。外部工具应通过 Go 编排器的 REST/WebSocket API 间接控制 Agent。

5. **GUI 本地访问** - 如果只需要本地控制，使用 Tauri GUI（`apps/gui/`）通过本地 IPC 控制 runtime，无需任何网络通信。
