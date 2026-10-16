> **English** | [中文](DEVELOPMENT.md)

# Wingman 开发指南

## 开发者快速上手

### 先决条件

- Windows 10/11
- Visual Studio 2022（或 Build Tools）
- CMake 3.20+
- vcpkg
- Git

### 构建

```bash
# Clone and build
git clone https://github.com/cuihairu/wingman.git
cd wingman
build-scripts\build-runtime-msvc-ninja.bat
```

## VS Code 工作区

仓库包含已检入的 VS Code 配置，用于常见开发
工作流：

- `.vscode/settings.json` 配置 C++23、CMake、Lua 和 Python 存根（stub）。
- `.vscode/tasks.json` 暴露 runtime、GUI、docs 和 Go server 任务。
- `.vscode/launch.json` 包含 EmmyLua attach 与当前脚本 launch 配置。
- `examples/wingman-scripts.code-workspace` 是面向脚本作者的轻量工作区。

完整工作流见 [VS Code 开发环境](development-environment.md)。

## 项目结构

```
wingman/
├── apps/                    # Applications
│   ├── agent/               # CLI runtime
│   ├── gui/                 # Tauri GUI
│   └── android/             # Android 端侧 Agent (Kotlin，实验性)
├── lib/                     # Core library
│   └── wingman/             # Core functionality
├── libs/                    # Support libraries
│   ├── lua/                 # Lua engine binding
│   ├── python/              # Python engine binding (WINGMAN_ENABLE_PYTHON=ON)
│   ├── transport/           # TCP transport（长度前缀帧 + JSON 体）
│   ├── agentcore/           # 远程 agent 公共件（注册/心跳/事件缓冲）
│   ├── androidagent/        # Android 链路共享件（C++）
│   └── crash/               # 崩溃采集封装（Crashpad client）
└── orchestrator/            # Orchestration layer
    ├── dashboard/           # Web dashboard
    └── server/              # Go server
```

## Lua 开发工具

Wingman 内置 Lua 脚本支持。进行 Lua 开发时，可以选择性安装：

### LuaRocks（包管理器）

LuaRocks 是 Lua 模块的包管理器，类似于 Node.js 的 npm。

**安装：**
```cmd
examples\lua_scripts\install-luarocks.cmd
```

**用法：**
```cmd
REM Add to PATH (temporary)
set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%

REM Install a package
luarocks install lua-cjson

REM List installed packages
luarocks list
```

### Busted（测试框架）

Busted 是 Lua 的单元测试框架。

**安装：**
```cmd
REM First install LuaRocks, then:
examples\lua_scripts\install-busted.cmd
```

**运行测试：**
```cmd
examples\lua_scripts\run-lua-tests.cmd
```

**或手动运行：**
```cmd
set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%
busted tests -o utfTerminal
```

## 创建 Lua 脚本

### 基本示例

```lua
local wingman = require("wingman")

-- script.lua

-- Use built-in modules
-- Get screen size
local width = wingman.screen.getScreenWidth()
local height = wingman.screen.getScreenHeight()
print("Screen: " .. width .. "x" .. height)

-- Get pixel color
local color = wingman.screen.getPixel(100, 100)
print(string.format("Color at (100, 100): 0x%02X%02X%02X", color.r, color.g, color.b))

-- Move mouse
wingman.input.click(500, 300)
```

### 安装外部 Lua 包

```cmd
REM Using LuaRocks
luarocks install lua-cjson     -- JSON parser
luarocks install luafilesystem -- File operations
luarocks install penlight      -- Lua utility libraries
```

然后在脚本中：
```lua
local cjson = require("cjson")
local lfs = require("lfs")

-- Use the packages
local data = cjson.encode({name = "Wingman", version = "0.1.0"})
```

## 运行测试

### C++ 单元测试

```cmd
REM Build with tests enabled
cmake -S . -B build-tests ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_MANIFEST_FEATURES=tests ^
  -DWINGMAN_BUILD_TESTS=ON
cmake --build build-tests --config Debug --target core_tests agent_tests transport_tests

REM Run tests
ctest --test-dir build-tests -C Debug --output-on-failure
```

`WINGMAN_BUILD_TESTS=ON` 会自动启用标准 C++ 测试集；Lua 绑定层测试仍然按需通过 `BUILD_LUA_TESTS=ON` 单独打开。
建议测试使用单独的 `build-tests/` 目录，避免覆盖已有的开发构建目录。

### Linux X11 测试可选依赖

Linux 上 `platform_x11_test.cpp` 的用例依赖三个**运行期可选依赖**（不在
vcpkg 清单内，由系统包管理器提供）。缺失时对应用例 `GTEST_SKIP` 而非
fail，其余用例不受影响：

| 依赖 | 覆盖用例 | 缺失时的行为 |
| --- | --- | --- |
| Xvfb | `X11PlatformTest.*` 全部 | 无可用 DISPLAY 时 SetUp 统一 skip |
| openbox | `X11WmIntegrationTest.*`（真实 WM 集成） | 与 Xvfb 一并检测，缺失 skip |
| xclip | `X11PlatformTest.ClipboardTextRoundtrip` | setText 走优雅失败路径，skip |

安装（Debian/Ubuntu）：

```bash
sudo apt install xvfb openbox xclip
```

`X11PlatformTest` 需要一个已运行的 X display（真桌面或手动起 Xvfb）：

```bash
Xvfb -screen 0 1280x800x24 :99 &
DISPLAY=:99 ctest --test-dir build-runtime -R X11Platform --output-on-failure
```

`X11WmIntegrationTest` 无需手动准备 display——测试进程自起自毁专用
Xvfb + openbox 子进程（flock 串行化、专用 display 号、崩溃陪葬），只要
两个二进制在 PATH 上即可；它也不读写既有 DISPLAY，不会影响并行用例。

#### Linux OpenCV vision（可选）

`findImage`（模板匹配）、`Bitmap` PNG 编解码、`screenshot.capture` 的 JPEG
编码依赖 OpenCV（vcpkg `vision` feature，Linux 开发构建按需启用；Windows
构建经顶层 manifest 依赖恒启用）：

```bash
cmake -S . -B build-runtime -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DVCPKG_MANIFEST_FEATURES="tests;vision" \
  # ...其余参数同现有配置
```

首次启用会源码编译 opencv4（约 15-30 分钟）。不启用时 Linux 构建走
`vision_stub.cpp`：`findImage` 恒 false、`screenshot.capture` 返回明确
错误信封（相关测试 GTEST_SKIP / 断言降级路径）。CI Linux job 不启用该
feature（compat 构建不编 lib/wingman，不装 OpenCV）。

### Lua 测试

```cmd
REM Requires LuaRocks and Busted (see above)
examples\lua_scripts\run-lua-tests.cmd
```

### GUI ↔ Runtime 跨语言集成测试（Linux）

GUI 侧 Rust `IpcClient` ↔ C++ runtime `LocalIpcServer` 的本地 IPC 端到端测试
（`apps/gui/src-tauri/src/ipc/integration_tests.rs`）。每个用例 spawn 真 runtime
子进程（`start --standalone`，socket 注入临时目录），走 UDS 帧协议断言。

```bash
# 1. 构建 runtime（一次即可；Linux 无 Lua/X 运行期依赖）
cmake -S . -B build-runtime -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux -DVCPKG_MANIFEST_FEATURES=tests \
  -DCMAKE_BUILD_TYPE=Debug -DWINGMAN_BUILD_TESTS=ON \
  -DWINGMAN_BUILD_DESKTOP=ON -DWINGMAN_BUILD_LUA=OFF
cmake --build build-runtime --target wingman-agent

# 2. 跑集成测试（自动定位 build-runtime 产物，或用 WINGMAN_AGENT_BIN 指定）
cargo test --manifest-path apps/gui/src-tauri/Cargo.toml integration_tests -- --nocapture
```

runtime 二进制缺失时用例打印 `SKIP:` 跳过（不 fail）。CI Linux job 只构建
proto+transport，故本测试不进 CI，属开发机验证。

## CI/CD

本项目使用 GitHub Actions 做持续集成：

- **Build**：push 到 main 时构建
- **Test**：运行 C++ 单元测试（CI 中跳过 Lua 测试）
- **Nightly**：每日自动构建并打版本标签

Lua 测试由开发者在本地运行，而非在 CI 中，因为 LuaRocks 是可选开发工具。

## 其他资源

- [Lua Reference Manual](https://www.lua.org/manual/5.5/)
- [LuaRocks Documentation](https://luarocks.org/)
- [Busted Documentation](https://olivinelabs.com/busted/)
