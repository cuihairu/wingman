# 核心 API

核心 API 提供 Wingman 的基础功能，包括屏幕操作、输入模拟、窗口管理等。

## 模块列表

| 模块 | 功能 | 文档 |
|------|------|------|
| `screen` | 屏幕操作 | [screen](screen.md) |
| `input` | 输入模拟 | [input](input.md) |
| `window` | 窗口管理 | [window](window.md) |
| `process` | 进程管理 | [process](process.md) |
| `vision` | 图像识别 | [vision](vision.md) |
| `ocr` | OCR 文字识别 | [ocr](ocr.md) |
| `smarttrigger` | 智能触发器 | [smart-trigger](smart-trigger.md) |
| `human` | 人性化模拟 | [human](human.md) |
| `perf` | 性能监控 | [perf](perf.md) |

---

## Screen - 屏幕操作

屏幕操作模块提供截图、颜色检测、图像识别等功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import screen
```

### 函数列表

#### `capture(x, y, width, height)`

截取屏幕截图。

**参数（仅 Python，均可选）:**
- `x` (number): 起始 X 坐标
- `y` (number): 起始 Y 坐标
- `width` (number): 宽度
- `height` (number): 高度

Lua 无参数，截取全屏。

**返回:** boolean（截图是否成功）

**示例:**
```lua
-- Lua
local ok = screen.capture()
```

```python
# Python
ok = screen.capture(0, 0, 1920, 1080)
```

#### `captureRegion(region)`

截取指定区域的屏幕截图。

**参数:**
- `region` (table): 区域 `{x, y, width, height}`

**返回:** boolean（截图是否成功）

**示例:**
```lua
-- Lua
local ok = screen.captureRegion({x=0, y=0, width=1920, height=1080})
```

```python
# Python
ok = screen.captureRegion({"x": 0, "y": 0, "width": 1920, "height": 1080})
```

#### `findColor(color, region, tolerance)`

在指定区域查找指定颜色。

**参数:**
- `color` (number/table): 颜色值（0xRRGGBB 格式或 `{r, g, b}` 表）
- `region` (table): 搜索区域 `{x, y, width, height}`
- `tolerance` (number): 颜色容差，默认 10

**返回:** Lua 返回单数组 `{point, found}`；Python 返回元组 `(point, found)`

**示例:**
```lua
-- Lua
local res = screen.findColor(0xFF0000, {x=0, y=0, width=1920, height=1080}, 10)
if res[2] then
    print(string.format("Found at: %d, %d", res[1].x, res[1].y))
end
```

```python
# Python
point, found = screen.findColor(0xFF0000, {"x": 0, "y": 0, "width": 1920, "height": 1080}, 10)
if found:
    print(f"Found at: {point['x']}, {point['y']}")
```

#### `findColors(color, region, tolerance, maxCount)`

在指定区域查找指定颜色的所有匹配点。

**参数:**
- `color` (number/table): 颜色值（0xRRGGBB 格式或 `{r, g, b}` 表）
- `region` (table): 搜索区域 `{x, y, width, height}`
- `tolerance` (number): 颜色容差，默认 10
- `maxCount` (number): 最大返回数量，默认 0（不限）

**返回:** Point 数组

**示例:**
```lua
-- Lua
local points = screen.findColors(0xFF0000, {x=0, y=0, width=1920, height=1080}, 10, 0)
for i, point in ipairs(points) do
    print(string.format("Found at: %d, %d", point.x, point.y))
end
```

```python
# Python
points = screen.findColors(0xFF0000, {"x": 0, "y": 0, "width": 1920, "height": 1080}, 10, 0)
for point in points:
    print(f"Found at: {point['x']}, {point['y']}")
```

#### `findImage(imagePath, region, threshold)`

在指定区域查找图像。

**参数:**
- `imagePath` (string): 图像文件路径
- `region` (table): 搜索区域 `{x, y, width, height}`，默认全屏
- `threshold` (number): 匹配阈值（0.0-1.0），默认 0.9

**返回:** Lua 返回单数组 `{point, found}`；Python 返回元组 `(point, found)`

**示例:**
```lua
-- Lua
local res = screen.findImage("target.png", {x=0, y=0, width=1920, height=1080}, 0.8)
if res[2] then
    print(string.format("Found at: %d, %d", res[1].x, res[1].y))
end
```

```python
# Python
point, found = screen.findImage("target.png", {"x": 0, "y": 0, "width": 1920, "height": 1080}, 0.8)
if found:
    print(f"Found at: {point['x']}, {point['y']}")
```

#### `getPixel(x, y)`

获取指定坐标的像素颜色。

**参数:**
- `x` (number): X 坐标
- `y` (number): Y 坐标

**返回:** Lua 返回 `{r, g, b, a}` 表；Python 返回 `Color`

**示例:**
```lua
-- Lua
local color = screen.getPixel(100, 100)
print(string.format("Color: 0x%02X%02X%02X", color.r, color.g, color.b))
```

```python
# Python
color = screen.getPixel(100, 100)
print(f"Color: 0x{color['r']:02X}{color['g']:02X}{color['b']:02X}")
```

#### `getScreenWidth()` / `getScreenHeight()`

获取屏幕宽高（像素）。

**返回:** int

**示例:**
```lua
-- Lua
print(screen.getScreenWidth())
print(screen.getScreenHeight())
```

```python
# Python
print(screen.getScreenWidth())
print(screen.getScreenHeight())
```

---

## Input - 输入模拟

输入模拟模块提供鼠标、键盘等输入设备的模拟操作。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import input
```

### 鼠标操作

#### `click(x, y, button)`

将鼠标移动到指定位置并点击。

**参数:**
- `x` (number): X 坐标
- `y` (number): Y 坐标
- `button` (number): 按键（0=左键（默认），1=中键，2=右键）

**返回:** 无

**示例:**
```lua
-- Lua
input.click(100, 100)    -- 左键点击
input.click(100, 100, 2) -- 右键点击
```

```python
# Python
input.click(100, 100)    # 左键点击
input.click(100, 100, 2) -- 右键点击
```

#### `move(x, y, duration)`

将鼠标移动到指定位置。

**参数:**
- `x` (number): 目标 X 坐标
- `y` (number): 目标 Y 坐标
- `duration` (number): 移动后等待的毫秒数（默认 0）

**返回:** 无

**示例:**
```lua
-- Lua
input.move(500, 500, 500)
```

```python
# Python
input.move(500, 500, 500)
```

#### `scroll(x, y, delta)`

将鼠标移动到指定位置并执行滚轮操作。

**参数:**
- `x` (number): X 坐标
- `y` (number): Y 坐标
- `delta` (number): 滚动量（正数向上，负数向下）

**返回:** 无

**示例:**
```lua
-- Lua
input.scroll(500, 500, 5)   -- 向上滚动
input.scroll(500, 500, -5)  -- 向下滚动
```

```python
# Python
input.scroll(500, 500, 5)   # 向上滚动
input.scroll(500, 500, -5)  # 向下滚动
```

### 键盘操作

#### `keyDown(vkCode)` / `keyUp(vkCode)` / `key(vkCode)`

按下 / 释放 / 按下并释放指定按键。

**参数:**
- `vkCode` (number): 虚拟键码（如 0x41 = A，0x20 = 空格，0x11 = Shift，0x10 = Ctrl）

**返回:** 无

**示例:**
```lua
-- Lua
input.keyDown(0x41)   -- 按住 A
input.keyUp(0x41)     -- 释放 A
input.key(0x20)       -- 按下并释放空格
```

```python
# Python
input.keyDown(0x41)   -- 按住 A
input.keyUp(0x41)     -- 释放 A
input.key(0x20)       -- 按下并释放空格
```

### 文本输入与延迟

#### `type(text, delay)`

输入文本字符串。

**参数:**
- `text` (string): 要输入的文本
- `delay` (number): 输入完成后等待的毫秒数（默认 10）

**返回:** 无

**示例:**
```lua
-- Lua
input.type("Hello, World!", 10)
```

```python
# Python
input.type("Hello, World!", 10)
```

#### `delay(ms)` / `randomDelay(minMs, maxMs)`

延迟等待。

**参数:**
- `ms` (number): 等待毫秒数
- `minMs` / `maxMs` (number): 随机延迟区间（毫秒）

**返回:** 无

**示例:**
```lua
-- Lua
input.delay(100)
input.randomDelay(50, 200)
```

```python
# Python
input.delay(100)
input.randomDelay(50, 200)
```

---

## Window - 窗口管理

窗口管理模块提供窗口查找、激活、位置获取等功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import window
```

### 函数列表

#### `find(title)`

按标题查找窗口。

**参数:**
- `title` (string): 窗口标题

**返回:** Lua 返回数组 `{handle, found}`；Python 返回元组 `(handle, found)`

**示例:**
```lua
-- Lua
local res = window.find("Notepad")
if res[2] then
    print("Window handle:", res[1])
end
```

```python
# Python
handle, found = window.find("Notepad")
if found:
    print("Window handle:", handle)
```

#### `activate(handle)`

激活指定句柄的窗口。

**参数:**
- `handle` (number): 窗口句柄（由 `find` 返回）

**返回:** boolean - 是否成功

**示例:**
```lua
-- Lua
local res = window.find("Notepad")
if res[2] and window.activate(res[1]) then
    print("Window activated")
end
```

```python
# Python
handle, found = window.find("Notepad")
if found and window.activate(handle):
    print("Window activated")
```

#### `getForeground()`

获取当前前台窗口句柄。

**返回:** number - 窗口句柄

**示例:**
```lua
-- Lua
local handle = window.getForeground()
```

```python
# Python
handle = window.getForeground()
```

#### `getTitle(handle)`

获取指定窗口的标题。

**参数:**
- `handle` (number): 窗口句柄

**返回:** string - 窗口标题

**示例:**
```lua
-- Lua
print(window.getTitle(handle))
```

```python
# Python
print(window.getTitle(handle))
```

#### `getBounds(handle)`

获取指定窗口的边界位置与尺寸。

**参数:**
- `handle` (number): 窗口句柄

**返回:** `{x, y, width, height}` 表

**示例:**
```lua
-- Lua
local b = window.getBounds(handle)
print(string.format("Position: %d, %d", b.x, b.y))
print(string.format("Size: %d x %d", b.width, b.height))
```

```python
# Python
b = window.getBounds(handle)
print(f"Position: {b['x']}, {b['y']}")
print(f"Size: {b['width']} x {b['height']}")
```

#### `waitFor(title, timeout)`

等待标题匹配的窗口出现。

**参数:**
- `title` (string): 窗口标题
- `timeout` (number): 等待超时（毫秒）

**返回:** boolean - 是否等到

**示例:**
```lua
-- Lua
if window.waitFor("Notepad", 5000) then
    print("Notepad appeared")
end
```

```python
# Python
if window.waitFor("Notepad", 5000):
    print("Notepad appeared")
```

---

## Process - 进程管理

进程管理模块提供进程查找和管理功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import process
```

### 函数列表

#### `find(name)`

按名称查找进程。

**参数:**
- `name` (string): 进程名称（支持通配符）

**返回:** Lua 返回数组 `{pid, found}`；Python 返回元组 `(pid, found)`

**示例:**
```lua
-- Lua
local res = process.find("notepad.exe")
if res[2] then
    print("PID:", res[1])
end
```

```python
# Python
pid, found = process.find("notepad.exe")
if found:
    print("PID:", pid)
```

#### `exists(pid)`

检查指定进程是否存在。

**参数:**
- `pid` (number): 进程 ID

**返回:** boolean

**示例:**
```lua
-- Lua
if process.exists(pid) then
    print("Process is running")
end
```

```python
# Python
if process.exists(pid):
    print("Process is running")
```

#### `start(path, args, workdir)`

启动指定进程。

**参数:**
- `path` (string): 可执行文件路径
- `args` (string): 命令行参数字符串（可选）
- `workdir` (string): 工作目录（可选）

**返回:** number - 进程 ID（pid）

**示例:**
```lua
-- Lua
local pid = process.start("C:\\Windows\\notepad.exe")
print("Started process with PID:", pid)

-- 带参数与工作目录启动
local pid = process.start("C:\\Program Files\\MyApp\\app.exe",
    "--config config.ini", "C:\\Program Files\\MyApp")
```

```python
# Python
pid = process.start("C:\\Windows\\notepad.exe")
print(f"Started process with PID: {pid}")

# 带参数与工作目录启动
pid = process.start("C:\\Program Files\\MyApp\\app.exe",
    "--config config.ini", "C:\\Program Files\\MyApp")
```

#### `wait(pid, timeout)`

等待指定进程退出。

**参数:**
- `pid` (number): 进程 ID
- `timeout` (number): 超时毫秒数（可选）

**返回:** boolean - 是否已退出

**示例:**
```lua
-- Lua
if process.wait(pid, 5000) then
    print("Process exited")
end
```

```python
# Python
if process.wait(pid, 5000):
    print("Process exited")
```

#### `terminate(pid, force)`

终止指定进程。

**参数:**
- `pid` (number): 进程 ID
- `force` (boolean): 是否强制终止（可选）

**返回:** boolean - 是否成功

**示例:**
```lua
-- Lua
if process.terminate(pid) then
    print("Process terminated")
end
```

```python
# Python
if process.terminate(pid):
    print("Process terminated")
```

#### `waitFor(name, timeout)`

等待名称匹配的进程出现。

**参数:**
- `name` (string): 进程名称
- `timeout` (number): 超时毫秒数（可选）

**返回:** boolean - 是否等到

**示例:**
```lua
-- Lua
if process.waitFor("notepad.exe", 5000) then
    print("Notepad appeared")
end
```

```python
# Python
if process.waitFor("notepad.exe", 5000):
    print("Notepad appeared")
```

---

## Vision - 图像识别

图像识别模块提供高级图像匹配和识别功能。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import vision
```

### 函数列表

#### `findColor(color, tolerance, region)`

在指定区域查找指定颜色（整屏）。

**参数:**
- `color` (number/table): 颜色值（0xRRGGBB 格式或 `{r, g, b}` 表）
- `tolerance` (number): 颜色容差（默认 10）
- `region` (table): 搜索区域 `{x, y, width, height}`（默认全屏）

**返回:** `{x, y}` 表或 nil

**示例:**
```lua
-- Lua
local p = vision.findColor(0xFF0000, 10, {x=0, y=0, width=1920, height=1080})
if p then
    print(string.format("Found at: %d, %d", p.x, p.y))
end
```

```python
# Python
p = vision.findColor(0xFF0000, 10, {"x": 0, "y": 0, "width": 1920, "height": 1080})
if p:
    print(f"Found at: {p['x']}, {p['y']}")
```

#### `findAllColors(color, tolerance, region)`

查找指定颜色的所有匹配点。

**参数:**
- `color` (number/table): 颜色值（0xRRGGBB 格式或 `{r, g, b}` 表）
- `tolerance` (number): 颜色容差（默认 10）
- `region` (table): 搜索区域 `{x, y, width, height}`（默认全屏）

**返回:** `{x, y}` 表数组

**示例:**
```lua
-- Lua
for i, p in ipairs(vision.findAllColors(0xFF0000, 10)) do
    print(string.format("Found at: %d, %d", p.x, p.y))
end
```

```python
# Python
for p in vision.findAllColors(0xFF0000, 10):
    print(f"Found at: {p['x']}, {p['y']}")
```

#### `hasColor(color, tolerance, region)`

判断指定区域是否包含指定颜色。

**参数:**
- `color` (number/table): 颜色值（0xRRGGBB 格式或 `{r, g, b}` 表）
- `tolerance` (number): 颜色容差（默认 10）
- `region` (table): 搜索区域 `{x, y, width, height}`（默认全屏）

**返回:** boolean

**示例:**
```lua
-- Lua
if vision.hasColor(0xFF0000, 10) then
    print("Color present")
end
```

```python
# Python
if vision.hasColor(0xFF0000, 10):
    print("Color present")
```

#### `getDominantColor(region)`

获取指定区域的主色调。

**参数:**
- `region` (table): 取色区域 `{x, y, width, height}`（默认全屏）

**返回:** `{r, g, b, a}` 表

**示例:**
```lua
-- Lua
local c = vision.getDominantColor({x=100, y=100, width=50, height=50})
print(string.format("Color: 0x%02X%02X%02X", c.r, c.g, c.b))
```

```python
# Python
c = vision.getDominantColor({"x": 100, "y": 100, "width": 50, "height": 50})
print(f"Color: 0x{c['r']:02X}{c['g']:02X}{c['b']:02X}")
```

#### `findImage(templatePath, threshold, region)`

使用模板匹配查找图像。

**参数:**
- `templatePath` (string): 模板图像路径
- `threshold` (number): 匹配阈值（默认 0.9）
- `region` (table): 搜索区域 `{x, y, width, height}`（默认全屏）

**返回:** `{found, position?, confidence?, region?}` 表

**示例:**
```lua
-- Lua
local m = vision.findImage("icon.png", 0.8)
if m.found then
    print(string.format("Match at: %d, %d, confidence: %.2f",
        m.position.x, m.position.y, m.confidence))
end
```

```python
# Python
m = vision.findImage("icon.png", 0.8)
if m["found"]:
    print(f"Match at: {m['position']['x']}, {m['position']['y']}, confidence: {m['confidence']}")
```

#### 模板批量管理

`templateAdd(name, path)` / `templateRemove(name)` / `templateHas(name)` 登记、注销、查询具名模板；`templateList()` 列出全部登记模板（`[{name, path}]`）；`templateFindAll(names, threshold, region)` 批量识别（返回 `[{name, found, position?, confidence?, region?}]`，`names` 为空时识别全部登记模板）。

**示例:**
```lua
-- Lua
vision.templateAdd("icon", "C:\\assets\\icon.png")
local results = vision.templateFindAll({"icon", "coin"}, 0.8)
for i, r in ipairs(results) do
    print(r.name, r.found, r.position and r.position.x or "-")
end
```

```python
# Python
vision.templateAdd("icon", "C:\\assets\\icon.png")
results = vision.templateFindAll(["icon", "coin"], 0.8)
for r in results:
    print(r["name"], r["found"], r.get("position", {}).get("x"))
```

#### AI 视觉识别

`aiSetup(cfg)` / `aiSetupLocal(cfg)` 配置 OpenAI 兼容 provider（`{baseUrl, model, timeoutSeconds}`），`aiSetupStatus()` 查询配置状态，`aiLocate(desc, region)` / `aiElements(desc, region)` 按自然语言描述定位元素。需以 `WINGMAN_ENABLE_VISION` 构建。

**示例:**
```lua
-- Lua
vision.aiSetup({baseUrl = "https://api.example.com/v1", model = "gpt-4o", timeoutSeconds = 60})
print(vision.aiLocate("game inventory button", {x=0, y=0, width=1920, height=1080}))
```

```python
# Python
vision.aiSetup({"baseUrl": "https://api.example.com/v1", "model": "gpt-4o", "timeoutSeconds": 60})
print(vision.aiLocate("game inventory button", {"x": 0, "y": 0, "width": 1920, "height": 1080}))
```

---

## OCR - 文字识别

OCR 模块提供文字识别功能（需要启用 Tesseract）。

### Lua API

```lua
local wingman = require("wingman")

```

### Python API

```python
from wingman import ocr
```

### 函数列表

#### `recognize(region)`

识别指定区域的文字。

**参数:**
- `region` (table): 识别区域 `{x, y, width, height}`

**返回:** `{success, text, confidence}` 表

**示例:**
```lua
-- Lua
local res = ocr.recognize({x=100, y=100, width=500, height=100})
if res.success then
    print("Recognized text:", res.text)
end
```

```python
# Python
res = ocr.recognize({"x": 100, "y": 100, "width": 500, "height": 100})
if res["success"]:
    print(f"Recognized text: {res['text']}")
```

#### `recognizeText(region)`

识别指定区域的文字，仅返回文本。

**参数:**
- `region` (table): 识别区域 `{x, y, width, height}`

**返回:** string 或 nil

**示例:**
```lua
-- Lua
local text = ocr.recognizeText({x=100, y=100, width=500, height=100})
if text then
    print("Recognized text:", text)
end
```

```python
# Python
text = ocr.recognizeText({"x": 100, "y": 100, "width": 500, "height": 100})
if text:
    print(f"Recognized text: {text}")
```

---

## 相关文档

- [智能触发器](smart-trigger.md)
- [人性化模拟](human.md)
- [数据持久化 API](data.md)
- [序列化 API](serialize.md)
- [快速开始](../guide/getting-started.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
