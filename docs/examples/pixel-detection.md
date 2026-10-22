> [中文](pixel-detection.md) | **English**

# Pixel Detection

Demonstrates how to detect a specified color on the screen.

## Code

```lua
-- examples/lua_scripts/pixel_detection.lua

local wingman = require("wingman")

-- 目标颜色：红色
local targetColor = 0xFF0000

-- 搜索区域：全屏（表）
local region = {x = 0, y = 0, width = 1920, height = 1080}

-- 颜色容差
local tolerance = 10

-- 查找颜色（findColors 返回命中点数组）
while true do
  local points = wingman.screen.findColors(targetColor, region, tolerance)

  if #points > 0 then
    print(string.format("Found %d points", #points))
    for i, point in ipairs(points) do
      print(string.format("  Point %d: %d, %d", i, point.x, point.y))
    end
  else
    print("No points found")
  end

  -- 等待 100ms
  wingman.util.sleep(100)
end
```

> When you only need to check for existence and grab the first hit point, use `wingman.screen.findColor(targetColor, region, tolerance)` instead: it returns a single array `{hit point, found}`, so take `res[1]` / `res[2]` (the second argument must be a region table; passing scalar coordinates yields an empty rectangle, so nothing is ever found).

## Running

```bash
wingman-agent.exe script examples/lua_scripts/pixel_detection.lua
```
