# 像素检测

演示如何检测屏幕上的指定颜色。

## 代码

```lua
local wingman = require("wingman")

-- scripts/examples/pixel-detection.lua

-- 目标颜色：红色
local targetColor = 0xFF0000

-- 搜索区域：全屏
local x1, y1 = 0, 0
local x2, y2 = 1920, 1080

-- 颜色容差
local tolerance = 10

-- 查找颜色
while true do
  local points = wingman.screen.findColor(targetColor, x1, y1, x2, y2, tolerance)
  
  if points then
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

## 运行

```bash
wingman-runtime.exe script scripts/examples/pixel-detection.lua
```
