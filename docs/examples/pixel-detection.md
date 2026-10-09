# 像素检测

演示如何检测屏幕上的指定颜色。

## 代码

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

> 只需判断是否存在并取第一个命中点时，改用 `wingman.screen.findColor(targetColor, region, tolerance)`：返回单数组 `{命中点, 是否找到}`，取 `res[1]` / `res[2]`（第二参必须是 region 表；传标量坐标会得到零矩形，永远查不到）。

## 运行

```bash
wingman-agent.exe script examples/lua_scripts/pixel_detection.lua
```
