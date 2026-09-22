# Wingman Development Guide

## Quick Start for Developers

### Prerequisites

- Windows 10/11
- Visual Studio 2022 (or Build Tools)
- CMake 3.20+
- vcpkg
- Git

### Building

```bash
# Clone and build
git clone https://github.com/cuihairu/wingman.git
cd wingman
build-scripts\build-runtime-msvc-ninja.bat
```

## VS Code Workspace

The repository includes checked-in VS Code configuration for common development
flows:

- `.vscode/settings.json` configures C++23, CMake, Lua 5.4 and Python stubs.
- `.vscode/tasks.json` exposes runtime, GUI, docs and Go server tasks.
- `.vscode/launch.json` contains EmmyLua attach and current-script launch configs.
- `examples/wingman-scripts.code-workspace` is a lightweight workspace for script authors.

See [VS Code 开发环境](development-environment.md) for the full workflow.

## Project Structure

```
wingman/
├── apps/                    # Applications
│   ├── runtime/             # CLI runtime
│   ├── gui/                 # Tauri GUI
├── lib/                     # Core library
│   └── wingman/             # Core functionality
├── libs/                    # Support libraries
│   ├── lua/                 # Lua engine binding
│   ├── python/              # Python engine binding
│   └── transport/           # TCP/WebSocket transport
└── orchestrator/            # Orchestration layer
    ├── dashboard/           # Web dashboard
    └── server/             # Go server
```

## Lua Development Tools

Wingman includes Lua scripting support. For Lua development, you can optionally install:

### LuaRocks (Package Manager)

LuaRocks is a package manager for Lua modules, similar to npm for Node.js.

**Installation:**
```cmd
scripts\install-luarocks.cmd
```

**Usage:**
```cmd
REM Add to PATH (temporary)
set PATH=%CD%\scripts\luarocks;%PATH%

REM Install a package
luarocks install lua-cjson

REM List installed packages
luarocks list
```

### Busted (Testing Framework)

Busted is a unit testing framework for Lua.

**Installation:**
```cmd
REM First install LuaRocks, then:
scripts\install-busted.cmd
```

**Run Tests:**
```cmd
scripts\run-lua-tests.cmd
```

**Or manually:**
```cmd
set PATH=%CD%\scripts\luarocks;%PATH%
busted tests -o utfTerminal
```

## Creating Lua Scripts

### Basic Example

```lua
local wingman = require("wingman")

-- script.lua

-- Use built-in modules
-- Get screen size
local width, height = wingman.screen.getDimensions()
print("Screen: " .. width .. "x" .. height)

-- Get pixel color
local color = wingman.screen.getPixel(100, 100)
print("Color at (100, 100): " .. string.format("0x%06X", color))

-- Move mouse
wingman.input.click(500, 300)
```

### Installing External Lua Packages

```cmd
REM Using LuaRocks
luarocks install lua-cjson     -- JSON parser
luarocks install luafilesystem -- File operations
luarocks install penlight      -- Lua utility libraries
```

Then in your script:
```lua
local cjson = require("cjson")
local lfs = require("lfs")

-- Use the packages
local data = cjson.encode({name = "Wingman", version = "0.1.0"})
```

## Running Tests

### C++ Unit Tests

```cmd
REM Build with tests enabled
cmake -S . -B build-tests ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_MANIFEST_FEATURES=tests ^
  -DWINGMAN_BUILD_TESTS=ON
cmake --build build-tests --config Debug --target core_tests runtime_tests transport_tests proto_tests debug_tests

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

### Lua Tests

```cmd
REM Requires LuaRocks and Busted (see above)
scripts\run-lua-tests.cmd
```

### GUI↔Runtime 跨语言集成测试（Linux）

GUI 侧 Rust `IpcClient` ↔ C++ runtime `LocalIpcServer` 的本地 IPC 端到端测试
（`apps/gui/src-tauri/src/ipc/integration_tests.rs`）。每个用例 spawn 真 runtime
子进程（`start --standalone`，socket 注入临时目录），走 UDS 帧协议断言。

```bash
# 1. 构建 runtime（一次即可；Linux 无 Lua/X 运行期依赖）
cmake -S . -B build-runtime -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux -DVCPKG_MANIFEST_FEATURES=tests \
  -DCMAKE_BUILD_TYPE=Debug -DWINGMAN_BUILD_TESTS=ON \
  -DWINGMAN_BUILD_RUNTIME=ON -DWINGMAN_BUILD_AGENT=OFF -DWINGMAN_BUILD_LUA=OFF
cmake --build build-runtime --target wingman-runtime

# 2. 跑集成测试（自动定位 build-runtime 产物，或用 WINGMAN_RUNTIME_BIN 指定）
cargo test --manifest-path apps/gui/src-tauri/Cargo.toml integration_tests -- --nocapture
```

runtime 二进制缺失时用例打印 `SKIP:` 跳过（不 fail）。CI Linux job 只构建
proto+transport，故本测试不进 CI，属开发机验证。

## CI/CD

The project uses GitHub Actions for continuous integration:

- **Build**: Builds on push to main
- **Test**: Runs C++ unit tests (Lua tests skipped in CI)
- **Nightly**: Daily automated builds with version tagging

Lua tests are run locally by developers, not in CI, since LuaRocks is an optional dev tool.

## Additional Resources

- [Lua Reference Manual](https://www.lua.org/manual/5.4/)
- [LuaRocks Documentation](https://luarocks.org/)
- [Busted Documentation](https://olivinelabs.com/busted/)
