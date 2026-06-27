# API: wingman.vision

视觉模块，提供屏幕检测、图像匹配、形状识别等功能。

## 模块概述

vision 模块提供屏幕视觉检测功能：
- **颜色检测** - 查找颜色、检查颜色存在、获取主要颜色
- **图像匹配** - 查找图像模板

---

## 查找颜色

### find_color(color, tolerance?, region?) / findColor(color, tolerance?, region?)

**说明**：查找指定颜色在屏幕中的第一个位置。

**函数签名**：

```python
find_color(color: dict | int, tolerance: int = 10, region: dict = None) -> dict | None
```

```lua
findColor(color: table | number, tolerance: number = 10, region: table = nil) -> table | nil
```

**参数**：
- `color` - 颜色值，支持 `{r: number, g: number, b: number}` 或整数 `0xRRGGBB`
- `tolerance` - 可选，容差值（0-255），默认 10
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
local wingman = require("wingman")

-- 查找红色
local pos = wingman.vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    print("找到颜色:", pos.x, pos.y)
end

-- 使用十六进制颜色
local pos = wingman.vision.findColor(0xFF0000, 10)
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
local wingman = require("wingman")

local positions = wingman.vision.findAllColors({r=255, g=0, b=0}, 5)
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
local wingman = require("wingman")

if wingman.vision.hasColor({r=0, g=255, b=0}, 10) then
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
local wingman = require("wingman")

local color = wingman.vision.getDominantColor({x=0, y=0, width=200, height=200})
print("主要颜色:", color.r, color.g, color.b)
```

:::

---

## 查找图像

### find_image(template_path, threshold?, region?) / findImage(templatePath, threshold?, region?)

**说明**：查找图像模板在屏幕中的位置。

**函数签名**：

```python
find_image(template_path: str, threshold: float = 0.9, region: dict = None) -> dict
```

```lua
findImage(templatePath: string, threshold: number = 0.9, region: table = nil) -> table
```

**参数**：
- `template_path` / `templatePath` - 模板图片路径
- `threshold` - 可选，匹配阈值（0.0-1.0），默认 0.9
- `region` - 可选，搜索区域，默认全屏

**返回**：
- 匹配结果对象：
  - `found` / `found` - 是否找到
  - `position` / `position` - 位置 `{x: number, y: number}`
  - `confidence` / `confidence` - 置信度（0-1）
  - `region` / `region` - 匹配区域 `{x: number, y: number, width: number, height: number}`

:::tabs

== Python

```python:line-numbers
from wingman import vision

result = vision.find_image("target.png", 0.9)
if result['found']:
    print(f"找到图像: {result['position']['x']}, {result['position']['y']}")
    print(f"置信度: {result['confidence']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local result = wingman.vision.findImage("target.png", 0.9)
if result.found then
    print("找到图像:", result.position.x, result.position.y)
    print("置信度:", result.confidence)
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_color(color, tolerance?, region?)` | `findColor(color, tolerance?, region?)` | 查找颜色 | color: 颜色值<br>tolerance: 容差(默认10)<br>region: 搜索区域(可选)<br>返回: 位置对象或None/nil |
| `find_all_colors(color, tolerance?, region?)` | `findAllColors(color, tolerance?, region?)` | 查找所有颜色 | 返回: 位置数组 |
| `has_color(color, tolerance?, region?)` | `hasColor(color, tolerance?, region?)` | 检查颜色存在 | 返回: 是否存在 |
| `get_dominant_color(region?)` | `getDominantColor(region?)` | 获取主要颜色 | region: 分析区域(可选)<br>返回: 颜色对象RGBA |
| `find_image(path, threshold?, region?)` | `findImage(path, threshold?, region?)` | 查找图像 | path: 模板路径<br>threshold: 匹配阈值(默认0.9)<br>region: 搜索区域(可选)<br>返回: 匹配结果对象 |
