# API: wingman.screen

屏幕操作模块，提供屏幕截图、像素颜色获取、颜色查找、图像匹配等功能。

## 模块概述

screen 模块是 Wingman 的核心模块之一，用于与屏幕进行交互。主要功能包括：

- **屏幕截图**：截取全屏或指定区域
- **像素操作**：获取像素颜色、查找指定颜色的像素
- **图像匹配**：在屏幕中查找图像模板
- **等待机制**：等待图像或颜色出现

### 坐标系统

Wingman 使用屏幕左上角为原点 (0, 0) 的坐标系统：
- X 轴向右递增
- Y 轴向下递增
- 单位为像素

```
(0,0) ───────────────► X
  │
  │
  │
  ▼
  Y
```

### 数据类型

> **详细说明**：查看 [数据类型参考](./types.md) 了解以下对象的完整结构。

#### Image 对象

`capture()` 函数返回的 Image 对象表示截取的图像区域。该对象可以用于：

- 传递给 `find_image()` 作为模板进行图像匹配
- 传递给 `wait_for_image()` 等待图像出现

**注意**：Image 对象主要用于内部图像匹配操作，不直接暴露像素数据。如需获取像素颜色，请使用 `get_pixel()` 函数。

#### 匹配结果对象

`find_image()` 和 `wait_for_image()` 返回的匹配结果包含：

**Python**: `(x: int, y: int, confidence: float)` 元组
- `x` - 匹配位置 X 坐标
- `y` - 匹配位置 Y 坐标
- `confidence` - 相似度（0.0-1.0）

**Lua**: `{x: number, y: number, confidence: number}` 表格
- `x` - 匹配位置 X 坐标
- `y` - 匹配位置 Y 坐标
- `confidence` - 相似度（0.0-1.0）

#### 颜色值

RGB 颜色值使用 24 位整数表示，格式为 `0xRRGGBB`。参见 [颜色值](./types.md#颜色值) 了解详情。

---

## 截取屏幕

### capture(x, y, width, height)

**说明**：截取屏幕的指定区域，返回图像对象。

**函数签名**：

```python
capture(x: int, y: int, width: int, height: int) -> Image
```

```lua
capture(x: number, y: number, width: number, height: number) -> Image
```

**参数**：
- `x, y` - 截取区域的左上角坐标
- `width, height` - 截取区域的宽度和高度

**返回**：Image 对象，可用于：
- 作为 `find_image()` 的模板参数进行图像匹配
- 作为 `wait_for_image()` 的等待目标

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 截取全屏（假设屏幕分辨率为 1920x1080）
img = screen.capture(0, 0, 1920, 1080)

# 截取指定区域（从 100,100 开始，宽 400，高 300）
img = screen.capture(100, 100, 400, 300)

# 截取窗口的一个小区域用于后续处理
region = screen.capture(500, 200, 200, 50)
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 截取全屏（假设屏幕分辨率为 1920x1080）
local img = screen.capture(0, 0, 1920, 1080)

-- 截取指定区域（从 100,100 开始，宽 400，高 300）
local img = screen.capture(100, 100, 400, 300)

-- 截取窗口的一个小区域用于后续处理
local region = screen.capture(500, 200, 200, 50)
```

:::

---

## 获取像素颜色

### get_pixel(x, y) / getPixel(x, y)

**说明**：获取指定坐标的像素颜色值。

**函数签名**：

```python
get_pixel(x: int, y: int) -> int
```

```lua
getPixel(x: number, y: number) -> number
```

**参数**：
- `x, y` - 像素坐标

**返回**：RGB 颜色值（整数格式，如 0xFF0000 表示红色）

**颜色格式**：
- 返回值为 24 位 RGB 整数
- 十六进制格式：0xRRGGBB
- 例如：0xFF0000 = 纯红，0x00FF00 = 纯绿，0x0000FF = 纯蓝

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 获取 (100, 100) 位置的像素颜色
color = screen.get_pixel(100, 100)
print(f"颜色值: 0x{color:06X}")

# 检查是否为红色
if color == 0xFF0000:
    print("这是红色像素")

# 提取 RGB 分量
r = (color >> 16) & 0xFF
g = (color >> 8) & 0xFF
b = color & 0xFF
print(f"R={r}, G={g}, B={b}")
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 获取 (100, 100) 位置的像素颜色
local color = screen.getPixel(100, 100)
print(string.format("颜色值: 0x%06X", color))

-- 检查是否为红色
if color == 0xFF0000 then
    print("这是红色像素")
end

-- 提取 RGB 分量
local r = (color >> 16) & 0xFF
local g = (color >> 8) & 0xFF
local b = color & 0xFF
print(string.format("R=%d, G=%d, B=%d", r, g, b))
```

:::

---

## 查找像素

### find_pixel(color, x, y, width, height, tolerance?) / findPixel(color, x, y, width, height, tolerance?)

**说明**：在指定区域内查找第一个匹配指定颜色的像素。

**函数签名**：

```python
find_pixel(color: int, x: int, y: int, width: int, height: int, tolerance: int = 10) -> tuple[int, int] | None
```

```lua
findPixel(color: number, x: number, y: number, width: number, height: number, tolerance: number = 10) -> number, number | nil, nil
```

**参数**：
- `color` - 目标颜色（RGB 值）
- `x, y, width, height` - 搜索区域
- `tolerance` - 可选，颜色容差（0-255），默认 10

**返回**：
- Python: 找到时返回 (x, y) 元组，未找到返回 None
- Lua: 找到时返回 x, y，未找到返回 nil, nil

**颜色容差**：容差越大，允许的颜色差异越大。例如容差为 10 时，目标颜色为 0xFF0000，会匹配 0xF00000 到 0xFF0A0A 范围内的颜色。

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 在全屏范围内查找红色像素（默认容差 10）
result = screen.find_pixel(0xFF0000, 0, 0, 1920, 1080)
if result:
    x, y = result
    print(f"找到红色像素，位置: {x}, {y}")
else:
    print("未找到红色像素")

# 在指定区域查找，使用精确匹配（容差为 0）
result = screen.find_pixel(0x00FF00, 100, 100, 500, 500, 0)
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 在全屏范围内查找红色像素（默认容差 10）
local x, y = screen.findPixel(0xFF0000, 0, 0, 1920, 1080)
if x then
    print(string.format("找到红色像素，位置: %d, %d", x, y))
else
    print("未找到红色像素")
end

-- 在指定区域查找，使用精确匹配（容差为 0）
local x, y = screen.findPixel(0x00FF00, 100, 100, 500, 500, 0)
```

:::

---

## 查找所有匹配颜色

### find_color(color, x, y, width, height, tolerance?) / findColor(color, x, y, width, height, tolerance?)

**说明**：在指定区域内查找所有匹配指定颜色的像素。

**函数签名**：

```python
find_color(color: int, x: int, y: int, width: int, height: int, tolerance: int = 10) -> list[tuple[int, int]]
```

```lua
findColor(color: number, x: number, y: number, width: number, height: number, tolerance: number = 10) -> table[{x: number, y: number}]
```

**参数**：
- `color` - 目标颜色（RGB 值）
- `x, y, width, height` - 搜索区域
- `tolerance` - 可选，颜色容差，默认 10

**返回**：
- Python: 匹配像素的坐标列表 `[(x1, y1), (x2, y2), ...]`
- Lua: 匹配像素的坐标表格 `[{x=x1, y=y1}, {x=x2, y=y2}, ...]`

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 查找所有红色像素
results = screen.find_color(0xFF0000, 0, 0, 1920, 1080)
print(f"找到 {len(results)} 个红色像素")

# 遍历所有匹配的像素
for x, y in results:
    print(f"红色像素位置: {x}, {y}")
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 查找所有红色像素
local results = screen.findColor(0xFF0000, 0, 0, 1920, 1080)
print("找到 " .. #results .. " 个红色像素")

-- 遍历所有匹配的像素
for i, result in ipairs(results) do
    print(string.format("红色像素位置: %d, %d", result.x, result.y))
end
```

:::

---

## 图像匹配

### find_image(image_path, x, y, width, height, threshold?) / findImage(imagePath, x, y, width, height, threshold?)

**说明**：在屏幕指定区域中查找与模板图像匹配的位置。

**函数签名**：

```python
find_image(image_path: str, x: int, y: int, width: int, height: int, threshold: float = 0.9) -> tuple[int, int, float] | None
```

```lua
findImage(imagePath: string, x: number, y: number, width: number, height: number, threshold: number = 0.9) -> table{x: number, y: number, confidence: number} | nil
```

**参数**：
- `image_path` / `imagePath` - 模板图像文件路径（支持 PNG、JPG 等格式）
- `x, y, width, height` - 搜索区域
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.9

**返回**：
- Python: 找到时返回 (x, y, confidence) 元组，未找到返回 None
- Lua: 找到时返回 {x=x, y=y, confidence=confidence} 表格，未找到返回 nil

**匹配阈值**：值越高匹配越严格。0.9 表示 90% 相似度才认为匹配成功。

**模板图像要求**：
- 建议使用 PNG 格式（无损压缩）
- 模板应截取自实际运行环境
- 避免包含变化较大的元素（如动态文字、进度条）

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 在全屏范围内查找图像
result = screen.find_image("target.png", 0, 0, 1920, 1080)
if result:
    x, y, confidence = result
    print(f"找到图像，位置: {x}, {y}，相似度: {confidence:.2f}")
else:
    print("未找到图像")

# 在指定区域查找，使用较低的匹配阈值
result = screen.find_image("button.png", 100, 100, 500, 500, 0.8)
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 在全屏范围内查找图像
local result = screen.findImage("target.png", 0, 0, 1920, 1080)
if result then
    print(string.format("找到图像，位置: %d, %d，相似度: %f",
        result.x, result.y, result.confidence))
else
    print("未找到图像")
end

-- 在指定区域查找，使用较低的匹配阈值
local result = screen.findImage("button.png", 100, 100, 500, 500, 0.8)
```

:::

---

## 等待图像出现

### wait_for_image(image_path, timeout, threshold?) / waitForImage(imagePath, timeout, threshold?)

**说明**：等待指定图像在屏幕上出现。每隔一段时间检查一次，直到找到图像或超时。

**函数签名**：

```python
wait_for_image(image_path: str, timeout: int, threshold: float = 0.9) -> tuple[int, int, float] | None
```

```lua
waitForImage(imagePath: string, timeout: number, threshold: number = 0.9) -> table{x: number, y: number, confidence: number} | nil
```

**参数**：
- `image_path` / `imagePath` - 模板图像文件路径
- `timeout` - 超时时间（毫秒）
- `threshold` - 可选，匹配阈值，默认 0.9

**返回**：
- 找到时返回匹配结果（格式同 find_image）
- 超时未找到返回 None/nil

**使用场景**：
- 等待加载画面消失
- 等待按钮出现
- 等待状态变化

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 等待"加载完成"图像出现（最多等待 5 秒）
result = screen.wait_for_image("loaded.png", 5000)
if result:
    print("加载完成")
else:
    print("等待超时")

# 等待按钮出现后再点击
button = screen.wait_for_image("submit_button.png", 3000)
if button:
    x, y, _ = button
    # 点击找到的位置（需要 input 模块）
    # input.click(x, y)
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")

-- 等待"加载完成"图像出现（最多等待 5 秒）
local result = screen.waitForImage("loaded.png", 5000)
if result then
    print("加载完成")
else
    print("等待超时")
end

-- 等待按钮出现后再点击
local button = screen.waitForImage("submit_button.png", 3000)
if button then
    local x, y = button.x, button.y
    -- 点击找到的位置（需要 input 模块）
    -- input.click(x, y)
end
```

:::

---

## 完整示例

### 游戏挂机脚本

这个示例展示了如何使用 screen 模块实现一个简单的游戏挂机脚本：

:::tabs

== Python

```python:line-numbers
from wingman import screen, input, util

def game_bot():
    """简单的游戏挂机脚本"""

    # 定义血量区域（假设在屏幕左上角）
    hp_x, hp_y = 100, 50
    hp_width, hp_height = 100, 20

    # 定义怪物的图像模板
    monster_image = "monster.png"

    while True:
        # 1. 检查血量
        hp_region = screen.capture(hp_x, hp_y, hp_width, hp_height)
        # 获取血量区域的第一个像素来判断颜色
        hp_color = screen.get_pixel(hp_x + 10, hp_y + 10)

        # 如果血量颜色变暗（表示血量低），使用补血
        if hp_color < 0xFF0000:  # 假设 0xFF0000 是满血红色
            print("血量低，使用补血...")
            # 点击补血按钮（假设位置在 200, 300）
            input.click(200, 300)
            util.sleep(1000)  # 等待补血动画

        # 2. 查找怪物
        monster = screen.find_image(monster_image, 0, 0, 1920, 1080)
        if monster:
            x, y, confidence = monster
            print(f"发现怪物，位置: {x}, {y}，相似度: {confidence:.2f}")
            # 点击怪物进行攻击
            input.click(x, y)
            util.sleep(500)  # 攻击后冷却
        else:
            print("未发现怪物，等待...")
            util.sleep(1000)  # 没有怪物时等待

        # 防止脚本过快运行
        util.sleep(100)

# 运行脚本（注意：这是无限循环，需要手动停止）
# game_bot()
```

== Lua

```lua:line-numbers
local screen = require("wingman.screen")
local input = require("wingman.input")
local util = require("wingman.util")

local function gameBot()
    -- 定义血量区域（假设在屏幕左上角）
    local hpX, hpY = 100, 50
    local hpWidth, hpHeight = 100, 20

    -- 定义怪物的图像模板
    local monsterImage = "monster.png"

    while true do
        -- 1. 检查血量
        local hpColor = screen.getPixel(hpX + 10, hpY + 10)

        -- 如果血量颜色变暗（表示血量低），使用补血
        if hpColor < 0xFF0000 then
            print("血量低，使用补血...")
            -- 点击补血按钮（假设位置在 200, 300）
            input.click(200, 300)
            util.sleep(1000)  -- 等待补血动画
        end

        -- 2. 查找怪物
        local monster = screen.findImage(monsterImage, 0, 0, 1920, 1080)
        if monster then
            print(string.format("发现怪物，位置: %d, %d，相似度: %f",
                monster.x, monster.y, monster.confidence))
            -- 点击怪物进行攻击
            input.click(monster.x, monster.y)
            util.sleep(500)  -- 攻击后冷却
        else
            print("未发现怪物，等待...")
            util.sleep(1000)  -- 没有怪物时等待
        end

        -- 防止脚本过快运行
        util.sleep(100)
    end
end

-- 运行脚本（注意：这是无限循环，需要手动停止）
-- gameBot()
```

:::

---

## 可用接口

### 截图操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `capture(x, y, w, h)` | `capture(x, y, w, h)` | 截取屏幕区域 | x,y: 左上角坐标<br>w,h: 宽高 |

### 像素操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get_pixel(x, y)` | `getPixel(x, y)` | 获取像素颜色 | x,y: 坐标<br>返回: RGB 颜色值 |
| `find_pixel(color, x, y, w, h, tol?)` | `findPixel(color, x, y, w, h, tol?)` | 查找单个像素 | color: 目标颜色<br>x,y,w,h: 搜索区域<br>tol: 容差(默认10) |
| `find_color(color, x, y, w, h, tol?)` | `findColor(color, x, y, w, h, tol?)` | 查找所有像素 | 同上，返回所有匹配 |

### 图像操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_image(path, x, y, w, h, thr?)` | `findImage(path, x, y, w, h, thr?)` | 查找图像 | path: 图像路径<br>x,y,w,h: 搜索区域<br>thr: 匹配阈值(默认0.9) |
| `wait_for_image(path, timeout, thr?)` | `waitForImage(path, timeout, thr?)` | 等待图像出现 | path: 图像路径<br>timeout: 超时(ms)<br>thr: 匹配阈值(默认0.9) |
