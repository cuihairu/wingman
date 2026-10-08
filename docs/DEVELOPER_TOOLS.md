# Developer Tools

Optional development tools for Wingman contributors live in
`examples/lua_scripts/` (Windows `.cmd` helpers for the Lua test stack).

## Available Scripts

| Script (in `examples/lua_scripts/`) | Description | Required |
|--------|-------------|-----------|
| `install-luarocks.cmd` | Install LuaRocks package manager | Optional |
| `install-busted.cmd` | Install Busted testing framework | LuaRocks |
| `run-lua-tests.cmd` | Run Lua unit tests | LuaRocks + Busted |

## Installation

### 1. LuaRocks (Package Manager)

```bash
examples\lua_scripts\install-luarocks.cmd
```

Installs LuaRocks to `examples/lua_scripts/luarocks/`

### 2. Busted (Testing Framework)

```bash
examples\lua_scripts\install-busted.cmd
```

Requires LuaRocks to be installed first.

### 3. Run Tests

```bash
examples\lua_scripts\run-lua-tests.cmd
```

Requires both LuaRocks and Busted.

## Notes

- These tools are **optional** for development
- Wingman runs without them - only needed for Lua development/testing
- LuaRocks is installed locally, not system-wide
- Add to PATH: `set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%`

## Manual Installation

If scripts don't work, install manually:

```bash
# Download LuaRocks
# https://luarocks.github.io/luarocks/releases/
# Extract to examples/lua_scripts/luarocks/

# Install Busted
luarocks install busted
```
