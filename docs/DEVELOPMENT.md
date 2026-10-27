> **English** | [中文](zh/DEVELOPMENT.md)

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

- `.vscode/settings.json` configures C++23, CMake, Lua and Python stubs.
- `.vscode/tasks.json` exposes runtime, GUI, docs and Go server tasks.
- `.vscode/launch.json` contains EmmyLua attach and current-script launch configs.
- `examples/wingman-scripts.code-workspace` is a lightweight workspace for script authors.

See [VS Code Development Environment](development-environment.md) for the full workflow.

## Project Structure

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

## Lua Development Tools

Wingman includes Lua scripting support. For Lua development, you can optionally install:

### LuaRocks (Package Manager)

LuaRocks is a package manager for Lua modules, similar to npm for Node.js.

**Installation:**
```cmd
examples\lua_scripts\install-luarocks.cmd
```

**Usage:**
```cmd
REM Add to PATH (temporary)
set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%

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
examples\lua_scripts\install-busted.cmd
```

**Run Tests:**
```cmd
examples\lua_scripts\run-lua-tests.cmd
```

**Or manually:**
```cmd
set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%
busted tests -o utfTerminal
```

## Creating Lua Scripts

### Basic Example

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
cmake --build build-tests --config Debug --target core_tests agent_tests transport_tests

REM Run tests
ctest --test-dir build-tests -C Debug --output-on-failure
```

`WINGMAN_BUILD_TESTS=ON` automatically enables the standard C++ test suite; Lua binding-layer tests are still toggled separately on demand via `BUILD_LUA_TESTS=ON`.
Use a dedicated `build-tests/` directory for tests so an existing development build directory is not clobbered.

### Optional Linux X11 test dependencies

On Linux the `platform_x11_test.cpp` cases depend on three **runtime-optional dependencies** (not in the
vcpkg manifest; provided by the system package manager). When one is missing the affected cases `GTEST_SKIP`
instead of failing; the remaining cases are unaffected:

| Dependency | Covered cases | Behavior when missing |
| --- | --- | --- |
| Xvfb | all `X11PlatformTest.*` | SetUp skips uniformly when no DISPLAY is available |
| openbox | `X11WmIntegrationTest.*` (real-WM integration) | probed together with Xvfb; skipped when missing |
| xclip | `X11PlatformTest.ClipboardTextRoundtrip` | setText takes the graceful-failure path; skipped |

Install (Debian/Ubuntu):

```bash
sudo apt install xvfb openbox xclip
```

`X11PlatformTest` needs a running X display (a real desktop or a manually started Xvfb):

```bash
Xvfb -screen 0 1280x800x24 :99 &
DISPLAY=:99 ctest --test-dir build-runtime -R X11Platform --output-on-failure
```

`X11WmIntegrationTest` needs no manually prepared display — the test process starts and tears down its own
dedicated Xvfb + openbox subprocesses (flock serialization, a dedicated display number, crash reaping). The
two binaries only have to be on PATH; the test neither reads nor writes the existing DISPLAY, so parallel
cases are unaffected.

#### Linux OpenCV vision (optional)

`findImage` (template matching), the `Bitmap` PNG codec and the JPEG encoding of `screenshot.capture`
depend on OpenCV (the vcpkg `vision` feature, enabled on demand for Linux development builds; Windows
builds enable it permanently via the top-level manifest):

```bash
cmake -S . -B build-runtime -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DVCPKG_MANIFEST_FEATURES="tests;vision" \
  # ...其余参数同现有配置
```

The first enablement compiles opencv4 from source (about 15-30 minutes). When it is disabled the Linux build
uses `vision_stub.cpp`: `findImage` always returns false and `screenshot.capture` returns a well-defined error
envelope (affected tests GTEST_SKIP / degraded-assertion paths). The CI Linux job does not enable this feature
(the compat build does not compile lib/wingman and does not install OpenCV).

### Lua Tests

```cmd
REM Requires LuaRocks and Busted (see above)
examples\lua_scripts\run-lua-tests.cmd
```

### GUI ↔ Runtime cross-language integration tests (Linux)

End-to-end local IPC tests between the GUI-side Rust `IpcClient` and the C++ runtime `LocalIpcServer`
(`apps/gui/src-tauri/src/ipc/integration_tests.rs`). Each case spawns a real runtime subprocess
(`start --standalone`, socket injected into a temp directory) and asserts over the UDS frame protocol.

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

When the runtime binary is missing the cases print `SKIP:` and skip (they do not fail). The CI Linux job only
builds proto+transport, so this test is not part of CI; it is a development-machine verification.

## CI/CD

The project uses GitHub Actions for continuous integration:

- **Build**: Builds on push to main
- **Test**: Runs C++ unit tests (Lua tests skipped in CI)
- **Nightly**: Daily automated builds with version tagging

Lua tests are run locally by developers, not in CI, since LuaRocks is an optional dev tool.

## Additional Resources

- [Lua Reference Manual](https://www.lua.org/manual/5.5/)
- [LuaRocks Documentation](https://luarocks.org/)
- [Busted Documentation](https://olivinelabs.com/busted/)
