# Wingman 项目架构

> 架构硬约束见 `docs/architecture-decisions.md`。修改 runtime、GUI、orchestrator 或 transport 前必须先阅读该文档。

## 目录结构

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── runtime/                  ← 主运行时（主动 Agent + 本地 IPC 服务端）
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── agent.cpp         ← Agent 主逻辑（主动连接编排器）
│   │   │   ├── remote_client.cpp ← 远程客户端（主动 outbound 连接编排器）
│   │   │   ├── standalone_mode.cpp ← 单机模式
│   │   │   └── commands/         ← CLI 子命令
│   │   ├── include/wingman/runtime/
│   │   ├── tests/                ← 应用测试
│   │   └── CMakeLists.txt
│   │
│   ├── gui/                      ← Tauri/Svelte GUI
│   │   ├── src-tauri/
│   │   ├── src/
│   │   └── package.json
│   │
│   └── client/                   ← 客户端库
│
├── lib/wingman/                  ← 核心库
│   ├── include/wingman/
│   │   ├── screen.hpp            ← 屏幕捕获
│   │   ├── input.hpp             ← 输入模拟
│   │   ├── trigger.hpp           ← 触发器
│   │   ├── vision.hpp            ← 视觉识别
│   │   ├── behavior_tree.hpp     ← 行为树
│   │   ├── ocr.hpp               ← OCR
│   │   └── ...
│   ├── src/
│   │   ├── screen.cpp
│   │   ├── input.cpp
│   │   └── ...
│   ├── tests/                    ← 核心库测试
│   └── CMakeLists.txt
│
├── libs/                         ← 内部辅助库
│   ├── transport/                ← 网络传输（TCP/WebSocket）
│   │   ├── include/wingman/transport/
│   │   │   ├── transport.hpp     ← 传输抽象
│   │   │   ├── transport_client.hpp
│   │   │   ├── transport_server.hpp
│   │   │   ├── session/          ← 会话层
│   │   │   └── channel/          ← 消息通道
│   │   └── src/
│   │
│   ├── lua/                      ← Lua 绑定（桥接 Lua → 核心库）
│   │   ├── include/wingman/lua/
│   │   └── src/
│   │
│   └── proto/                    ← Protobuf
│
├── examples/                     ← 示例代码
├── scripts/                      ← 脚本
├── docs/                         ← 文档
└── CMakeLists.txt                ← 根 CMake（聚合构建）
```

## 架构分层

```
┌─────────────────────────────────────────────────────────┐
│                 Remote orchestration                     │
│ dashboard/browser ──HTTP/WS──► Go orchestrator            │
│                                      ▲                    │
│                                      │ outbound transport │
│                                      │                    │
│                              runtime agent                │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                  Local standalone UI                     │
│ Tauri UI ──invoke──► Tauri Rust backend ──IPC──► runtime  │
│                                                   │       │
│                                                   ▼       │
│                                        lib/wingman core   │
└─────────────────────────────────────────────────────────┘
```

## 调用链

```
Lua 脚本
    ↓
libs/lua/ (Lua 绑定)
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/runtime/ (应用：CLI + 运行模式)
```

## 控制面

Runtime 远程模式和本地单机 UI 是两条不同控制路径。

| 场景 | 控制路径 | 约束 |
|------|----------|------|
| 远程编排 | runtime agent 主动 outbound 连接 Go orchestrator | Go server 是唯一远程中控入口 |
| 本地单机 UI | Tauri UI -> Rust backend -> local IPC -> runtime | 不使用 runtime HTTP/WebSocket server |

> **禁止**: Runtime 不得引入 HTTP/WebSocket server 作为本地 UI 或远程控制面。WebSocket 只允许用于 dashboard/browser 与 Go server 通信。

## 本地 IPC 策略

| 平台 | 默认 IPC | 备注 |
|------|----------|------|
| Windows | Named Pipe | Windows Unix Domain Socket 可探测支持，但不作为默认主路径 |
| macOS/Linux | Unix Domain Socket | 使用用户运行时目录下的 socket |
| 全平台 | Local TCP | 仅显式 debug fallback，默认关闭 |

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`
5. **控制面分离** - 远程编排走 Go server，本地 UI 走 IPC，runtime 不暴露 WebSocket/HTTP 控制面

## 参考项目

- [moderncpp-project-template](https://github.com/madduci/moderncpp-project-template) - 应用与库分离结构
- [Botcraft](https://github.com/adepierre/Botcraft) - 游戏机器人库设计
