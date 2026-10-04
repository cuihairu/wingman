# Wingman 项目目录结构

## 顶层目录

```
wingman/
├── apps/                    # 应用程序
│   ├── runtime/             # CLI 运行时 (C++)
│   ├── gui/                 # Tauri/Svelte 桌面 GUI
│   └── android/             # Android 端侧 Agent (Kotlin)
├── lib/                     # 核心库
│   └── wingman/             # 核心功能库
├── libs/                    # 引擎与链路库
│   ├── lua/                 # Lua 引擎绑定
│   ├── python/              # Python 引擎绑定
│   ├── transport/           # TCP/UDP 传输层
│   ├── agentcore/           # 远程 agent 公共件（注册/心跳/事件缓冲）
│   └── androidagent/        # Android 链路共享件（C++，供 android app 桥接）
├── orchestrator/            # 编排层
│   ├── dashboard/           # Web 控制面板 (React/Umi)
│   └── server/              # Go 服务端
├── assets/                  # 资源文件
├── build-scripts/           # 构建脚本
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
- **StandaloneMode**：单机模式，本地 IPC 供 GUI 控制

> **注意**: 旧的 PassiveMode（被动监听）和 `serve` 命令已被移除。Runtime 不再作为被动服务器。

#### gui/
Tauri/Svelte 桌面 GUI 应用，通过 Tauri Rust backend 使用本地 IPC 控制 runtime。

#### android/
Android 端侧 Agent（Kotlin，实验性）。无障碍服务注入 + MediaProjection 采集，出站 TCP 连 Go 中控，设计见 docs/android-agent-design.md。

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
- **hotkey** - 全局热键监听
- **file** - 文件 IO

脚本模块注册表（`script/modules/`，42 个 ModuleDescriptor）也在此库。

### libs/

引擎与链路库，各模块独立编译：

| 模块 | 说明 |
|------|------|
| **lua** | Lua 引擎绑定（sol2） |
| **python** | Python 引擎绑定（pybind11，默认关闭，`WINGMAN_ENABLE_PYTHON=ON` 启用） |
| **transport** | TCP/UDP 传输层（长度前缀帧 + JSON 体） |
| **agentcore** | 远程 agent 公共件：RemoteClient（注册/心跳/重连/断连缓存）、EventBuffer |
| **androidagent** | Android 链路共享件（C++，Kotlin 壳经 JNI 桥接复用桌面协议栈） |

### orchestrator/

编排层，提供 Web 控制和服务端功能：

#### dashboard/
Web 控制面板（基于 React/Umi），用于监控和管理 Agent

#### server/
Go 服务端程序，提供远程控制和 API 服务

## 调用链

```
Lua/Python 脚本 (.lua / .py)
    ↓
ScriptManager (语言无关) → IScriptEngine
    ↓
libs/lua/ 或 libs/python/ (引擎绑定)
    ↓
lib/wingman/ (42 个脚本模块 → screen, input, trigger...)
    ↓
apps/runtime/ (应用：CLI + outbound agent + local IPC)
    ↓ (outbound TCP)
orchestrator/server/ (Go 编排服务)
```

## 运行模式

Runtime 作为主动 Agent 运行，通过 outbound 连接到编排器。

| 模式 | 说明 | Transport |
|------|------|-----------|
| Agent 模式 | 主动连接到编排器服务器 | TcpClient (transport) |
| StandaloneMode | 单机模式，本地 IPC | Named Pipe (Windows) / UDS (macOS/Linux) |

> **注意**: PassiveMode 已被移除，Runtime 不再作为被动服务器。

## 设计原则

1. **核心库独立** - `lib/wingman/` 不依赖 `apps/`，可单独复用
2. **就近测试** - 每个模块都有自己的 `tests/` 目录
3. **职责清晰** - apps（应用）、lib（核心库）、libs（引擎与链路库）分离
4. **命名空间对应** - `include/wingman/xxx.hpp` → `namespace wingman::xxx`

## 构建说明

使用 vcpkg manifest 管理依赖（`vcpkg.json`）：

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

详细构建步骤请参考 [BUILD.md](../BUILD.md)。

## 测试

### 测试覆盖

- C++ 单元测试：`lib/wingman/tests/`（core_tests，2000+ 例）
- Runtime 测试：`apps/runtime/tests/`（runtime_tests）
- Lua 单元测试：`libs/lua/tests/`
- Python 单元测试：`libs/python/tests/`
- 传输层测试：`libs/transport/tests/`
- agentcore 测试：`libs/agentcore/tests/`
- GUI 前端测试：`apps/gui/tests/`（vitest）
- Dashboard 测试：`orchestrator/dashboard/`（jest）
- Go 服务端测试：`orchestrator/server/`（go test -race，三平台 CI）

启用测试构建（vcpkg toolchain + tests feature）：

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=tests -DWINGMAN_BUILD_TESTS=ON
```
