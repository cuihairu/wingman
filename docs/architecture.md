# Wingman 项目架构

## 目录结构

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── runtime/                  ← 主运行时（主动 Agent）
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── agent.cpp         ← Agent 主逻辑（主动连接编排器）
│   │   │   ├── remote_client.cpp ← 远程客户端（连接编排器）
│   │   │   ├── remote_client.cpp ← 远程客户端（transport TCP）
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
│   ├── inspector/                ← Tauri 检查工具
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
│                    apps/                                 │
│  ┌──────────────┐  ┌──────────┐  ┌──────────────┐     │
│  │   runtime    │  │   gui    │  │  inspector   │     │
│  │ (主动 Agent) │  │(Tauri GUI)│  │  (检查工具)   │     │
│  └──────┬───────┘  └────┬─────┘  └──────────────┘     │
└─────────┼───────────────┼──────────────────────────────┘
          │               │
┌─────────▼───────────────────────────────────────────────┐
│              lib/wingman/ (核心库)                        │
│  屏幕捕获、输入模拟、触发器、视觉识别、行为树、OCR...     │
│              (GUI 通过 Tauri IPC 直接调用核心库)         │
└─────────┬───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│              libs/ (辅助库)                              │
│  ┌──────────┐  ┌──────┐  ┌──────┐                      │
│  │transport │  │ lua  │  │ proto│                      │
│  └──────────┘  └──────┘  └──────┘                      │
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

## 运行模式

Runtime 作为主动 Agent 运行，通过 outbound 连接到编排器（Go orchestrator）或单机运行。

| 模式 | 说明 | Transport |
|------|------|-----------|
| Agent 模式 | 主动连接到编排器服务器 | TcpClient (transport) |
| StandaloneMode | 单机模式，无网络 | - |

> **注意**: 旧的 PassiveMode（被动监听模式）和 `serve` 命令已被移除。Runtime 不再作为被动服务器运行。
> 远程控制现在通过 `wingman::runtime::RemoteClient`（基于 transport 的 TCP）实现 Agent 到编排器的通信，
> 或通过 Tauri IPC 由 GUI 直接调用 C++ API。

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`

## 参考项目

- [moderncpp-project-template](https://github.com/madduci/moderncpp-project-template) - 应用与库分离结构
- [Botcraft](https://github.com/adepierre/Botcraft) - 游戏机器人库设计
