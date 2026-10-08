# 项目目录结构

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
│   ├── runtime/              # C++ 运行时（wingman-runtime：主动 Agent + 本地 IPC）
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

各库的测试随库存放：单元测试在 `lib/wingman/tests/`，脚本引擎测试（含集成测试）在 `libs/lua/tests/`、`libs/python/tests/`，无顶层 tests/ 目录。

## 目录说明

### docs/ - 文档中心

使用 VitePress 构建的文档网站，包含：
- 指南文档（guide/ 九篇）
- API 参考（api/）
- 示例讲解（examples/）

### orchestrator/ - 远程编排层

Go server 是远程中控入口。Runtime 作为 agent 主动 outbound 连接 Go server，Dashboard 只连接 Go server。

### local IPC - 本地单机控制

Tauri GUI 通过 Tauri Rust backend 使用本地 IPC 控制 runtime。Runtime 不提供 WebSocket/HTTP server 作为本地 UI 控制面。

### lib/wingman - 核心引擎

C++ 实现的核心功能：
- 屏幕操作
- 输入模拟
- 窗口管理
- 进程管理
- 人性化模拟
- 宏录制
- 触发器系统

平台相关实现按后端拆分在 `src/platform/{win,linux,mac}/` 下（如 `sendinput_input.cpp`、`xtest_input.cpp`、`cgevent_input.cpp`）。

### 调试器

Lua 脚本调试基于 EmmyLua attach（runtime 监听 `:9966`），脚本内可用 `debugger` 模块的四个函数：`start` / `stop` / `breakpoint` / `breakHere`。DAP 协议适配为规划项，尚未实现。

### libs/ - 脚本绑定与辅助库

- `libs/lua`：使用 sol2 将 C++ API 绑定到 Lua（统一挂到 `require("wingman")` 命名空间）。
- `libs/python`：pybind11 绑定，与 Lua 共享同一套模块注册。
- `libs/transport`：TCP 帧协议与会话管理。
- `libs/agentcore`：RemoteClient（Agent 侧远程连接核心）。
- `libs/androidagent`：Android Agent 实现。

### examples/lua_scripts/ - 脚本示例

Lua 脚本示例（`hello.lua`、`pixel_detection.lua`、`image_matching.lua`、`auto_loop.lua` 等），以 `wingman-runtime.exe script examples/lua_scripts/xxx.lua` 运行。

### scripts/ / build-scripts/ - 工程脚本

`build-scripts/` 是构建与打包脚本（Windows bat/ps1、CI 子脚本）；`scripts/` 是平台边界校验、部署验证等工程脚本。两者均不含 Lua 示例。
