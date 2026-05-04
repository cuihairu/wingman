# 项目目录结构

```
wingman/
├── .github/
│   └── workflows/           # CI/CD 配置
│       └── build.yml         # 构建工作流
│
├── docs/                     # VitePress 文档
│   ├── .vitepress/
│   │   └── config.mts        # VitePress 配置
│   ├── guide/                # 指南文档
│   │   ├── introduction.md   # 项目简介
│   │   ├── getting-started.md # 快速开始
│   │   ├── architecture.md   # 架构设计
│   │   ├── screen.md         # 屏幕操作
│   │   ├── input.md          # 输入模拟
│   │   ├── window.md         # 窗口管理
│   │   ├── process.md        # 进程管理
│   │   ├── macro.md          # 宏录制
│   │   ├── trigger.md        # 触发器
│   │   ├── human.md          # 人性化模拟
│   │   ├── remote.md         # 远程控制
│   │   ├── debugging.md      # 调试指南
│   │   └── performance.md    # 性能优化
│   ├── api/                   # API 参考
│   │   ├── screen.md          # wingman.screen
│   │   ├── input.md           # wingman.input
│   │   ├── window.md          # wingman.window
│   │   ├── process.md         # wingman.process
│   │   ├── human.md           # wingman.human
│   │   ├── macro.md           # wingman.macro
│   │   ├── trigger.md         # wingman.trigger
│   │   ├── util.md            # wingman.util
│   │   └── debug.md           # wingman.debug
│   ├── examples/             # 示例脚本
│   │   ├── hello-world.md
│   │   ├── pixel-detection.md
│   │   ├── image-matching.md
│   │   ├── auto-loop.md
│   │   └── macro-record.md
│   ├── index.md              # 首页
│   └── package.json          # 文档依赖
│
├── server/                   # 网络服务层
│   ├── include/wingman/
│   │   └── server/
│   │       ├── server.hpp
│   │       ├── client.hpp
│   │       └── protocol.hpp
│   └── src/
│       ├── server.cpp
│       ├── client.cpp
│       └── protocol.cpp
│
├── core/                     # C++ 核心引擎
│   ├── include/wingman/
│   │   ├── screen.hpp
│   │   ├── input.hpp
│   │   ├── window.hpp
│   │   ├── process.hpp
│   │   ├── human.hpp
│   │   ├── macro.hpp
│   │   ├── trigger.hpp
│   │   └── utils.hpp
│   └── src/
│       ├── main.cpp
│       ├── screen.cpp
│       ├── input.cpp
│       ├── window.cpp
│       ├── process.cpp
│       ├── human.cpp
│       ├── macro.cpp
│       ├── trigger.cpp
│       └── utils.cpp
│
├── debugger/                 # 调试器
│   ├── include/wingman/
│   │   └── debugger/
│   │       ├── debugger.hpp
│   │       ├── breakpoint.hpp
│   │       └── protocol.hpp
│   └── src/
│       ├── debugger.cpp
│       ├── breakpoint.cpp
│       └── protocol.cpp
│
├── bindings/                 # Lua 绑定
│   └── sol2/                 # sol2 header-only 库
│
├── scripts/                  # Lua 脚本
│   ├── examples/            # 示例脚本
│   │   ├── hello.lua
│   │   ├── pixel_detection.lua
│   │   ├── image_matching.lua
│   │   └── auto_loop.lua
│   └── api/                 # API 脚本
│
├── tools/                    # 开发工具
│   └── debugger_client/     # 调试器客户端
│
├── tests/                    # 测试
│   ├── unit/                # 单元测试
│   └── integration/         # 集成测试
│
├── CMakeLists.txt           # CMake 配置
├── vcpkg.json              # vcpkg 依赖
├── LICENSE                  # MIT 许可证
└── README.md               # 项目说明
```

## 目录说明

### docs/ - 文档中心

使用 VitePress 构建的文档网站，包含：
- 指南文档
- API 参考
- 示例脚本

### server/ - 网络服务层

实现 TCP Server/Client，支持远程控制：
- **Server 模式**：被动监听，等待连接
- **Client 模式**：主动连接 Nebula

### core/ - 核心引擎

C++ 实现的核心功能：
- 屏幕操作
- 输入模拟
- 窗口管理
- 进程管理
- 人性化模拟
- 宏录制
- 触发器系统

### debugger/ - 调试器

实现 Lua 脚本调试功能：
- 断点管理
- 单步执行
- 变量查看
- 调用栈
- DAP 协议支持

### bindings/ - Lua 绑定

使用 sol2 将 C++ API 绑定到 Lua。

### scripts/ - 脚本

Lua 脚本示例和 API 封装。
