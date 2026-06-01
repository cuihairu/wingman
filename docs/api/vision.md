# API: wingman.vision

视觉模块，提供屏幕检测、图像匹配、形状识别等功能。

## 模块概述

vision 模块提供屏幕视觉检测功能：
- **颜色检测** - 查找颜色、检查颜色存在、获取主要颜色
- **图像匹配** - 查找图像模板
- **形状识别** - 检测边缘、轮廓、圆形
- **屏幕截图** - 截取指定区域保存为图片

---

## 查找颜色

### find_color(color, tolerance?, region?) / findColor(color, tolerance?, region?)

**说明**：查找指定颜色在屏幕中的第一个位置。

**函数签名**：

```python
find_color(color: dict | int, tolerance: int = 0, region: dict = None) -> dict | None
```

```lua
findColor(color: table | number, tolerance: number = 0, region: table = nil) -> table | nil
```

**参数**：
- `color` - 颜色值，支持 `{r: number, g: number, b: number}` 或整数 `0xRRGGBB`
- `tolerance` - 可选，容差值（0-255），默认 0
- `region` - 可选，搜索区域 `{x: number, y: number, width: number, height: number}`，默认全屏

**返回**：
- 找到时返回 `{x: number, y: number}`
- 未找到时返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import vision

# 查找红色
pos = vision.find_color({"r": 255, "g": 0, "b": 0}, 10)
if pos:
    print(f"找到颜色: {pos['x']}, {pos['y']}")

# 使用十六进制颜色
pos = vision.find_color(0xFF0000, 10)
```

== Lua

```lua:line-numbers
local vision = require("wingman.vision")

-- 查找红色
local pos = vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    print("找到颜色:", pos.x, pos.y)
end

-- 使用十六进制颜色
local pos = vision.findColor(0xFF0000, 10)
```

:::

---

## 查找所有颜色

### find_all_colors(color, tolerance?, region?) / findAllColors(color, tolerance?, region?)

**说明**：查找所有匹配颜色的位置。

**函数签名**：

```python
find_all_colors(color: dict | int, tolerance: int = 0, region: dict = None) -> list[dict]
```

```lua
findAllColors(color: table | number, tolerance: number = 0, region: table = nil) -> table
```

**参数**：
- `color` - 颜色值，支持字典 `{r, g, b}` 或整数
- `tolerance` - 可选，容差值（0-255），默认 0
- `region` - 可选，搜索区域，默认全屏

**返回**：
- Python: 列表 `[{x: number, y: number}, ...]`
- Lua: 数组 `[{x: number, y: number}, ...]`

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

---

## 检查颜色存在

### has_color(color, tolerance?, region?) / hasColor(color, tolerance?, region?)

**说明**：检查区域内是否包含指定颜色。

**函数签名**：

```python
has_color(color: dict | int, tolerance: int = 0, region: dict = None) -> bool
```

```lua
hasColor(color: table | number, tolerance: number = 0, region: table = nil) -> boolean
```

**参数**：
- `color` - 颜色值
- `tolerance` - 可选，容差值（0-255），默认 0
- `region` - 可选，搜索区域，默认全屏

**返回**：
- 是否找到颜色

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

---

## 获取主要颜色

### get_dominant_color(region?) / getDominantColor(region?)

**说明**：获取区域内主要颜色（出现次数最多的颜色）。

**函数签名**：

```python
get_dominant_color(region: dict = None) -> dict
```

```lua
getDominantColor(region: table = nil) -> table
```

**参数**：
- `region` - 可选，分析区域，默认全屏

**返回**：
- 颜色对象 `{r: number, g: number, b: number, a: number}`

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

---

## 查找图像

### find_image(template_path, threshold?, region?) / findImage(templatePath, threshold?, region?)

**说明**：查找图像模板在屏幕中的位置。

**函数签名**：

```python
find_image(template_path: str, threshold: float = 0.8, region: dict = None) -> dict
```

```lua
findImage(templatePath: string, threshold: number = 0.8, region: table = nil) -> table
```

**参数**：
- `template_path` / `templatePath` - 模板图片路径
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.8
- `region` - 可选，搜索区域，默认全屏

**返回**：
- 匹配结果对象：
  - `found` / `found` - 是否找到
  - `position` / `position` - 位置 `{x: number, y: number}`
  - `confidence` / `confidence` - 置信度（0-1）

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

---

## 检测边缘

### detect_edges(region?, threshold1?, threshold2?) / detectEdges(region?, threshold1?, threshold2?)

**说明**：使用 Canny 算子检测边缘。

**函数签名**：

```python
detect_edges(region: dict = None, threshold1: float = 50, threshold2: float = 150) -> list[dict]
```

```lua
detectEdges(region: table = nil, threshold1: number = 50, threshold2: number = 150) -> table
```

**参数**：
- `region` - 可选，检测区域，默认全屏
- `threshold1` - 可选，Canny 第一个阈值，默认 50
- `threshold2` - 可选，Canny 第二个阈值，默认 150

**返回**：
- 边缘点数组 `[{x: number, y: number}, ...]`

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

---

## 检测轮廓

### detect_contours(region?) / detectContours(region?)

**说明**：检测区域内的轮廓。

**函数签名**：

```python
detect_contours(region: dict = None) -> list[list[dict]]
```

```lua
detectContours(region: table = nil) -> table
```

**参数**：
- `region` - 可选，检测区域，默认全屏

**返回**：
- 轮廓数组，每个轮廓是点数组 `[[{x, y}, ...], ...]`

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

---

## 检测圆形

### detect_circles(region?, min_radius?, max_radius?) / detectCircles(region?, minRadius?, maxRadius?)

**说明**：检测区域内的圆形。

**函数签名**：

```python
detect_circles(region: dict = None, min_radius: int = 10, max_radius: int = 100) -> list[dict]
```

```lua
detectCircles(region: table = nil, minRadius: number = 10, maxRadius: number = 100) -> table
```

**参数**：
- `region` - 可选，检测区域，默认全屏
- `min_radius` / `minRadius` - 可选，最小半径，默认 10
- `max_radius` / `maxRadius` - 可选，最大半径，默认 100

**返回**：
- 圆形数组，每个圆形包含：
  - `x` / `x` - 圆心 X 坐标
  - `y` / `y` - 圆心 Y 坐标
  - `radius` / `radius` - 半径

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

---

## 截取区域

### capture_region(region, output_path) / captureRegion(region, outputPath)

**说明**：截取屏幕区域并保存为图片。

**函数签名**：

```python
capture_region(region: dict, output_path: str) -> None
```

```lua
captureRegion(region: table, outputPath: string) -> nil
```

**参数**：
- `region` - 截取区域 `{x: number, y: number, width: number, height: number}`
- `output_path` / `outputPath` - 输出文件路径

**返回**：
- 无

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

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_color(color, tolerance?, region?)` | `findColor(color, tolerance?, region?)` | 查找颜色 | color: 颜色值<br>tolerance: 容差(默认0)<br>region: 搜索区域(可选)<br>返回: 位置对象或None/nil |
| `find_all_colors(color, tolerance?, region?)` | `findAllColors(color, tolerance?, region?)` | 查找所有颜色 | 返回: 位置数组 |
| `has_color(color, tolerance?, region?)` | `hasColor(color, tolerance?, region?)` | 检查颜色存在 | 返回: 是否存在 |
| `get_dominant_color(region?)` | `getDominantColor(region?)` | 获取主要颜色 | region: 分析区域(可选)<br>返回: 颜色对象RGBA |
| `find_image(path, threshold?, region?)` | `findImage(path, threshold?, region?)` | 查找图像 | path: 模板路径<br>threshold: 匹配阈值(默认0.8)<br>region: 搜索区域(可选)<br>返回: 匹配结果对象 |
| `detect_edges(region?, threshold1?, threshold2?)` | `detectEdges(region?, threshold1?, threshold2?)` | 检测边缘 | region: 检测区域(可选)<br>threshold1: Canny阈值1(默认50)<br>threshold2: Canny阈值2(默认150)<br>返回: 边缘点数组 |
| `detect_contours(region?)` | `detectContours(region?)` | 检测轮廓 | region: 检测区域(可选)<br>返回: 轮廓数组 |
| `detect_circles(region?, minRadius?, maxRadius?)` | `detectCircles(region?, minRadius?, maxRadius?)` | 检测圆形 | region: 检测区域(可选)<br>minRadius: 最小半径(默认10)<br>maxRadius: 最大半径(默认100)<br>返回: 圆形数组 |
| `capture_region(region, path)` | `captureRegion(region, path)` | 截取区域 | region: 截取区域<br>path: 输出路径 |
