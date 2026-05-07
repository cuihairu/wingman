# Wingman 项目目录结构

## 顶层目录

```
wingman/
├── assets/           # 资源文件（图标、图片等）
├── build/            # CMake 构建临时目录（.gitignore）
├── client/           # C++ 客户端
├── cmake/            # CMake 模块（FindScoopLua.cmake）
├── dashboard/        # Web 前端（VitePress + Vue）
├── docs/             # 项目文档
├── examples/         # 示例和模板
│   ├── configs/      # 应用配置示例（config.json）
│   ├── lua_scripts/  # Lua 脚本示例
│   └── profiles/     # 游戏配置示例（profile.json）
├── libs/             # 共享库
│   ├── core/         # 核心功能（Screen/Input/Trigger/Window 等）
│   ├── debug/        # 调试器适配（EmmyLua 集成）
│   ├── lua/          # Lua 引擎 + 绑定
│   ├── proto/        # Protobuf 协议封装
│   └── transport/    # 传输层（TCP/WebSocket）
├── protobuf/         # Protobuf 协议定义（.proto 文件）
├── scripts/          # 构建脚本（.ps1, .bat, .sh）
├── server/           # Go 服务端
├── setup/            # Windows 发布脚本（InnoSetup）
├── tests/            # 测试代码
│   ├── cpp/          # C++ 单元测试
│   ├── integration/  # 集成测试
│   └── unit/         # 单元测试
└── tools/            # 开发工具
    └── inspector/    # 图形化检查工具（wingman-lab）
```

## 模块说明

### client/
C++ 客户端程序，支持三种运行模式：
- ActiveMode：主动连接 Server
- PassiveMode：被动监听端口
- StandaloneMode：单机脚本执行

### server/
Go 服务端程序，提供 HTTP API 和 WebSocket 控制。

### libs/
共享库目录，各模块独立编译：
- **core**：核心功能（屏幕捕获、输入模拟、窗口管理等）
- **debug**：EmmyLua 调试器适配层
- **lua**：Lua 引擎绑定和脚本管理
- **proto**：Protobuf 协议封装
- **transport**：TCP/WebSocket 传输层

### protobuf/
Protobuf 协议定义文件：
- `agent_api.proto`：Agent API 服务定义
- `common.proto`：公共类型定义
- `debug.proto`：调试协议定义

### examples/
各类示例和模板文件，供用户参考和复制。

### dashboard/
Web 前端，基于 VitePress + Vue 构建。

### tools/inspector/
图形化开发工具，用于快速验证功能（截图、像素检测、图像匹配等）。

## 构建说明

使用 vcpkg 管理依赖：
```bash
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

详细构建步骤请参考 [README](../README.md#快速开始)。

## 测试

### 测试覆盖
- C++ 单元测试：`tests/cpp/` (30+ 测试文件)
- Lua 单元测试：`tests/unit/` (busted 框架)
- 集成测试：`tests/integration/`

启用测试构建：
```bash
cmake -DWINGMAN_BUILD_TESTS=ON ...
```

### 测试覆盖
- C++ 单元测试：`tests/cpp/`
- Lua 单元测试：`tests/unit/` (busted 框架)
- 集成测试：`tests/integration/`

启用测试构建：
```bash
cmake -DWINGMAN_BUILD_TESTS=ON ...
```
