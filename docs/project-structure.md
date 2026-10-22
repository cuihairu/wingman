> [中文](project-structure.md) | **English**

# Wingman Project Directory Structure

## Top-level Directories

```
wingman/
├── apps/                    # 应用程序
│   ├── agent/               # CLI 运行时 (C++)
│   ├── gui/                 # Tauri/Svelte 桌面 GUI
│   └── android/             # Android 端侧 Agent (Kotlin)
├── lib/                     # 核心库
│   └── wingman/             # 核心功能库
├── libs/                    # 引擎与链路库
│   ├── lua/                 # Lua 引擎绑定
│   ├── python/              # Python 引擎绑定
│   ├── transport/           # TCP/UDP 传输层
│   ├── agentcore/           # 远程 agent 公共件（注册/心跳/事件缓冲）
│   ├── androidagent/        # Android 链路共享件（C++，供 android app 桥接）
│   └── crash/               # 崩溃采集封装（Crashpad client 接入）
├── orchestrator/            # 编排层
│   ├── dashboard/           # Web 控制面板 (React/Umi)
│   └── server/              # Go 服务端
├── assets/                  # 资源文件
├── build-scripts/           # 构建脚本
├── cmake/                   # CMake 模块（crashpad.cmake）
├── config/                  # 配置文件
├── deployments/             # 部署编排（docker、guacd）
├── docs/                    # 项目文档
├── examples/                # 示例和模板
├── scripts/                 # 工具脚本（CI 守卫、安装、真机核验）
├── third_party/             # 钉版 git 子模块（crashpad 系，见 docs/dependencies.md）
└── vcpkg-ports/             # 本地 vcpkg ports（预留，当前为空）
```

## Module Description

### apps/

Application directory, containing all executables:

#### agent/
C++ CLI runtime, running as an active agent:
- **Agent mode**: actively connects to the orchestrator (Go orchestrator), receives tasks over transport TCP and reports status
- **StandaloneMode**: standalone mode, local IPC for GUI control

> **Note**: the old PassiveMode (passive listening) and the `serve` command have been removed. The runtime no longer acts as a passive server.

#### gui/
Tauri/Svelte desktop GUI application, controlling the runtime through local IPC via the Tauri Rust backend.

#### android/
Android on-device agent (Kotlin, experimental). Accessibility-service injection + MediaProjection capture, outbound TCP to the Go orchestrator; design in docs/android-agent-design.md.

### lib/wingman/

Core library, containing the main feature modules:

- **screen** - screen capture, pixel operations
- **input** - input simulation (mouse, keyboard)
- **window** - window management
- **process** - process management
- **trigger** - trigger system
- **vision** - visual recognition
- **behavior_tree** - behavior tree
- **ocr** - OCR text recognition
- **hotkey** - global hotkey listening
- **file** - file IO

The script module registry (`script/modules/`, 42 ModuleDescriptors) also lives in this library.

### libs/

Engine and link libraries, each module compiled independently:

| Module | Description |
|------|------|
| **lua** | Lua engine binding (sol2) |
| **python** | Python engine binding (pybind11, off by default, enabled with `WINGMAN_ENABLE_PYTHON=ON`) |
| **transport** | TCP transport layer (length-prefixed frames + JSON body; the script layer's UDP goes straight to asio, not through this library) |
| **agentcore** | Remote agent shared pieces: RemoteClient (registration/heartbeat/reconnect/disconnect buffering), EventBuffer |
| **androidagent** | Android link shared pieces (C++, reused by the Kotlin shell through JNI to share the desktop protocol stack) |
| **crash** | Crash collection wrapper (static library `wingman_crash`, links `crashpad_client`; enabled by `WINGMAN_ENABLE_CRASHPAD`, default on for Linux) |

### orchestrator/

Orchestration layer, providing the web control panel and server-side functionality:

#### dashboard/
Web control panel (based on React/Umi), used to monitor and manage agents

#### server/
Go server program, providing remote control and the API service

## Call Chain

```
Lua/Python 脚本 (.lua / .py)
    ↓
ScriptManager (语言无关) → IScriptEngine
    ↓
libs/lua/ 或 libs/python/ (引擎绑定)
    ↓
lib/wingman/ (42 个脚本模块 → screen, input, smarttrigger...)
    ↓
apps/agent/ (应用：CLI + outbound agent + local IPC)
    ↓ (outbound TCP)
orchestrator/server/ (Go 编排服务)
```

## Run Modes

The runtime runs as an active agent, connecting to the orchestrator outbound.

| Mode | Description | Transport |
|------|------|-----------|
| Agent mode | Actively connects to the orchestrator server | TcpClient (transport) |
| StandaloneMode | Standalone mode, local IPC | Named Pipe (Windows) / UDS (macOS/Linux) |

> **Note**: PassiveMode has been removed; the runtime no longer acts as a passive server.

## Design Principles

1. **Independent core library** - `lib/wingman/` does not depend on `apps/` and can be reused on its own
2. **Tests next to code** - every module has its own `tests/` directory
3. **Clear responsibilities** - apps (applications), lib (core library) and libs (engine and link libraries) are separated
4. **Namespaces** - subdirectory headers are nested by directory: `include/wingman/<dir>/xxx.hpp` → `namespace wingman::<dir>` (capture / core / ipc / platform / rpc / vision the same way); top-level headers use a flat `namespace wingman` (31 of 33, with `crypt.hpp` / `core.hpp` as exceptions using `wingman::crypt` / `wingman::core`)

## Build Notes

Dependencies are managed with the vcpkg manifest (`vcpkg.json`):

```bash
build-scripts\build-runtime-msvc-ninja.bat
```

For the detailed build steps see [BUILD.md](../BUILD.md).

## Tests

### Test coverage

- C++ unit tests: `lib/wingman/tests/` (core_tests, 2000+ cases)
- Runtime tests: `apps/agent/tests/` (agent_tests)
- Lua unit tests: `libs/lua/tests/`
- Python unit tests: `libs/python/tests/`
- Transport layer tests: `libs/transport/tests/`
- agentcore tests: `libs/agentcore/tests/`
- GUI frontend tests: `apps/gui/tests/` (vitest)
- Dashboard tests: `orchestrator/dashboard/` (jest)
- Go server tests: `orchestrator/server/` (go test -race, three-platform CI)

Enabling a test build (vcpkg toolchain + tests feature):

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static \
  -DVCPKG_MANIFEST_FEATURES=tests -DWINGMAN_BUILD_TESTS=ON
```
