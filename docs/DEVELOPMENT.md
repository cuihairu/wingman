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
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Lua Development Tools

Wingman includes Lua scripting support. For Lua development, you can optionally install:

### LuaRocks (Package Manager)

LuaRocks is a package manager for Lua modules, similar to npm for Node.js.

**Installation:**
```bash
scripts\install-luarocks.cmd
```

**Usage:**
```bash
# Add to PATH (temporary)
set PATH=%CD%\scripts\luarocks;%PATH%

# Install a package
luarocks install lua-cjson

# List installed packages
luarocks list
```

### Busted (Testing Framework)

Busted is a unit testing framework for Lua.

**Installation:**
```bash
# First install LuaRocks, then:
scripts\install-busted.cmd
```

**Run Tests:**
```bash
scripts\run-lua-tests.cmd
```

**Or manually:**
```bash
set PATH=%CD%\scripts\luarocks;%PATH%
busted tests -o utfTerminal
```

### Project Structure

```
wingman/
├── include/           # C++ headers
├── src/              # C++ source files
├── bindings/         # Lua C API bindings
├── scripts/          # Lua scripts and developer tools
│   ├── install-luarocks.cmd
│   ├── install-busted.cmd
│   └── run-lua-tests.cmd
├── tests/            # Lua tests
└── docs/             # Documentation
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
print("Color at (100, 100): " .. color:toHex())

-- Move mouse
input.move(500, 300)
```

### Installing External Lua Packages

```bash
# Using LuaRocks
luarocks install lua-cjson     # JSON parser
luarocks install luafilesystem # File operations
luarocks install penlight      # Lua utility libraries
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

```bash
# Build with tests enabled
cmake -B build -DWINGMAN_BUILD_TESTS=ON
cmake --build build --config Debug

# Run tests
.\build\tests\cpp\Debug\wingman_tests.exe
```

### Lua Tests

```bash
# Requires LuaRocks and Busted (see above)
scripts\run-lua-tests.cmd
```

## CI/CD

The project uses GitHub Actions for continuous integration:

- **Build**: Builds on push to main/develop
- **Test**: Runs C++ unit tests (Lua tests skipped in CI)
- **Nightly**: Daily automated builds with version tagging

Lua tests are run locally by developers, not in CI, since LuaRocks is an optional dev tool.

## Additional Resources

- [Lua Reference Manual](https://www.lua.org/manual/5.4/)
- [LuaRocks Documentation](https://luarocks.org/)
- [Busted Documentation](https://olivinelabs.com/busted/)
