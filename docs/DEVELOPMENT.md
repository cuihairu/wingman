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
│   ├── clasp/               # Command-line library
│   ├── debug/               # EmmyLua debugger adapter
│   ├── lua/                 # Lua engine binding
│   ├── python/              # Python engine binding
│   ├── proto/               # Protobuf wrapper
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
-- script.lua

-- Use built-in modules
local screen = require("wingman.screen")
local input = require("wingman.input")

-- Get screen size
local width, height = screen.getDimensions()
print("Screen: " .. width .. "x" .. height)

-- Get pixel color
local color = screen.getPixel(100, 100)
print("Color at (100, 100): " .. string.format("0x%06X", color))

-- Move mouse
input.click(500, 300)
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
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake -S . -B build-msvc-ninja-vcpkg -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:\Users\admin\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DWINGMAN_BUILD_TESTS=ON -DBUILD_CORE_TESTS=ON"
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" && cmake --build build-msvc-ninja-vcpkg --config Debug"

REM Run tests
.\build-msvc-ninja-vcpkg\lib\wingman\tests\core_tests.exe
```

### Lua Tests

```cmd
REM Requires LuaRocks and Busted (see above)
scripts\run-lua-tests.cmd
```

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
