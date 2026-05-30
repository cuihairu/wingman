# API: wingman.screen

屏幕操作模块。

## 截取屏幕

<CodeTabs>

:::slot python

```python
# 截取全屏
img = screen.capture(0, 0, 1920, 1080)

# 截取指定区域
img = screen.capture(100, 100, 500, 500)
```

:::

:::slot lua

```lua
-- 截取全屏
local img = screen.capture(0, 0, 1920, 1080)

-- 截取指定区域
local img = screen.capture(100, 100, 500, 500)
```

:::

</CodeTabs>

## 获取像素颜色

<CodeTabs>

:::slot python

```python
color = screen.get_pixel(100, 100)
print(f"Color: 0x{color:06X}")
```

:::

:::slot lua

```lua
local color = screen.getPixel(100, 100)
print(string.format("Color: 0x%06X", color))
```

:::

</CodeTabs>

## 查找像素

<CodeTabs>

:::slot python

```python
# 查找单个像素
result = screen.find_pixel(0xFF0000, 0, 0, 1920, 1080, 10)
if result:
    x, y = result
    print(f"Found at: {x}, {y}")
```

:::

:::slot lua

```lua
-- 查找单个像素
local x, y = screen.findPixel(0xFF0000, 0, 0, 1920, 1080, 10)
if x then
    print(string.format("Found at: %d, %d", x, y))
end
```

:::

</CodeTabs>

## 查找颜色（多点）

<CodeTabs>

:::slot python

```python
points = screen.find_color(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    for point in points:
        print(f"Found: {point['x']}, {point['y']}")
```

:::

:::slot lua

```lua
local points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for _, point in ipairs(points) do
        print(string.format("Found: %d, %d", point.x, point.y))
    end
end
```

:::

</CodeTabs>

## 查找图像

<CodeTabs>

:::slot python

```python
result = screen.find_image("template.png", 0.9)
if result:
    x, y = result
    input.click(x, y)
```

:::

:::slot lua

```lua
local x, y = screen.findImage("template.png", 0.9)
if x then
    input.click(x, y)
end
```

:::

</CodeTabs>

## 获取屏幕尺寸

<CodeTabs>

:::slot python

```python
w, h = screen.get_size()
print(f"Screen: {w}x{h}")
```

:::

:::slot lua

```lua
local w, h = screen.getSize()
print(string.format("Screen: %dx%d", w, h))
```

:::

</CodeTabs>

---

## 可用接口

### `capture(x, y, w, h)` / `capture(x, y, w, h)`

截取屏幕指定区域。

**参数：**
- `x` (number) - 起始 X 坐标
- `y` (number) - 起始 Y 坐标
- `w` (number) - 宽度
- `h` (number) - 高度

**返回：**
- `Image` - 图像对象

### `get_pixel(x, y)` / `getPixel(x, y)`

获取指定位置的像素颜色。

**参数：**
- `x` (number) - X 坐标
- `y` (number) - Y 坐标

**返回：**
- `number` - 颜色值 (0xRRGGBB)

### `find_pixel(color, x1, y1, x2, y2, tolerance)` / `findPixel(...)`

在指定区域内查找像素。

**参数：**
- `color` (number) - 目标颜色 (0xRRGGBB)
- `x1, y1` (number) - 搜索区域左上角
- `x2, y2` (number) - 搜索区域右下角
- `tolerance` (number) - 颜色容差 (0-255)

**返回：**
- `(number, number)` | `None` / `nil` - 找到的位置 x, y，未找到返回 nil

### `find_color(color, x1, y1, x2, y2, tolerance)` / `findColor(...)`

在指定区域内查找颜色（支持多点）。

**参数：**
- `color` (number) - 目标颜色 (0xRRGGBB)
- `x1, y1` (number) - 搜索区域左上角
- `x2, y2` (number) - 搜索区域右下角
- `tolerance` (number) - 颜色容差 (0-255)

**返回：**
- `list[dict]` | `None` / `table` | `nil` - 找到的点列表 `[{"x": x, "y": y}, ...]`，未找到返回 nil

### `find_image(template, threshold)` / `findImage(...)`

在屏幕中查找图像。

**参数：**
- `template` (Image|string) - 模板图像对象或文件路径
- `threshold` (number) - 匹配阈值 (0.0-1.0)

**返回：**
- `(number, number)` | `None` / `nil` - 找到的位置 x, y，未找到返回 nil

### `get_size()` / `getSize()`

获取屏幕尺寸。

**返回：**
- `(number, number)` - 宽度, 高度
