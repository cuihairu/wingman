> [中文](development-environment.md) | **English**

# VS Code Development Environment

This page describes the repository's built-in VS Code configuration, script completion and the debugging workflow.

## Recommended Extensions

After opening the repository, VS Code recommends the following key extensions from `.vscode/extensions.json`:

| Extension | Purpose |
|------|------|
| C/C++, CMake Tools | Runtime and core library development |
| `tangzx.emmylua`, `sumneko.lua` | Lua script completion, diagnostics and debugging |
| Python, Pylance | Python script type checking |
| rust-analyzer | Tauri GUI backend |
| Svelte | Tauri GUI frontend |
| Go | Orchestrator server |

## Repository-level Configuration

The committed `.vscode/` configuration contains:

| File | Description |
|------|------|
| `.vscode/settings.json` | C++23, CMake, Lua, Python typing and search exclusion rules |
| `.vscode/tasks.json` | Common build/test tasks for Runtime, GUI, Docs and the Go server |
| `.vscode/launch.json` | EmmyLua attach, plus running the current Lua/Python script through the runtime |
| `.vscode/extensions.json` | Recommended extension list |

## Lua Completion

The Lua type definitions live at:

```text
assets/vscode-wingman-lua/wingman.d.lua
```

The repository-level `settings.json` already adds that directory to `Lua.workspace.library`. When you open `examples/lua_scripts` or the repository root, the Lua extension can read the `wingman` API annotations and provide completion.

If you only want to edit the script directory, open it directly:

```powershell
code examples/wingman-scripts.code-workspace
```

This workspace contains only the example scripts, the configuration directory and the Lua type definitions, which is what you want for script debugging and authoring.

## Lua Debugging

The runtime default configuration `apps/agent/config/agent.toml` already includes the debug port:

```toml
[debugger]
enable = true
listen_port = 9966
wait_for_ide = false
```

In VS Code choose `Attach to Wingman Lua (EmmyLua :9966)`, then run a Lua script that enables the EmmyLua debug entry point to connect. This configuration uses the `emmylua_new` debugger and connects via `launch + ideConnectDebugger=true`, with the default port `9966`.

> Note: the current debugging chain depends on the EmmyLua debug component (the `emmy_core` dynamic library, loaded by the script runtime via require, with no build switch). If `emmy_core` is missing from the runtime directory, debugging degrades to unavailable, but script execution is unaffected.

## Running Scripts

Build the runtime first:

```powershell
build-scripts\build-runtime-msvc-ninja.bat
```

Then open any `.lua` or `.py` script in VS Code and choose:

| Configuration | Purpose |
|------|------|
| `Run current Lua script (runtime)` | Invokes `wingman-agent.exe script ${file}` |
| `Run current Python script (runtime)` | Invokes `wingman-agent.exe script ${file}` |

Python scripts require reconfiguring the build with `WINGMAN_ENABLE_PYTHON=ON` and installing the vcpkg `python` feature dependency.

## Python Type Hints

The Python type stubs live at:

```text
libs/python/typing/wingman
```

`settings.json` already sets `python.analysis.stubPath` and `python.analysis.extraPaths`. Pylance uses these `.pyi` files to provide completion for imports such as `from wingman import screen, input`.

## Common Tasks

Run `Tasks: Run Task` from the VS Code command palette:

| Task | Command |
|------|------|
| `configure runtime` | `build-scripts/configure-msvc-ninja.bat` |
| `build runtime` | `build-scripts/build-runtime-msvc-ninja.bat` |
| `build gui` | `npm run build` in `apps/gui` |
| `build docs` | `npm run docs:build` in `docs` |
| `test orchestrator server` | `go test ./...` in `orchestrator/server` |

## Boundary Notes

- The GUI does not connect directly to a runtime HTTP/WebSocket; local control must go through Tauri backend -> local IPC -> runtime.
- The Dashboard connects only to the Go orchestrator, never directly to the runtime.
- Local TCP IPC is allowed only as an explicit debug fallback and is disabled by default.
