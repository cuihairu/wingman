# VS Code 开发环境

本文说明仓库内置的 VS Code 配置、脚本补全和调试工作流。

## 推荐扩展

打开仓库后，VS Code 会根据 `.vscode/extensions.json` 推荐以下主要扩展：

| 扩展 | 用途 |
|------|------|
| C/C++、CMake Tools | Runtime 和核心库开发 |
| `tangzx.emmylua`、`sumneko.lua` | Lua 脚本补全、诊断和调试 |
| Python、Pylance | Python 脚本类型检查 |
| rust-analyzer | Tauri GUI 后端 |
| Svelte | Tauri GUI 前端 |
| Go | Orchestrator server |

## 仓库级配置

已提交的 `.vscode/` 配置包含：

| 文件 | 说明 |
|------|------|
| `.vscode/settings.json` | C++23、CMake、Lua 5.4、Python typing 和搜索排除规则 |
| `.vscode/tasks.json` | Runtime、GUI、Docs、Go server 的常用构建/测试任务 |
| `.vscode/launch.json` | EmmyLua attach，以及通过 runtime 运行当前 Lua/Python 脚本 |
| `.vscode/extensions.json` | 推荐扩展列表 |

## Lua 补全

Lua 类型定义位于：

```text
assets/vscode-wingman-lua/wingman.d.lua
```

仓库级 `settings.json` 已将该目录加入 `Lua.workspace.library`。打开 `examples/lua_scripts` 或项目根目录时，Lua 扩展可以读取 `wingman` API 注解并提供补全。

如果只想编辑脚本目录，可以直接打开：

```powershell
code examples/wingman-scripts.code-workspace
```

这个 workspace 只包含示例脚本、配置目录和 Lua 类型定义，适合脚本调试和编写。

## Lua 调试

Runtime 默认配置文件 `apps/runtime/config/agent.toml` 已包含调试端口：

```toml
[debugger]
enable = true
listen_port = 9966
wait_for_ide = false
```

VS Code 中选择 `Attach to Wingman Lua (EmmyLua :9966)`，再运行启用了 EmmyLua 调试入口的 Lua 脚本即可连接。该配置使用 `emmylua_new` 调试器，走 `launch + ideConnectDebugger=true` 方式连接，默认端口为 `9966`。

> 注意：当前调试链路依赖 EmmyLua 调试组件和 runtime 构建参数 `WINGMAN_ENABLE_EMMY=ON`。如果运行时目录缺少 `emmy_core` 动态库，调试会降级为不可用，但脚本执行不受影响。

## 运行脚本

先构建 runtime：

```powershell
build-scripts\build-runtime-msvc-ninja.bat
```

然后在 VS Code 中打开任意 `.lua` 或 `.py` 脚本，选择：

| 配置 | 用途 |
|------|------|
| `Run current Lua script (runtime)` | 调用 `wingman-runtime.exe script ${file}` |
| `Run current Python script (runtime)` | 调用 `wingman-runtime.exe script ${file}` |

Python 脚本需要使用 `WINGMAN_ENABLE_PYTHON=ON` 重新配置构建，并安装 vcpkg `python` feature 依赖。

## Python 类型提示

Python 类型 stub 位于：

```text
libs/python/typing/wingman
```

`settings.json` 已配置 `python.analysis.stubPath` 和 `python.analysis.extraPaths`。Pylance 会用这些 `.pyi` 文件为 `from wingman import screen, input` 等导入提供补全。

## 常用任务

在 VS Code 命令面板运行 `Tasks: Run Task`：

| 任务 | 命令 |
|------|------|
| `configure runtime` | `build-scripts/configure-msvc-ninja.bat` |
| `build runtime` | `build-scripts/build-runtime-msvc-ninja.bat` |
| `build gui` | `npm run build` in `apps/gui` |
| `build docs` | `npm run docs:build` in `docs` |
| `test orchestrator server` | `go test ./...` in `orchestrator/server` |

## 边界说明

- GUI 不直接连接 runtime HTTP/WebSocket；本地控制必须走 Tauri backend -> local IPC -> runtime。
- Dashboard 只连接 Go orchestrator，不直接连接 runtime。
- Local TCP IPC 仅允许显式 debug fallback，默认关闭。
