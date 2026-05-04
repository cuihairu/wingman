# 图像匹配

演示如何在屏幕上查找图像。

## 代码

```lua
-- scripts/examples/image-matching.lua

local screen = require("wingman.screen")
local util = require("wingman.util")

-- 要查找的图像路径
local imagePath = "images/target.png"

-- 搜索区域
local x1, y1 = 0, 0
local x2, y2 = 1920, 1080

-- 匹配阈值 (0-1)
local threshold = 0.9

while true do
  local result = screen.findImage(imagePath, x1, y1, x2, y2, threshold)
  
  if result then
    print(string.format("Found image at: %d, %d (confidence: %.2f)", 
      result.x, result.y, result.confidence))
    
    -- 点击找到的位置
    input.click(result.x, result.y)
  else
    print("Image not found")
  end
  
  util.sleep(500)
end
```

## 运行

```bash
wingman.exe scripts/examples/image-matching.lua
```
