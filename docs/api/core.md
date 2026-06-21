# 核心 API

核心 API 提供 Wingman 的基础功能，包括屏幕操作、输入模拟、窗口管理等。

## 📋 模块列表

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

截取指定区域的屏幕截图。

**参数:**
- `x` (number): 起始 X 坐标
- `y` (number): 起始 Y 坐标
- `width` (number): 宽度
- `height` (number): 高度

**返回:** Image 对象

**示例:**
```lua
-- Lua
local screenshot = screen.capture(0, 0, 1920, 1080)
screenshot:save("screenshot.png")

-- 获取截图信息
print(screenshot:width())   -- 1920
print(screenshot:height())  -- 1080
```

```python
# Python
screenshot = screen.capture(0, 0, 1920, 1080)
screenshot.save("screenshot.png")

# 获取截图信息
print(screenshot.width())   # 1920
print(screenshot.height())  # 1080
```

#### `captureWindow(windowTitle)`

截取指定窗口的截图。

**参数:**
- `windowTitle` (string): 窗口标题（支持通配符）

**返回:** Image 对象

**示例:**
```lua
-- Lua
local screenshot = screen.captureWindow("Notepad")
screenshot:save("notepad.png")
```

```python
# Python
screenshot = screen.captureWindow("Notepad")
screenshot.save("notepad.png")
```

#### `findColor(color, x, y, width, height, tolerance)`

在指定区域查找指定颜色。

**参数:**
- `color` (number): 颜色值（0xRRGGBB 格式）
- `x` (number): 搜索区域起始 X 坐标
- `y` (number): 搜索区域起始 Y 坐标
- `width` (number): 搜索区域宽度
- `height` (number): 搜索区域高度
- `tolerance` (number): 颜色容差（0-255）

**返回:** Point 数组或 nil

**示例:**
```lua
-- Lua
local points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points then
    for i, point in ipairs(points) do
        print(string.format("Found at: %d, %d", point.x, point.y))
    end
end
```

```python
# Python
points = screen.findColor(0xFF0000, 0, 0, 1920, 1080, 10)
if points:
    for point in points:
        print(f"Found at: {point['x']}, {point['y']}")
```

#### `findImage(imagePath, x, y, width, height, threshold)`

在指定区域查找图像。

**参数:**
- `imagePath` (string): 图像文件路径
- `x` (number): 搜索区域起始 X 坐标
- `y` (number): 搜索区域起始 Y 坐标
- `width` (number): 搜索区域宽度
- `height` (number): 搜索区域高度
- `threshold` (number): 匹配阈值（0.0-1.0）

**返回:** Match 对象或 nil

**示例:**
```lua
-- Lua
local match = screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
if match then
    print(string.format("Found at: %d, %d, confidence: %.2f",
        match.x, match.y, match.confidence))
end
```

```python
# Python
match = screen.findImage("target.png", 0, 0, 1920, 1080, 0.8)
if match:
    print(f"Found at: {match['x']}, {match['y']}, confidence: {match['confidence']}")
```

#### `getPixelColor(x, y)`

获取指定坐标的像素颜色。

**参数:**
- `x` (number): X 坐标
- `y` (number): Y 坐标

**返回:** 颜色值（0xRRGGBB 格式）

**示例:**
```lua
-- Lua
local color = screen.getPixelColor(100, 100)
print(string.format("Color: 0x%X", color))
```

```python
# Python
color = screen.getPixelColor(100, 100)
print(f"Color: 0x{color:X}")
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

在指定位置执行鼠标点击。

**参数:**
- `x` (number): X 坐标
- `y` (number): Y 坐标
- `button` (string): 按键（"left", "right", "middle"）

**示例:**
```lua
-- Lua
input.click(100, 100, "left")   -- 左键点击
input.click(100, 100, "right")  -- 右键点击
```

```python
# Python
input.click(100, 100, "left")   # 左键点击
input.click(100, 100, "right")  # 右键点击
```

#### `move(x, y, duration)`

将鼠标移动到指定位置。

**参数:**
- `x` (number): 目标 X 坐标
- `y` (number): 目标 Y 坐标
- `duration` (number): 移动持续时间（毫秒）

**示例:**
```lua
-- Lua
input.move(500, 500, 500)  -- 500毫秒移动到(500, 500)
```

```python
# Python
input.move(500, 500, 500)  # 500毫秒移动到(500, 500)
```

#### `moveHuman(x, y)`

以人类自然的速度移动鼠标（使用贝塞尔曲线）。

**参数:**
- `x` (number): 目标 X 坐标
- `y` (number): 目标 Y 坐标

**示例:**
```lua
-- Lua
input.moveHuman(500, 500)  -- 人类自然的移动方式
```

```python
# Python
input.moveHuman(500, 500)  # 人类自然的移动方式
```

#### `scroll(delta)`

执行鼠标滚轮操作。

**参数:**
- `delta` (number): 滚动量（正数向上，负数向下）

**示例:**
```lua
-- Lua
input.scroll(5)   -- 向上滚动
input.scroll(-5)  -- 向下滚动
```

```python
# Python
input.scroll(5)   # 向上滚动
input.scroll(-5)  # 向下滚动
```

### 键盘操作

#### `keyDown(key)`

按下指定按键。

**参数:**
- `key` (string): 按键名称（如 "A", "F1", "SPACE", "CTRL"）

**示例:**
```lua
-- Lua
input.keyDown("CTRL")
input.keyDown("A")
```

```python
# Python
input.keyDown("CTRL")
input.keyDown("A")
```

#### `keyUp(key)**

释放指定按键。

**参数:**
- `key` (string): 按键名称

**示例:**
```lua
-- Lua
input.keyUp("A")
input.keyUp("CTRL")
```

```python
# Python
input.keyUp("A")
input.keyUp("CTRL")
```

#### `keyPress(key, duration)**

执行按键按下和释放的组合操作。

**参数:**
- `key` (string): 按键名称
- `duration` (number): 按下持续时间（毫秒，可选）

**示例:**
```lua
-- Lua
input.keyPress("A")           -- 按下并释放 A 键
input.keyPress("SPACE", 100)  -- 按下空格键 100 毫秒
```

```python
# Python
input.keyPress("A")           # 按下并释放 A 键
input.keyPress("SPACE", 100)  # 按下空格键 100 毫秒
```

#### `keyPressCombination(keys)`

执行组合按键。

**参数:**
- `keys` (table/array): 按键数组

**示例:**
```lua
-- Lua
input.keyPressCombination({"CTRL", "A"})   -- Ctrl+A
input.keyPressCombination({"CTRL", "SHIFT", "ESC"})  -- Ctrl+Shift+Esc
```

```python
# Python
input.keyPressCombination(["CTRL", "A"])   # Ctrl+A
input.keyPressCombination(["CTRL", "SHIFT", "ESC"])  # Ctrl+Shift+Esc
```

### 文本输入

#### `text(text)`

输入文本字符串。

**参数:**
- `text` (string): 要输入的文本

**示例:**
```lua
-- Lua
input.text("Hello, World!")
```

```python
# Python
input.text("Hello, World!")
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

查找指定标题的窗口。

**参数:**
- `title` (string): 窗口标题（支持通配符 * 和 ?）

**返回:** Window 对象数组

**示例:**
```lua
-- Lua
local windows = window.find("Notepad")
for i, win in ipairs(windows) do
    print(string.format("Found: %s at %d, %d",
        win.title, win.x, win.y))
end
```

```python
# Python
windows = window.find("Notepad")
for win in windows:
    print(f"Found: {win['title']} at {win['x']}, {win['y']}")
```

#### `findByClass(className)`

根据窗口类名查找窗口。

**参数:**
- `className` (string): 窗口类名

**返回:** Window 对象数组

**示例:**
```lua
-- Lua
local windows = window.findByClass("Notepad")
```

```python
# Python
windows = window.findByClass("Notepad")
```

#### `activate(title)`

激活指定标题的窗口。

**参数:**
- `title` (string): 窗口标题

**返回:** boolean - 是否成功

**示例:**
```lua
-- Lua
if window.activate("Notepad") then
    print("Window activated successfully")
end
```

```python
# Python
if window.activate("Notepad"):
    print("Window activated successfully")
```

#### `getBounds(title)`

获取窗口的边界位置。

**参数:**
- `title` (string): 窗口标题

**返回:** Bounds 对象（x, y, width, height）

**示例:**
```lua
-- Lua
local bounds = window.getBounds("Notepad")
print(string.format("Position: %d, %d", bounds.x, bounds.y))
print(string.format("Size: %d x %d", bounds.width, bounds.height))
```

```python
# Python
bounds = window.getBounds("Notepad")
print(f"Position: {bounds['x']}, {bounds['y']}")
print(f"Size: {bounds['width']} x {bounds['height']}")
```

#### `setForeground(title)`

将指定窗口设置为前台窗口。

**参数:**
- `title` (string): 窗口标题

**返回:** boolean - 是否成功

**示例:**
```lua
-- Lua
window.setForeground("Notepad")
```

```python
# Python
window.setForeground("Notepad")
```

#### `minimize(title)`

最小化指定窗口。

**参数:**
- `title` (string): 窗口标题

**示例:**
```lua
-- Lua
window.minimize("Notepad")
```

```python
# Python
window.minimize("Notepad")
```

#### `maximize(title)`

最大化指定窗口。

**参数:**
- `title` (string): 窗口标题

**示例:**
```lua
-- Lua
window.maximize("Notepad")
```

```python
# Python
window.maximize("Notepad")
```

#### `restore(title)`

恢复指定窗口到正常状态。

**参数:**
- `title` (string): 窗口标题

**示例:**
```lua
-- Lua
window.restore("Notepad")
```

```python
# Python
window.restore("Notepad")
```

#### `close(title)`

关闭指定窗口。

**参数:**
- `title` (string): 窗口标题

**示例:**
```lua
-- Lua
window.close("Notepad")
```

```python
# Python
window.close("Notepad")
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

查找指定名称的进程。

**参数:**
- `name` (string): 进程名称（支持通配符）

**返回:** Process 对象数组

**示例:**
```lua
-- Lua
local processes = process.find("notepad.exe")
for i, proc in ipairs(processes) do
    print(string.format("PID: %d, Name: %s", proc.pid, proc.name))
end
```

```python
# Python
processes = process.find("notepad.exe")
for proc in processes:
    print(f"PID: {proc['pid']}, Name: {proc['name']}")
```

#### `exists(name)`

检查指定进程是否存在。

**参数:**
- `name` (string): 进程名称

**返回:** boolean

**示例:**
```lua
-- Lua
if process.exists("notepad.exe") then
    print("Notepad is running")
end
```

```python
# Python
if process.exists("notepad.exe"):
    print("Notepad is running")
```

#### `getByPid(pid)`

根据进程 ID 获取进程信息。

**参数:**
- `pid` (number): 进程 ID

**返回:** Process 对象或 nil

**示例:**
```lua
-- Lua
local proc = process.getByPid(1234)
if proc then
    print("Process name:", proc.name)
end
```

```python
# Python
proc = process.getByPid(1234)
if proc:
    print(f"Process name: {proc['name']}")
```

#### `terminate(name)`

终止指定进程。

**参数:**
- `name` (string): 进程名称

**返回:** boolean - 是否成功

**示例:**
```lua
-- Lua
if process.terminate("notepad.exe") then
    print("Process terminated")
end
```

```python
# Python
if process.terminate("notepad.exe"):
    print("Process terminated")
```

#### `start(path, args)`

启动指定进程。

**参数:**
- `path` (string): 可执行文件路径
- `args` (string/array): 命令行参数（可选）

**返回:** Process 对象或 nil

**示例:**
```lua
-- Lua
local proc = process.start("C:\\Windows\\notepad.exe")
if proc then
    print("Started process with PID:", proc.pid)
end

-- 带参数启动
local proc = process.start("C:\\Program Files\\MyApp\\app.exe", {
    "--config", "config.ini",
    "--debug"
})
```

```python
# Python
proc = process.start("C:\\Windows\\notepad.exe")
if proc:
    print(f"Started process with PID: {proc['pid']}")

# 带参数启动
proc = process.start("C:\\Program Files\\MyApp\\app.exe", [
    "--config", "config.ini",
    "--debug"
])
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

#### `matchTemplate(templatePath, x, y, width, height, threshold)`

使用模板匹配查找图像。

**参数:**
- `templatePath` (string): 模板图像路径
- `x, y, width, height` (number): 搜索区域
- `threshold` (number): 匹配阈值（0.0-1.0）

**返回:** Match 对象数组

**示例:**
```lua
-- Lua
local matches = vision.matchTemplate("icon.png", 0, 0, 1920, 1080, 0.8)
for i, match in ipairs(matches) do
    print(string.format("Match at: %d, %d, confidence: %.2f",
        match.x, match.y, match.confidence))
end
```

```python
# Python
matches = vision.matchTemplate("icon.png", 0, 0, 1920, 1080, 0.8)
for match in matches:
    print(f"Match at: {match['x']}, {match['y']}, confidence: {match['confidence']}")
```

#### `findMultiple(templatePath, x, y, width, height, threshold, maxCount)`

查找多个匹配项。

**参数:**
- `templatePath` (string): 模板图像路径
- `x, y, width, height` (number): 搜索区域
- `threshold` (number): 匹配阈值
- `maxCount` (number): 最大匹配数量

**返回:** Match 对象数组

**示例:**
```lua
-- Lua
local matches = vision.findMultiple("coin.png", 0, 0, 1920, 1080, 0.8, 10)
print("Found " .. #matches .. " coins")
```

```python
# Python
matches = vision.findMultiple("coin.png", 0, 0, 1920, 1080, 0.8, 10)
print(f"Found {len(matches)} coins")
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

#### `recognize(x, y, width, height, language)`

识别指定区域的文字。

**参数:**
- `x, y, width, height` (number): 识别区域
- `language` (string): 语言代码（可选，默认 "eng"）

**返回:** string - 识别的文字

**示例:**
```lua
-- Lua
local text = ocr.recognize(100, 100, 500, 100, "eng")
print("Recognized text:", text)
```

```python
# Python
text = ocr.recognize(100, 100, 500, 100, "eng")
print(f"Recognized text: {text}")
```

#### `recognizeWords(x, y, width, height, language)`

识别指定区域的文字并返回单词位置。

**参数:**
- `x, y, width, height` (number): 识别区域
- `language` (string): 语言代码（可选）

**返回:** Word 对象数组

**示例:**
```lua
-- Lua
local words = ocr.recognizeWords(100, 100, 500, 100)
for i, word in ipairs(words) do
    print(string.format("Word: %s at %d, %d",
        word.text, word.x, word.y))
end
```

```python
# Python
words = ocr.recognizeWords(100, 100, 500, 100)
for word in words:
    print(f"Word: {word['text']} at {word['x']}, {word['y']}")
```

---

## 🔗 相关文档

- [智能触发器](smart-trigger.md)
- [人性化模拟](human.md)
- [数据持久化 API](data.md)
- [序列化 API](serialize.md)
- [快速开始](../getting-started.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
