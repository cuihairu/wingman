> [中文](../zh/examples/auto-loop.md) | **English**

# Auto Loop

Demonstrates an automation script that continuously monitors and performs actions.

## Code

```lua
-- examples/lua_scripts/auto_loop.lua

local wingman = require("wingman")

-- 配置
local TARGET_COLOR = 0x00FF00  -- 绿色
local SEARCH_REGION = {x = 0, y = 0, width = 1920, height = 1080}
local COOLDOWN = 1000  -- 冷却时间 1 秒

local lastActionTime = 0

print("Auto loop started... Press Ctrl+C to stop")

while true do
  -- 检查冷却
  local currentTime = wingman.util.getTime()
  if currentTime - lastActionTime >= COOLDOWN then
    -- 查找目标颜色（findColors 返回命中点数组）
    local points = wingman.screen.findColors(TARGET_COLOR, SEARCH_REGION, 10)

    if points and #points > 0 then
      -- 找到目标，执行操作
      local point = points[1]
      print(string.format("Target found at: %d, %d", point.x, point.y))

      -- 点击目标
      wingman.input.click(point.x, point.y)

      -- 更新冷却时间
      lastActionTime = currentTime
    end
  end

  -- 短暂休眠避免高 CPU 占用
  wingman.util.sleep(50)
end
```

> When you only need the first hit point, use `wingman.screen.findColor(color, region, tolerance)` instead: it returns a single array `{hit point, found}`, so take `res[1]` / `res[2]` (the Lua side does not expand it into multiple return values).

## Running

```bash
wingman-agent.exe script examples/lua_scripts/auto_loop.lua
```
