# Developer Tools

This directory contains optional development tools for Wingman contributors.

## Available Scripts

| Script | Description | Required |
|--------|-------------|-----------|
| `install-luarocks.cmd` | Install LuaRocks package manager | Optional |
| `install-busted.cmd` | Install Busted testing framework | LuaRocks |
| `run-lua-tests.cmd` | Run Lua unit tests | LuaRocks + Busted |

## Installation

### 1. LuaRocks (Package Manager)

```bash
install-luarocks.cmd
```

Installs LuaRocks to `scripts/luarocks/`

### 2. Busted (Testing Framework)

```bash
install-busted.cmd
```

Requires LuaRocks to be installed first.

### 3. Run Tests

```bash
run-lua-tests.cmd
```

Requires both LuaRocks and Busted.

## Notes

- These tools are **optional** for development
- Wingman runs without them - only needed for Lua development/testing
- LuaRocks is installed locally, not system-wide
- Add to PATH: `set PATH=%CD%\scripts\luarocks;%PATH%`

## Manual Installation

If scripts don't work, install manually:

```bash
# Download LuaRocks
# https://luarocks.github.io/luarocks/releases/
# Extract to scripts/luarocks/

# Install Busted
luarocks install busted
```
