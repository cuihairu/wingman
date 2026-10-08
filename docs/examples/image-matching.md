# 图像匹配

演示如何在屏幕上查找图像。

> 模板图片需自备：仓库不附带 `images/target.png` 等示例图，请先准备一张小尺寸 PNG 放到脚本运行目录下。

## 代码

```lua
-- examples/lua_scripts/image_matching.lua

local wingman = require("wingman")

-- 要查找的图像路径（需自备模板图片）
local imagePath = "images/target.png"

-- 搜索区域（表）
local region = {x = 0, y = 0, width = 1920, height = 1080}

-- 匹配阈值 (0-1)
local threshold = 0.9

while true do
  -- 返回单数组：res[1] = 命中点，res[2] = 是否找到
  local res = wingman.screen.findImage(imagePath, region, threshold)

  if res[2] then
    print(string.format("Found image at: %d, %d", res[1].x, res[1].y))

    -- 点击找到的位置
    wingman.input.click(res[1].x, res[1].y)
  else
    print("Image not found")
  end

  wingman.util.sleep(500)
end
```

## 需要匹配置信度（confidence）时

`wingman.screen.findImage` 不返回置信度；改用 `wingman.vision.findImage`（返回 `{found, position, confidence, region}` 对象）：

```lua
local wingman = require("wingman")

local result = wingman.vision.findImage("images/target.png", 0.9, {x = 0, y = 0, width = 1920, height = 1080})
if result.found then
    print(string.format("Found at (%d, %d), confidence: %.2f",
        result.position.x, result.position.y, result.confidence))
    wingman.input.click(result.position.x, result.position.y)
end
```

## 运行

```bash
wingman-runtime.exe script examples/lua_scripts/image_matching.lua
```
