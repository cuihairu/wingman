# 像素检测

演示如何检测屏幕上的指定颜色。

## 代码

```lua
-- scripts/examples/pixel-detection.lua

local screen = require("wingman.screen")
local util = require("wingman.util")

-- 目标颜色：红色
local targetColor = 0xFF0000

-- 搜索区域：全屏
local x1, y1 = 0, 0
local x2, y2 = 1920, 1080

-- 颜色容差
local tolerance = 10

-- 查找颜色
while true do
  local points = screen.findColor(targetColor, x1, y1, x2, y2, tolerance)
  
  if points then
    print(string.format("Found %d points", #points))
    for i, point in ipairs(points) do
      print(string.format("  Point %d: %d, %d", i, point.x, point.y))
    end
  else
    print("No points found")
  end
  
  -- 等待 100ms
  util.sleep(100)
end
```

## 运行

```bash
wingman.exe scripts/examples/pixel-detection.lua
```
