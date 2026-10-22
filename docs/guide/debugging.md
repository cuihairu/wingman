> [中文](debugging.md) | **English**

# Debugging Guide

Wingman's Lua debugging is based on EmmyLua. The repository ships a VS Code configuration that can be used directly for attach debugging and for running the current script through the runtime.

## VS Code Configuration

Install the extensions suggested by VS Code first, or install them manually:

- `tangzx.emmylua`
- `sumneko.lua`
- `ms-python.python`
- `ms-python.vscode-pylance`

Files bundled in the repository:

| File | Purpose |
|------|---------|
| `.vscode/launch.json` | The `Attach to Wingman Lua (EmmyLua :9966)` configuration plus script run configurations |
| `.vscode/settings.json` | Lua, the Wingman Lua type library, and Python stub paths |
| `assets/vscode-wingman-lua/wingman.d.lua` | Lua API completion declarations |
| `libs/python/typing/wingman` | Python API type declarations |

See [VS Code Development Environment](../development-environment.md) for the full configuration details.

## Lua Debugging Workflow

1. Build the runtime: `build-scripts\build-runtime-msvc-ninja.bat`
2. Confirm debugging is enabled in the runtime configuration: `[debugger] enable = true` in `apps/agent/config/agent.toml`
3. In VS Code, select `Attach to Wingman Lua (EmmyLua :9966)`
4. Run the Lua script you want to debug
5. Use breakpoints, stepping, and the variables panel to locate the problem

The default debugging port is `9966`.

## Running the Current Script

The VS Code `Run current Lua script (runtime)` and `Run current Python script (runtime)` commands execute:

```powershell
.\build-msvc-ninja-vcpkg\apps\agent\wingman-agent.exe script <当前文件>
```

Python scripts require reconfiguring the build with `WINGMAN_ENABLE_PYTHON=ON`.

## Logging-Based Debugging

Use the notification/logging modules to output debug information:

```lua
local wingman = require("wingman")

wingman.notify.info("Info message")
wingman.notify.warn("Warning message")
wingman.notify.error("Error message")
```

## Troubleshooting

### Script Not Responding

- Check for infinite loops
- Narrow the screenshot or image-matching region
- Add log output at key branches

### Performance Issues

- Optimize the image search region
- Reduce unnecessary screenshots

### Breakpoints Not Hit

- Confirm port 9966 is not already occupied
- Confirm `.vscode/launch.json` has the `emmylua_new` configuration selected (`Attach to Wingman Lua (EmmyLua :9966)`)
- Confirm VS Code is connected to `localhost:9966`
