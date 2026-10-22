> [中文](hello-world.md) | **English**

# Hello World

The simplest Wingman script example.

## Code

```lua
-- examples/lua_scripts/hello_world.lua

local wingman = require("wingman")

print("=== Wingman Hello World ===")

-- 获取屏幕尺寸
local width = wingman.screen.getScreenWidth()
local height = wingman.screen.getScreenHeight()
print(string.format("屏幕尺寸: %d x %d", width, height))

-- 等待 1 秒
wingman.util.sleep(1000)

print("脚本执行完成!")
```

## Running

```bash
wingman-agent.exe script examples/lua_scripts/hello_world.lua
```

## Output

```
=== Wingman Hello World ===
屏幕尺寸: 1920 x 1080
脚本执行完成!
```
