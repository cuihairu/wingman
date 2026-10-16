> **English** | [中文](DEVELOPER_TOOLS.md)

# 开发者工具

Wingman 贡献者可用的可选开发工具位于
`examples/lua_scripts/`（Lua 测试栈的 Windows `.cmd` 辅助脚本）。

## 可用脚本

| 脚本（位于 `examples/lua_scripts/`） | 说明 | 依赖 |
|--------|-------------|-----------|
| `install-luarocks.cmd` | 安装 LuaRocks 包管理器 | 可选 |
| `install-busted.cmd` | 安装 Busted 测试框架 | LuaRocks |
| `run-lua-tests.cmd` | 运行 Lua 单元测试 | LuaRocks + Busted |

## 安装

### 1. LuaRocks（包管理器）

```bash
examples\lua_scripts\install-luarocks.cmd
```

将 LuaRocks 安装到 `examples/lua_scripts/luarocks/`

### 2. Busted（测试框架）

```bash
examples\lua_scripts\install-busted.cmd
```

需要先安装 LuaRocks。

### 3. 运行测试

```bash
examples\lua_scripts\run-lua-tests.cmd
```

需要同时安装 LuaRocks 和 Busted。

## 说明

- 这些工具对开发而言是**可选的**
- Wingman 不需要它们即可运行——仅在 Lua 开发/测试时需要
- LuaRocks 是本地安装，而非系统级安装
- 加入 PATH：`set PATH=%CD%\examples\lua_scripts\luarocks;%PATH%`

## 手动安装

如果脚本无法工作，请手动安装：

```bash
# Download LuaRocks
# https://luarocks.github.io/luarocks/releases/
# Extract to examples/lua_scripts/luarocks/

# Install Busted
luarocks install busted
```
