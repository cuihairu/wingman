> [中文](../zh/guide/structure.md) | **English**

# Project Directory Structure

```
wingman/
├── .github/
│   └── workflows/           # CI/CD 配置
│       ├── ci.yml            # 主 CI 工作流
│       ├── build-agent.yml   # Agent 构建工作流
│       ├── build-package.yml # 打包工作流
│       ├── nightly.yml       # Nightly 构建
│       ├── release.yml       # 发布工作流
│       └── ...               # deploy-docs / deploy-server / verify-platforms 等
│
├── docs/                     # VitePress 文档
│   ├── .vitepress/
│   │   └── config.mts        # VitePress 配置
│   ├── guide/                # 指南文档（共九篇）
│   │   ├── introduction.md        # 项目简介
│   │   ├── getting-started.md     # 快速开始
│   │   ├── architecture.md        # 架构设计
│   │   ├── config.md              # 配置系统
│   │   ├── dashboard.md           # Dashboard 使用
│   │   ├── runtime-gui.md         # Runtime GUI 使用
│   │   ├── script-development.md  # 脚本开发
│   │   ├── debugging.md           # 调试指南
│   │   └── structure.md           # 目录结构（本页）
│   ├── api/                   # API 参考（wingman.* 各模块）
│   │   ├── screen.md / input.md / window.md / process.md
│   │   ├── macro.md / trigger.md / human.md / util.md
│   │   ├── vision.md / http.md / config.md / uia/ ...
│   ├── examples/             # 示例讲解
│   │   ├── hello-world.md
│   │   ├── pixel-detection.md
│   │   ├── image-matching.md
│   │   ├── auto-loop.md
│   │   ├── macro-record.md
│   │   └── ui-automation.md
│   ├── index.md              # 首页
│   └── package.json          # 文档依赖
│
├── apps/                     # 应用程序
│   ├── agent/                # C++ 运行时（wingman-agent：主动 Agent + 本地 IPC）
│   ├── gui/                  # Tauri/Svelte 桌面 GUI
│   │   ├── src/              # Svelte 前端
│   │   └── src-tauri/        # Rust 后端
│   └── android/              # Android Agent
│
├── lib/wingman/              # C++ 核心引擎
│   ├── include/wingman/      # 公共头文件
│   ├── src/                  # 实现（script/modules/ 为脚本绑定注册点）
│   └── tests/                # 单元测试
│
├── libs/                     # 辅助库
│   ├── lua/                  # Lua 绑定（sol2，LuaScriptEngine）
│   ├── python/               # Python 绑定（pybind11，含 typing 类型桩）
│   ├── transport/            # TCP 传输层（帧协议）
│   ├── agentcore/            # 远程客户端核心（RemoteClient）
│   └── androidagent/         # Android Agent 库
│
├── orchestrator/             # 远程编排层
│   ├── dashboard/            # Web 控制面板 (React/Umi)
│   └── server/               # Go 服务端
│
├── examples/                 # 示例
│   ├── lua_scripts/          # Lua 脚本示例
│   │   ├── hello.lua
│   │   ├── hello_world.lua
│   │   ├── pixel_detection.lua
│   │   ├── image_matching.lua
│   │   ├── auto_loop.lua
│   │   ├── macro_record.lua
│   │   └── ui_automation_example.lua
│   ├── configs/              # 示例配置
│   └── profiles/             # 示例 game profile
│
├── build-scripts/            # 构建/打包脚本（configure-msvc-ninja.bat 等）
├── scripts/                  # 校验/部署工程脚本（check_platform_boundary.sh 等）
│
├── CMakeLists.txt           # CMake 配置
├── vcpkg.json              # vcpkg 依赖清单
├── LICENSE                  # Apache-2.0 许可证
└── README.md               # 项目说明
```

Each library's tests live alongside that library: unit tests in `lib/wingman/tests/`, script engine tests (including integration tests) in `libs/lua/tests/` and `libs/python/tests/`; there is no top-level tests/ directory.

## Directory Notes

### docs/ - Documentation Hub

A documentation site built with VitePress, containing:
- Guide documentation (nine pages under guide/)
- API reference (api/)
- Example walkthroughs (examples/)

### orchestrator/ - Remote Orchestration Layer

The Go server is the remote control entry point. The runtime acts as an agent and actively connects outbound to the Go server; the Dashboard only connects to the Go server.

### local IPC - Local Single-Machine Control

The Tauri GUI controls the runtime over local IPC through the Tauri Rust backend. The runtime does not expose a WebSocket/HTTP server as a local UI control plane.

### lib/wingman - Core Engine

Core functionality implemented in C++:
- Screen operations
- Input simulation
- Window management
- Process management
- Humanized simulation
- Macro recording
- Trigger system

Platform-specific implementations are split by backend under `src/platform/{win,linux,mac}/` (e.g. `sendinput_input.cpp`, `xtest_input.cpp`, `cgevent_input.cpp`).

### Debugger

Lua script debugging is based on EmmyLua attach (the runtime listens on `:9966`). Inside a script, four functions of the `debugger` module are available: `start` / `stop` / `breakpoint` / `breakHere`. DAP protocol adaptation is a planned item and not yet implemented.

### libs/ - Script Bindings and Supporting Libraries

- `libs/lua`: uses sol2 to bind the C++ API into Lua (everything mounted under the `require("wingman")` namespace).
- `libs/python`: pybind11 bindings sharing the same module registration as Lua.
- `libs/transport`: TCP frame protocol and session management.
- `libs/agentcore`: RemoteClient (the agent-side remote connection core).
- `libs/androidagent`: the Android Agent implementation.

### examples/lua_scripts/ - Script Examples

Lua script examples (`hello.lua`, `pixel_detection.lua`, `image_matching.lua`, `auto_loop.lua`, etc.), run with `wingman-agent.exe script examples/lua_scripts/xxx.lua`.

### scripts/ / build-scripts/ - Engineering Scripts

`build-scripts/` holds build and packaging scripts (Windows bat/ps1, CI sub-scripts); `scripts/` holds engineering scripts such as platform boundary checks and deployment verification. Neither contains Lua examples.
