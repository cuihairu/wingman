# Wingman 项目目录结构

## 顶层目录

```
wingman/
├── apps/                    # 应用程序
│   ├── runtime/             # CLI 运行时 (C++)
│   ├── gui/                 # Tauri/Svelte GUI
│   ├── inspector/           # Tauri 检查工具
│   └── client/              # 客户端库
├── lib/                     # 核心库
│   └── wingman/             # 核心功能库
├── libs/                    # 辅助库
│   ├── clasp/               # 命令行库
│   ├── debug/               # EmmyLua 调试器适配
│   ├── lua/                 # Lua 引擎绑定
│   ├── python/              # Python 引擎绑定
│   ├── proto/               # Protobuf 协议封装
│   └── transport/           # TCP/WebSocket 传输层
├── orchestrator/            # 编排层
│   ├── dashboard/           # Web 控制面板 (React/Umi)
│   └── server/             # Go 服务端
├── protobuf/                # Protobuf 协议定义
├── assets/                  # 资源文件
├── build-scripts/          # 构建脚本
├── cmake/                   # CMake 模块
├── config/                  # 配置文件
├── docs/                    # 项目文档
├── examples/                # 示例和模板
└── vcpkg-ports/             # 本地 vcpkg ports
```

## 模块说明

### apps/

应用程序目录，包含所有可执行程序：

#### runtime/
C++ CLI 运行时，作为主动 Agent 运行：
- **Agent 模式**：主动连接到编排器（Go orchestrator），通过 transport TCP 接收任务并上报状态
- **StandaloneMode**：单机模式，无网络

> **注意**: 旧的 PassiveMode（被动监听）和 `serve` 命令已被移除。Runtime 不再作为被动服务器。

#### gui/
Tauri/Svelte 桌面 GUI 应用，通过 Tauri IPC 直接调用 C++ 核心库 API。

#### inspector/
Tauri 2.0 开发检查工具，用于快速验证功能（截图、像素检测、图像匹配等）

### lib/wingman/

核心库，包含主要功能模块：

- **screen** - 屏幕捕获、像素操作
- **input** - 输入模拟（鼠标、键盘）
- **window** - 窗口管理
- **process** - 进程管理
- **trigger** - 触发器系统
- **vision** - 视觉识别
- **behavior_tree** - 行为树
- **ocr** - OCR 文字识别

### libs/

辅助库，各模块独立编译：

| 模块 | 说明 |
|------|------|
| **clasp** | 命令行库（通过 overlay ports 管理） |
| **debug** | EmmyLua 调试器适配层 |
| **lua** | Lua 引擎绑定（sol2） |
| **python** | Python 引擎绑定（pybind11） |
| **proto** | Protobuf 协议封装 |
| **transport** | TCP/WebSocket 传输层 |

### orchestrator/

编排层，提供 Web 控制和服务端功能：

#### dashboard/
Web 控制面板（基于 React/Umi），用于监控和管理 Agent

#### server/
Go 服务端程序，提供远程控制和 API 服务

### protobuf/

Protobuf 协议定义文件：
- `agent_api.proto` - Agent API 服务定义
- `common.proto` - 公共类型定义
- `debug.proto` - 调试协议定义

## 调用链

```
Lua/Python 脚本 (.lua / .py)
    ↓
libs/lua/ 或 libs/python/ (引擎绑定)
    ↓
lib/wingman/ (核心功能：screen, input, trigger...)
    ↓
apps/runtime/ (应用：CLI + 运行模式)
    ↓
orchestrator/server/ (Go 编排服务)
```

## 运行模式

Runtime 作为主动 Agent 运行，通过 outbound 连接到编排器。

| 模式 | 说明 | Transport |
|------|------|-----------|
| Agent 模式 | 主动连接到编排器服务器 | TcpClient (transport) |
| StandaloneMode | 单机模式，无网络 | - |

> **注意**: PassiveMode 已被移除，Runtime 不再作为被动服务器。

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（辅助库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`

## 构建说明

使用 vcpkg 管理依赖：

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

详细构建步骤请参考 [BUILD.md](../BUILD.md)。

## 测试

### 测试覆盖

- C++ 单元测试：`lib/wingman/tests/`
- Lua 单元测试：`libs/lua/tests/`
- Python 单元测试：`libs/python/tests/`
- 集成测试：`libs/transport/tests/`

启用测试构建：

```bash
cmake -B build -DWINGMAN_BUILD_TESTS=ON ...
```
