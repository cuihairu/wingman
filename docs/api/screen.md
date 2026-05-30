# API: wingman.screen

屏幕操作模块。

## 截取屏幕

:::tabs

== Python

```python:line-numbers
# 截取全屏
img = screen.capture(0, 0, 1920, 1080)

# 截取指定区域
img = screen.capture(100, 100, 500, 500)
```

== Lua

```lua:line-numbers
-- 截取全屏
local img = screen.capture(0, 0, 1920, 1080)

-- 截取指定区域
local img = screen.capture(100, 100, 500, 500)
```

:::

## 获取像素颜色

:::tabs

== Python

```python:line-numbers
color = screen.get_pixel(100, 100)
print(f"Color: 0x{color:06X}")
```

== Lua

```lua:line-numbers
local color = screen.getPixel(100, 100)
print(string.format("Color: 0x%06X", color))
```

:::

## 查找像素

:::tabs

== Python

```python:line-numbers
# 查找单个像素
result = screen.find_pixel(0xFF0000, 0, 0, 1920, 1080, 10)
if result:
    x, y = result
    print(f"Found at: {x}, {y}")
```

== Lua

```lua:line-numbers
-- 查找单个像素
local x, y = screen.findPixel(0xFF0000, 0, 0, 1920, 1080, 10)
if x then
    print(string.format("Found at: %d, %d", x, y))
end
```

:::

## 查找颜色

:::tabs

== Python

```python:line-numbers
# 查找指定颜色
results = screen.find_color(0xFF0000, 0, 0, 1920, 1080)
for x, y in results:
    print(f"Found red pixel at: {x}, {y}")
```

== Lua

```lua:line-numbers
-- 查找指定颜色
local results = screen.findColor(0xFF0000, 0, 0, 1920, 1080)
for i, result in ipairs(results) do
    local x, y = result.x, result.y
    print(string.format("Found red pixel at: %d, %d", x, y))
end
```

:::

## 图像匹配

:::tabs

== Python

```python:line-numbers
# 查找图像
result = screen.find_image("target.png", 0, 0, 1920, 1080)
if result:
    x, y, confidence = result
    print(f"Found at: {x}, {y} (confidence: {confidence})")
```

== Lua

```lua:line-numbers
-- 查找图像
local result = screen.findImage("target.png", 0, 0, 1920, 1080)
if result then
    local x, y, confidence = result.x, result.y, result.confidence
    print(string.format("Found at: %d, %d (confidence: %f)", x, y, confidence))
end
```

:::

## 等待图像出现

:::tabs

== Python

```python:line-numbers
# 等待图像出现（最多等待 5 秒）
result = screen.wait_for_image("loading.png", 5000)
if result:
    print("加载完成")
```

== Lua

```lua:line-numbers
-- 等待图像出现（最多等待 5 秒）
local result = screen.waitForImage("loading.png", 5000)
if result then
    print("加载完成")
end
```

:::

---

## 完整示例

### 游戏挂机脚本

:::tabs

== Python

```python:line-numbers
from wingman import screen, input, util

# 等待游戏窗口激活
util.sleep(1000)

# 检查血量
hp_region = screen.capture(100, 50, 100, 20)
hp_color = screen.get_pixel_color(hp_region, 10, 10)

if hp_color < 0xFF0000:  # 血量低
    # 点击补血按钮
    input.click(200, 300)
    util.sleep(1000)

# 查找怪物
while True:
    monster = screen.find_image("monster.png", 0, 0, 1920, 1080)
    if monster:
        x, y, _ = monster
        input.click(x, y)
        util.sleep(500)
    else:
        util.sleep(1000)
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")
local input = require("wingman.input")
local util = require("wingman.util")

-- 等待游戏窗口激活
util.sleep(1000)

-- 检查血量
local hpRegion = screen.capture(100, 50, 100, 20)
local hpColor = screen.getPixelColor(hpRegion, 10, 10)

if hpColor < 0xFF0000 then
    -- 点击补血按钮
    input.click(200, 300)
    util.sleep(1000)
end

-- 查找怪物
while true do
    local monster = screen.findImage("monster.png", 0, 0, 1920, 1080)
    if monster then
        local x, y = monster.x, monster.y
        input.click(x, y)
        util.sleep(500)
    else
        util.sleep(1000)
    end
end
```

:::

---

## 可用接口

### `capture(x, y, width, height)` / `capture(x, y, width, height)`

截取屏幕区域。

**参数：**
- `x, y` - 起始坐标
- `width, height` - 区域大小

**返回：**
- Image 对象

### `get_pixel(x, y)` / `getPixel(x, y)`

获取指定坐标的像素颜色。

**参数：**
- `x, y` - 像素坐标

**返回：**
- `number` - RGB 颜色值（如 0xFF0000 表示红色）

### `find_pixel(color, x, y, width, height, tolerance?)` / `findPixel(color, x, y, width, height, tolerance?)`

查找指定颜色的像素。

**参数：**
- `color` - 目标颜色（RGB）
- `x, y, width, height` - 搜索区域
- `tolerance` - 可选，颜色容差（0-255），默认 10

**返回：**
- Python: `tuple(x, y) | None`
- Lua: `number x, number y` 或 `nil`

### `find_color(color, x, y, width, height, tolerance?)` / `findColor(color, x, y, width, height, tolerance?)`

查找所有指定颜色的像素。

**参数：**
- `color` - 目标颜色（RGB）
- `x, y, width, height` - 搜索区域
- `tolerance` - 可选，颜色容差，默认 10

**返回：**
- Python: `list[tuple(x, y)]`
- Lua: `table[{x, y}]`

### `find_image(image_path, x, y, width, height, threshold?)` / `findImage(imagePath, x, y, width, height, threshold?)`

查找图像。

**参数：**
- `image_path` / `imagePath` - 模板图像路径
- `x, y, width, height` - 搜索区域
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.9

**返回：**
- Python: `tuple(x, y, confidence) | None`
- Lua: `table{x, y, confidence} | nil`

### `wait_for_image(image_path, timeout, threshold?)` / `waitForImage(imagePath, timeout, threshold?)`

等待图像出现。

**参数：**
- `image_path` / `imagePath` - 模板图像路径
- `timeout` - 超时时间（毫秒）
- `threshold` - 可选，匹配阈值，默认 0.9

**返回：**
- 匹配结果，格式同 `find_image`
