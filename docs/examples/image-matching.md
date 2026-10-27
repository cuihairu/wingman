> [中文](../zh/examples/image-matching.md) | **English**

# Image Matching

Demonstrates how to find an image on the screen.

> You must supply the template images yourself: the repository does not ship example images like `images/target.png`; prepare a small PNG and place it in the script's working directory first.

## Code

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

## When You Need the Match Confidence

`wingman.screen.findImage` does not return a confidence value; use `wingman.vision.findImage` instead (it returns a `{found, position, confidence, region}` object):

```lua
local wingman = require("wingman")

local result = wingman.vision.findImage("images/target.png", 0.9, {x = 0, y = 0, width = 1920, height = 1080})
if result.found then
    print(string.format("Found at (%d, %d), confidence: %.2f",
        result.position.x, result.position.y, result.confidence))
    wingman.input.click(result.position.x, result.position.y)
end
```

## Running

```bash
wingman-agent.exe script examples/lua_scripts/image_matching.lua
```
