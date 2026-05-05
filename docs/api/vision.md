# Vision API

视觉模块提供屏幕检测、图像匹配、形状识别等功能。

## Lua API

### vision.findColor(color, tolerance?, region?)

查找指定颜色的位置。

```lua
local pos = vision.findColor({r=255, g=0, b=0}, 10)
if pos then
    print("找到颜色: ", pos.x, pos.y)
end
```

**参数:**
- `color` - 颜色值 `{r, g, b}` 或整数 `0xRRGGBB`
- `tolerance` - 容差值，默认 0
- `region` - 搜索区域 `{x, y, width, height}`，默认全屏

**返回:** `{x, y}` 或 `nil`

---

### vision.findAllColors(color, tolerance?, region?)

查找所有匹配颜色的位置。

```lua
local positions = vision.findAllColors({r=255, g=0, b=0}, 5)
for i, pos in ipairs(positions) do
    print("位置", i, ":", pos.x, pos.y)
end
```

**返回:** 数组 `[{x, y}, ...]`

---

### vision.hasColor(color, tolerance?, region?)

检查区域内是否包含指定颜色。

```lua
if vision.hasColor({r=0, g=255, b=0}, 10) then
    print("找到了绿色")
end
```

**返回:** `boolean`

---

### vision.getDominantColor(region?)

获取区域内主要颜色（众数）。

```lua
local color = vision.getDominantColor({x=0, y=0, width=200, height=200})
print("主要颜色:", color.r, color.g, color.b)
```

**返回:** `{r, g, b, a}`

---

### vision.findImage(templatePath, threshold?, region?)

查找图像模板在屏幕中的位置。

```lua
local result = vision.findImage("target.png", 0.8)
if result.found then
    print("找到图像:", result.position.x, result.position.y)
    print("置信度:", result.confidence)
end
```

**参数:**
- `templatePath` - 模板图片路径
- `threshold` - 匹配阈值 0.0-1.0，默认 0.8
- `region` - 搜索区域，默认全屏

**返回:** `{found: boolean, position: {x, y}, confidence: number}`

---

### vision.detectEdges(region?, threshold1?, threshold2?)

使用 Canny 算子检测边缘。

```lua
local edges = vision.detectEdges({x=0, y=0, width=800, height=600}, 50, 150)
print("检测到", #edges, "个边缘点")
```

**返回:** `[{x, y}, ...]`

---

### vision.detectContours(region?)

检测轮廓。

```lua
local contours = vision.detectContours({x=0, y=0, width=800, height=600})
for i, contour in ipairs(contours) do
    print("轮廓", i, "有", #contour, "个点")
end
```

---

### vision.detectCircles(region?, minRadius?, maxRadius?)

检测圆形。

```lua
local circles = vision.detectCircles({x=0, y=0, width=800, height=600}, 10, 100)
for i, circle in ipairs(circles) do
    print("圆", i, ": center(", circle.x, circle.y, ") radius=", circle.radius)
end
```

---

### vision.captureRegion(region, outputPath)

截取屏幕区域并保存为图片。

```lua
vision.captureRegion({x=0, y=0, width=1920, height=1080}, "screenshot.png")
```

**返回:** `boolean`
