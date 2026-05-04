# 自动化循环

演示持续监控并执行操作的自动化脚本。

## 代码

```lua
-- scripts/examples/auto-loop.lua

local screen = require("wingman.screen")
local input = require("wingman.input")
local util = require("wingman.util")

-- 配置
local TARGET_COLOR = 0x00FF00  -- 绿色
local SEARCH_REGION = {0, 0, 1920, 1080}
local COOLDOWN = 1000  -- 冷却时间 1 秒

local lastActionTime = 0

print("Auto loop started... Press Ctrl+C to stop")

while true do
  -- 检查冷却
  local currentTime = util.getTime()
  if currentTime - lastActionTime >= COOLDOWN then
    -- 查找目标颜色
    local points = screen.findColor(TARGET_COLOR, 
      SEARCH_REGION[1], SEARCH_REGION[2], 
      SEARCH_REGION[3], SEARCH_REGION[4], 
      10)
    
    if points and #points > 0 then
      -- 找到目标，执行操作
      local point = points[1]
      print(string.format("Target found at: %d, %d", point.x, point.y))
      
      -- 点击目标
      input.click(point.x, point.y)
      
      -- 更新冷却时间
      lastActionTime = currentTime
    end
  end
  
  -- 短暂休眠避免高 CPU 占用
  util.sleep(50)
end
```

## 运行

```bash
wingman.exe scripts/examples/auto-loop.lua
```
