# API: wingman.vision

视觉模块提供屏幕检测、图像匹配、形状识别等功能。

## 查找颜色

:::tabs

== Python

```python:line-numbers
from wingman import vision

# 查找红色
pos = vision.find_color({"r": 255, "g": 0, "b": 0}, 10)
if pos:
    print(f"找到颜色: {pos['x']}, {pos['y']}")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

-- 查找红色
local pos = vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    print("找到颜色:", pos.x, pos.y)
end
```

:::

## 查找所有颜色

:::tabs

== Python

```python:line-numbers
from wingman import vision

positions = vision.find_all_colors({"r": 255, "g": 0, "b": 0}, 5)
for i, pos in enumerate(positions):
    print(f"位置 {i}: {pos['x']}, {pos['y']}")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local positions = vision.findAllColors({r=255, g=0, b=0}, 5)
for i, pos in ipairs(positions) do
    print("位置", i, ":", pos.x, pos.y)
end
```

:::

## 检查颜色存在

:::tabs

== Python

```python:line-numbers
from wingman import vision

if vision.has_color({"r": 0, "g": 255, "b": 0}, 10):
    print("找到了绿色")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

if vision.hasColor({r=0, g=255, b=0}, 10) then
    print("找到了绿色")
end
```

:::

## 获取主要颜色

:::tabs

== Python

```python:line-numbers
from wingman import vision

color = vision.get_dominant_color({"x": 0, "y": 0, "width": 200, "height": 200})
print(f"主要颜色: {color['r']}, {color['g']}, {color['b']}")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local color = vision.getDominantColor({x=0, y=0, width=200, height=200})
print("主要颜色:", color.r, color.g, color.b)
```

:::

## 查找图像

:::tabs

== Python

```python:line-numbers
from wingman import vision

result = vision.find_image("target.png", 0.8)
if result['found']:
    print(f"找到图像: {result['position']['x']}, {result['position']['y']}")
    print(f"置信度: {result['confidence']}")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local result = vision.findImage("target.png", 0.8)
if result.found then
    print("找到图像:", result.position.x, result.position.y)
    print("置信度:", result.confidence)
end
```

:::

## 检测边缘

:::tabs

== Python

```python:line-numbers
from wingman import vision

edges = vision.detect_edges({"x": 0, "y": 0, "width": 800, "height": 600}, 50, 150)
print(f"检测到 {len(edges)} 个边缘点")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local edges = vision.detectEdges({x=0, y=0, width=800, height=600}, 50, 150)
print("检测到", #edges, "个边缘点")
```

:::

## 检测轮廓

:::tabs

== Python

```python:line-numbers
from wingman import vision

contours = vision.detect_contours({"x": 0, "y": 0, "width": 800, "height": 600})
for i, contour in enumerate(contours):
    print(f"轮廓 {i}: {len(contour)} 个点")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local contours = vision.detectContours({x=0, y=0, width=800, height=600})
for i, contour in ipairs(contours) do
    print("轮廓", i, "有", #contour, "个点")
end
```

:::

## 检测圆形

:::tabs

== Python

```python:line-numbers
from wingman import vision

circles = vision.detect_circles({"x": 0, "y": 0, "width": 800, "height": 600}, 10, 100)
for i, circle in enumerate(circles):
    print(f"圆 {i}: center({circle['x']}, {circle['y']}) radius={circle['radius']}")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

local circles = vision.detectCircles({x=0, y=0, width=800, height=600}, 10, 100)
for i, circle in ipairs(circles) do
    print("圆", i, ": center(", circle.x, circle.y, ") radius=", circle.radius)
end
```

:::

## 截取区域

:::tabs

== Python

```python:line-numbers
from wingman import vision

vision.capture_region({"x": 0, "y": 0, "width": 1920, "height": 1080}, "screenshot.png")
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

vision.captureRegion({x=0, y=0, width=1920, height=1080}, "screenshot.png")
```

:::

---

## 可用接口

### `find_color(color, tolerance?, region?)` / `findColor(...)`

查找指定颜色的位置。

**参数：**
- `color` - 颜色值 `{r, g, b}` 或整数 `0xRRGGBB`
- `tolerance` - 容差值，默认 0
- `region` - 搜索区域 `{x, y, width, height}`，默认全屏

**返回：** `{x, y}` 或 `None`/`nil`

### `find_all_colors(color, tolerance?, region?)` / `findAllColors(...)`

查找所有匹配颜色的位置。

**返回：** 列表/数组 `[{x, y}, ...]`

### `has_color(color, tolerance?, region?)` / `hasColor(...)`

检查区域内是否包含指定颜色。

**返回：** `boolean`

### `get_dominant_color(region?)` / `getDominantColor(...)`

获取区域内主要颜色（众数）。

**返回：** `{r, g, b, a}`

### `find_image(template_path, threshold?, region?)` / `findImage(...)`

查找图像模板在屏幕中的位置。

**参数：**
- `template_path` / `templatePath` - 模板图片路径
- `threshold` - 匹配阈值 0.0-1.0，默认 0.8
- `region` - 搜索区域，默认全屏

**返回：** `{found: boolean, position: {x, y}, confidence: number}`

### `detect_edges(region?, threshold1?, threshold2?)` / `detectEdges(...)`

使用 Canny 算子检测边缘。

**返回：** `[{x, y}, ...]`

### `detect_contours(region?)` / `detectContours(...)`

检测轮廓。

### `detect_circles(region?, min_radius?, max_radius?)` / `detectCircles(...)`

检测圆形。

### `capture_region(region, output_path)` / `captureRegion(...)`

截取屏幕区域并保存为图片。
