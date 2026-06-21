# 调试指南

Wingman 的 Lua 调试基于 EmmyLua。仓库已提供 VS Code 配置，可直接用于 attach 调试和通过 runtime 运行当前脚本。

## VS Code 配置

推荐先安装 VS Code 提示的扩展，或手动安装：

- `tangzx.emmylua`
- `sumneko.lua`
- `ms-python.python`
- `ms-python.vscode-pylance`

仓库内置文件：

| 文件 | 说明 |
|------|------|
| `.vscode/launch.json` | `Attach to Wingman Lua (EmmyLua :9966)` 和脚本运行配置 |
| `.vscode/settings.json` | Lua 5.4、Wingman Lua 类型库、Python stub 路径 |
| `assets/vscode-wingman-lua/wingman.d.lua` | Lua API 补全声明 |
| `libs/python/typing/wingman` | Python API 类型声明 |

完整配置说明见 [VS Code 开发环境](../development-environment.md)。

## Lua 调试流程

1. 构建 runtime：`build-scripts\build-runtime-msvc-ninja.bat`
2. 确认 runtime 配置启用调试：`apps/runtime/config/agent.toml` 中 `[debugger] enable = true`
3. 在 VS Code 选择 `Attach to Wingman Lua (EmmyLua :9966)`
4. 运行需要调试的 Lua 脚本
5. 使用断点、单步、变量窗口定位问题

默认调试端口是 `9966`。

## 运行当前脚本

VS Code 的 `Run current Lua script (runtime)` 和 `Run current Python script (runtime)` 会执行：

```powershell
.\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe script <当前文件>
```

Python 脚本需要使用 `WINGMAN_ENABLE_PYTHON=ON` 重新配置构建。

## 日志调试

使用通知/日志类模块输出调试信息：

```lua
local notify = require("wingman.notify")

notify.info("Info message")
notify.warn("Warning message")
notify.error("Error message")
```

## 常见问题

### 脚本无响应

- 检查是否有死循环
- 缩小截图或图像匹配区域
- 在关键分支增加日志输出

### 性能问题

- 优化图像查找范围
- 减少不必要的屏幕截图

### 断点未命中

- 确认 VS Code 已连接到 `localhost:9966`
- 确认 runtime 构建启用了 `WINGMAN_ENABLE_EMMY`
- 确认运行目录中存在 EmmyLua 调试组件
