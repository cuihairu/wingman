# API: wingman.screen

屏幕操作模块，提供屏幕截图、像素颜色获取、颜色查找、图像匹配等功能。

## 模块概述

screen 模块是 Wingman 的核心模块之一，用于与屏幕进行交互。主要功能包括：

- **屏幕截图**：截取全屏或指定区域
- **像素操作**：获取像素颜色、查找指定颜色的像素
- **图像匹配**：在屏幕中查找图像模板
- **屏幕尺寸**：获取屏幕宽高

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

### 区域对象（region）

许多函数接受一个 `region` 对象来限定操作范围，结构为：

- `x` - 区域左上角 X 坐标
- `y` - 区域左上角 Y 坐标
- `width` - 区域宽度
- `height` - 区域高度

**Python**: `{x, y, width, height}` 字典
**Lua**: `{x=, y=, width=, height=}` 表格

### 颜色值

颜色以 RGB 形式传递。`findColor` / `findColors` 的 `color` 参数使用 `0xRRGGBB` 整数表示，例如 `0xFF0000` 表示纯红。

---

## 截取屏幕

### capture()

**说明**：截取全屏，返回是否截取成功。

**函数签名**：

```python
capture() -> bool
```

```lua
capture() -> boolean
```

**返回**：截取成功返回 `true`，失败返回 `false`。

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 截取全屏
ok = screen.capture()
if ok:
    print("截屏成功")
else:
    print("截屏失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 截取全屏
local ok = wingman.screen.capture()
if ok then
    print("截屏成功")
else
    print("截屏失败")
end
```

:::

---

## 截取指定区域

### captureRegion(region) / capture_region(region)

**说明**：截取屏幕的指定区域，返回是否截取成功。

**函数签名**：

```python
captureRegion(region: dict) -> bool
```

```lua
captureRegion(region: table) -> boolean
```

**参数**：
- `region` - 区域对象 `{x, y, width, height}`

**返回**：截取成功返回 `true`，失败返回 `false`。

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 截取从 (100,100) 开始，宽 400 高 300 的区域
ok = screen.captureRegion({"x": 100, "y": 100, "width": 400, "height": 300})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 截取从 (100,100) 开始，宽 400 高 300 的区域
local ok = wingman.screen.captureRegion({x=100, y=100, width=400, height=300})
```

:::

---

## 获取像素颜色

### getPixel(x, y)

**说明**：获取指定坐标的像素颜色，返回 RGBA 分量对象。

**函数签名**：

```python
getPixel(x: int, y: int) -> dict
```

```lua
getPixel(x: number, y: number) -> table
```

**参数**：
- `x, y` - 像素坐标

**返回**：包含 `r, g, b, a` 四个分量的对象（每个分量 0-255）：
- `r` - 红色分量
- `g` - 绿色分量
- `b` - 蓝色分量
- `a` - Alpha 分量

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 获取 (100, 100) 位置的像素颜色
color = screen.getPixel(100, 100)
print(f"R={color['r']}, G={color['g']}, B={color['b']}, A={color['a']}")

# 检查是否为红色
if color["r"] == 255 and color["g"] == 0 and color["b"] == 0:
    print("这是红色像素")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取 (100, 100) 位置的像素颜色
local color = wingman.screen.getPixel(100, 100)
print(string.format("R=%d, G=%d, B=%d, A=%d", color.r, color.g, color.b, color.a))

-- 检查是否为红色
if color.r == 255 and color.g == 0 and color.b == 0 then
    print("这是红色像素")
end
```

:::

---

## 查找颜色（首个匹配）

### findColor(color, region?, tolerance=10)

**说明**：在指定区域内查找第一个匹配指定颜色的像素。

**函数签名**：

```python
findColor(color: int, region: dict = None, tolerance: int = 10) -> dict
```

```lua
findColor(color: number, region: table = nil, tolerance: number = 10) -> table
```

**参数**：
- `color` - 目标颜色（`0xRRGGBB` 整数）
- `region` - 可选，搜索区域 `{x, y, width, height}`，省略时搜索全屏
- `tolerance` - 可选，颜色容差（0-255），默认 10

**返回**：包含 `point` 和 `found` 的对象：
- `point` - 匹配点坐标 `{x, y}`（未找到时为空）
- `found` - 是否找到匹配

**颜色容差**：容差越大，允许的颜色差异越大。

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 在全屏范围内查找红色像素（默认容差 10）
result = screen.findColor(0xFF0000)
if result["found"]:
    p = result["point"]
    print(f"找到红色像素，位置: {p['x']}, {p['y']}")
else:
    print("未找到红色像素")

# 在指定区域查找，精确匹配（容差为 0）
result = screen.findColor(
    0x00FF00,
    {"x": 100, "y": 100, "width": 500, "height": 500},
    0,
)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 在全屏范围内查找红色像素（默认容差 10）
local result = wingman.screen.findColor(0xFF0000)
if result.found then
    print(string.format("找到红色像素，位置: %d, %d", result.point.x, result.point.y))
else
    print("未找到红色像素")
end

-- 在指定区域查找，精确匹配（容差为 0）
local result = wingman.screen.findColor(
    0x00FF00,
    {x=100, y=100, width=500, height=500},
    0
)
```

:::

---

## 查找颜色（所有匹配）

### findColors(color, region?, tolerance)

**说明**：在指定区域内查找所有匹配指定颜色的像素。

**函数签名**：

```python
findColors(color: int, region: dict = None, tolerance: int = 10) -> list[dict]
```

```lua
findColors(color: number, region: table = nil, tolerance: number = 10) -> table
```

**参数**：
- `color` - 目标颜色（`0xRRGGBB` 整数）
- `region` - 可选，搜索区域 `{x, y, width, height}`，省略时搜索全屏
- `tolerance` - 可选，颜色容差（0-255），默认 10

**返回**：匹配点坐标数组 `[{x, y}, ...]`。

:::tabs

== Python

```python:line-numbers
from wingman import screen

# 查找所有红色像素
results = screen.findColors(0xFF0000)
print(f"找到 {len(results)} 个红色像素")

# 遍历所有匹配的像素
for p in results:
    print(f"红色像素位置: {p['x']}, {p['y']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找所有红色像素
local results = wingman.screen.findColors(0xFF0000)
print("找到 " .. #results .. " 个红色像素")

-- 遍历所有匹配的像素
for i, p in ipairs(results) do
    print(string.format("红色像素位置: %d, %d", p.x, p.y))
end
```

:::

---

## 图像匹配

### findImage(imagePath, region?, threshold=0.9)

**说明**：在屏幕指定区域中查找与模板图像匹配的位置。

**函数签名**：

```python
findImage(imagePath: str, region: dict = None, threshold: float = 0.9) -> dict
```

```lua
findImage(imagePath: string, region: table = nil, threshold: number = 0.9) -> table
```

**参数**：
- `imagePath` - 模板图像文件路径（支持 PNG、JPG 等格式）
- `region` - 可选，搜索区域 `{x, y, width, height}`，省略时搜索全屏
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.9

**返回**：包含 `point`、`found` 和 `region` 的对象：
- `point` - 匹配位置坐标 `{x, y}`（未找到时为空）
- `found` - 是否找到匹配
- `region` - 匹配到的区域 `{x, y, width, height}`

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
result = screen.findImage("target.png")
if result["found"]:
    p = result["point"]
    print(f"找到图像，位置: {p['x']}, {p['y']}")
else:
    print("未找到图像")

# 在指定区域查找，使用较低的匹配阈值
result = screen.findImage(
    "button.png",
    {"x": 100, "y": 100, "width": 500, "height": 500},
    0.8,
)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 在全屏范围内查找图像
local result = wingman.screen.findImage("target.png")
if result.found then
    print(string.format("找到图像，位置: %d, %d", result.point.x, result.point.y))
else
    print("未找到图像")
end

-- 在指定区域查找，使用较低的匹配阈值
local result = wingman.screen.findImage(
    "button.png",
    {x=100, y=100, width=500, height=500},
    0.8
)
```

:::

---

## 获取屏幕尺寸

### getScreenWidth() / getScreenHeight()

**说明**：获取屏幕的宽度或高度（像素）。

**函数签名**：

```python
getScreenWidth() -> int
getScreenHeight() -> int
```

```lua
getScreenWidth() -> number
getScreenHeight() -> number
```

**返回**：屏幕宽度或高度（像素）。

:::tabs

== Python

```python:line-numbers
from wingman import screen

w = screen.getScreenWidth()
h = screen.getScreenHeight()
print(f"屏幕尺寸: {w}x{h}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local w = wingman.screen.getScreenWidth()
local h = wingman.screen.getScreenHeight()
print(string.format("屏幕尺寸: %dx%d", w, h))
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

    # 定义怪物的图像模板
    monster_image = "monster.png"

    while True:
        # 1. 检查血量：获取血量区域内的一个像素
        hp_color = screen.getPixel(hp_x + 10, hp_y + 10)

        # 如果血量颜色变暗（表示血量低），使用补血
        if hp_color["r"] < 0xFF:
            print("血量低，使用补血...")
            input.click(200, 300, 0)
            util.sleep(1000)  # 等待补血动画

        # 2. 查找怪物
        monster = screen.findImage(monster_image)
        if monster["found"]:
            p = monster["point"]
            print(f"发现怪物，位置: {p['x']}, {p['y']}")
            input.click(p["x"], p["y"], 0)
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
local wingman = require("wingman")

local function gameBot()
    -- 定义血量区域（假设在屏幕左上角）
    local hpX, hpY = 100, 50

    -- 定义怪物的图像模板
    local monsterImage = "monster.png"

    while true do
        -- 1. 检查血量：获取血量区域内的一个像素
        local hpColor = wingman.screen.getPixel(hpX + 10, hpY + 10)

        -- 如果血量颜色变暗（表示血量低），使用补血
        if hpColor.r < 0xFF then
            print("血量低，使用补血...")
            wingman.input.click(200, 300, 0)
            wingman.util.sleep(1000)  -- 等待补血动画
        end

        -- 2. 查找怪物
        local monster = wingman.screen.findImage(monsterImage)
        if monster.found then
            print(string.format("发现怪物，位置: %d, %d",
                monster.point.x, monster.point.y))
            wingman.input.click(monster.point.x, monster.point.y, 0)
            wingman.util.sleep(500)  -- 攻击后冷却
        else
            print("未发现怪物，等待...")
            wingman.util.sleep(1000)  -- 没有怪物时等待
        end

        -- 防止脚本过快运行
        wingman.util.sleep(100)
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
| `capture()` | `capture()` | 截取全屏 | 返回: 是否成功 |
| `captureRegion(region)` | `captureRegion(region)` | 截取指定区域 | region: {x,y,width,height}<br>返回: 是否成功 |

### 像素操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `getPixel(x, y)` | `getPixel(x, y)` | 获取像素颜色 | x,y: 坐标<br>返回: {r,g,b,a} |
| `findColor(color, region?, tolerance=10)` | `findColor(color, region?, tolerance=10)` | 查找首个匹配像素 | color: 目标颜色<br>region: 搜索区域<br>tolerance: 容差(默认10)<br>返回: {point, found} |
| `findColors(color, region?, tolerance)` | `findColors(color, region?, tolerance)` | 查找所有匹配像素 | 同 findColor<br>返回: 点数组 |

### 图像操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `findImage(imagePath, region?, threshold=0.9)` | `findImage(imagePath, region?, threshold=0.9)` | 查找图像 | imagePath: 图像路径<br>region: 搜索区域<br>threshold: 匹配阈值(默认0.9)<br>返回: {point, found, region} |

### 屏幕尺寸

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `getScreenWidth()` | `getScreenWidth()` | 获取屏幕宽度 | 返回: 宽度(像素) |
| `getScreenHeight()` | `getScreenHeight()` | 获取屏幕高度 | 返回: 高度(像素) |
