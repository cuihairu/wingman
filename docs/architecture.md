# Wingman 项目架构

## 目录结构

```
wingman/
├── apps/                         ← 所有可执行程序
│   ├── client/                   ← 主应用
│   │   ├── src/
│   │   │   ├── main.cpp          ← 入口
│   │   │   ├── cli/              ← CLI 命令（一个文件一个命令）
│   │   │   │   ├── start.cpp
│   │   │   │   ├── stop.cpp
│   │   │   │   ├── status.cpp
│   │   │   │   └── ...
│   │   │   ├── modes/            ← 运行模式
│   │   │   │   ├── active_mode.cpp      ← 使用 TcpClient
│   │   │   │   ├── passive_mode.cpp     ← 使用 TcpServer
│   │   │   │   └── standalone_mode.cpp
│   │   │   └── gui/              ← GUI
│   │   │       ├── app.cpp
│   │   │       ├── panels/
│   │   │       └── widgets/
│   │   ├── tests/                ← 应用测试
│   │   └── CMakeLists.txt
│   │
│   └── inspector/                ← 检查工具
│       ├── src/
│       ├── include/
│       └── CMakeLists.txt
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
│  ┌──────────────┐           ┌──────────────┐           │
│  │   client     │           │  inspector   │           │
│  │  (应用入口)   │           │  (检查工具)   │           │
│  └──────┬───────┘           └──────────────┘           │
└─────────┼───────────────────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────────────────┐
│              lib/wingman/ (核心库)                        │
│  屏幕捕获、输入模拟、触发器、视觉识别、行为树、OCR...     │
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
apps/client/ (应用：CLI/GUI + 运行模式)
```

## 运行模式

| 模式 | 说明 | Transport |
|------|------|-----------|
| ActiveMode | 主动连接到服务器 | TcpClient |
| PassiveMode | 被动监听，等待连接 | TcpServer |
| StandaloneMode | 单机模式，无网络 | - |

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`

## 参考项目

- [moderncpp-project-template](https://github.com/madduci/moderncpp-project-template) - 应用与库分离结构
- [Botcraft](https://github.com/adepierre/Botcraft) - 游戏机器人库设计
