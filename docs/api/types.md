# API 数据类型参考

本文档详细说明了 Wingman API 中使用的各种数据类型和对象结构。

## 目录

- [Image 对象](#image-对象)
- [UIElement 对象](#uielement-对象)
- [Bounds 对象](#bounds-对象)
- [ElementInfo 对象](#elementinfo-对象)
- [HTTP Response 对象](#http-response-对象)
- [图像匹配结果](#图像匹配结果)
- [颜色值](#颜色值)
- [进程和窗口句柄](#进程和窗口句柄)

---

## Image 对象

**来源**：`screen.capture()`

**说明**：表示截取的屏幕图像区域。

**用途**：
- 作为 `find_image()` 的模板参数进行图像匹配
- 作为 `wait_for_image()` 的等待目标

**注意**：Image 对象主要用于内部图像匹配操作，不直接暴露像素数据。如需获取像素颜色，请使用 `screen.get_pixel()` 函数。

**示例**：

```python
from wingman import screen

# 截取屏幕区域
img = screen.capture(0, 0, 400, 300)

# 使用 Image 对象进行匹配
result = screen.find_image(img, 0, 0, 1920, 1080)
```

---

## UIElement 对象

**来源**：所有 `uia` 模块的查找函数

**说明**：代表一个 UI Automation 元素（按钮、编辑框、列表等）。

**方法**：

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取元素信息（返回 ElementInfo 对象） |
| `click()` | `:click()` | 点击元素 |
| `double_click()` | `:doubleClick()` | 双击元素 |
| `focus()` | `:focus()` | 设置焦点 |
| `get_value()` | `:getValue()` | 获取元素值 |
| `set_value(value)` | `:setValue(value)` | 设置元素值 |
| `get_children()` | `:getChildren()` | 获取子元素列表 |
| `expand()` | `:expand()` | 展开元素 |
| `collapse()` | `:collapse()` | 折叠元素 |
| `is_expanded()` | `:isExpanded()` | 检查是否已展开 |
| `is_visible()` | `:isVisible()` | 检查是否可见 |
| `is_enabled()` | `:isEnabled()` | 检查是否可用 |

**示例**：

```python
from wingman import uia

btn = uia.find_button("确定")
if btn:
    # 获取元素信息
    info = btn.get_info()

    # 点击元素
    btn.click()

    # 检查状态
    if btn.is_enabled():
        print("按钮可用")
```

---

## Bounds 对象

**来源**：`window.get_bounds()`, `ElementInfo.bounding_rect`

**说明**：表示矩形区域的位置和大小。

**Python 结构**：

```python
{
    "x": int,      # 左上角 X 坐标
    "y": int,      # 左上角 Y 坐标
    "width": int,  # 宽度
    "height": int  # 高度
}
```

**Lua 结构**：

```lua
{
    x = number,      -- 左上角 X 坐标
    y = number,      -- 左上角 Y 坐标
    width = number,  -- 宽度
    height = number  -- 高度
}
```

**示例**：

```python
from wingman import window

hwnd, found = window.find("记事本")
if found:
    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']})")
    print(f"大小: {bounds['width']}x{bounds['height']}")

    # 计算中心点
    center_x = bounds['x'] + bounds['width'] // 2
    center_y = bounds['y'] + bounds['height'] // 2
```

---

## ElementInfo 对象

**来源**：`UIElement.get_info()`

**说明**：包含 UI 元素的所有属性信息。

**Python 结构**：

```python
{
    "name": str,              # 元素名称（显示文本）
    "control_type": str,      # 控件类型（Button、Edit 等）
    "automation_id": str,     # AutomationId
    "bounding_rect": {        # 边界矩形
        "left": int,
        "top": int,
        "width": int,
        "height": int
    },
    "is_enabled": bool,       # 是否可用
    "is_visible": bool,       # 是否可见
    "value": str | None       # 元素值（如果有）
}
```

**Lua 结构**：

```lua
{
    name = string,            -- 元素名称
    controlType = string,      -- 控件类型
    automationId = string,    -- AutomationId
    boundingRect = {          -- 边界矩形
        left = number,
        top = number,
        width = number,
        height = number
    },
    isEnabled = boolean,       -- 是否可用
    isVisible = boolean,       -- 是否可见
    value = string | nil      -- 元素值
}
```

**示例**：

```python
from wingman import uia

btn = uia.find_button("确定")
if btn:
    info = btn.get_info()
    print(f"名称: {info['name']}")
    print(f"类型: {info['control_type']}")
    print(f"AutomationId: {info['automation_id']}")

    # 访问边界信息
    rect = info['bounding_rect']
    print(f"位置: ({rect['left']}, {rect['top']})")
```

---

## HTTP Response 对象

**来源**：所有 `http` 模块的请求函数

**说明**：HTTP 请求的响应结果。

**Python 结构**：

```python
{
    "success": bool,    # 请求是否成功（状态码 2xx）
    "status": int,      # HTTP 状态码
    "body": str,        # 响应体内容
    "headers": dict     # 响应头
}
```

**Lua 结构**：

```lua
{
    success = boolean,  -- 请求是否成功
    status = number,    -- HTTP 状态码
    body = string,      -- 响应体内容
    headers = table     -- 响应头
}
```

**示例**：

```python
from wingman import http

resp = http.get("https://api.example.com/data")
if resp["success"]:
    print(f"状态码: {resp['status']}")
    print(f"响应内容: {resp['body']}")
else:
    print(f"请求失败: {resp['status']}")
```

---

## 图像匹配结果

**来源**：`screen.find_image()`, `screen.wait_for_image()`

**说明**：图像匹配的位置和相似度信息。

**Python 结构**：`(x: int, y: int, confidence: float)` 元组

**Lua 结构**：`{x: number, y: number, confidence: number}` 表格

**字段说明**：
- `x` - 匹配位置 X 坐标
- `y` - 匹配位置 Y 坐标
- `confidence` - 相似度（0.0-1.0）

**示例**：

```python
from wingman import screen

result = screen.find_image("button.png", 0, 0, 1920, 1080)
if result:
    x, y, confidence = result
    print(f"找到图像，位置: ({x}, {y})")
    print(f"相似度: {confidence:.2%}")
```

```lua
local wingman = require("wingman")

local result = wingman.screen.findImage("button.png", 0, 0, 1920, 1080)
if result then
    print(string.format("找到图像，位置: (%d, %d)", result.x, result.y))
    print(string.format("相似度: %d%%", result.confidence * 100))
end
```

---

## 颜色值

**来源**：`screen.get_pixel()`, `screen.find_pixel()`, `screen.find_color()`

**说明**：RGB 颜色值，使用 24 位整数表示。

**格式**：十六进制 `0xRRGGBB`
- `RR` - 红色分量（0-255）
- `GG` - 绿色分量（0-255）
- `BB` - 蓝色分量（0-255）

**常见颜色值**：

| 颜色 | 十六进制 | 说明 |
|-----|---------|-----|
| 纯红 | `0xFF0000` | |
| 纯绿 | `0x00FF00` | |
| 纯蓝 | `0x0000FF` | |
| 白色 | `0xFFFFFF` | RGB(255, 255, 255) |
| 黑色 | `0x000000` | RGB(0, 0, 0) |

**提取 RGB 分量**：

```python
color = 0xFF0000  # 红色

r = (color >> 16) & 0xFF  # 255
g = (color >> 8) & 0xFF   # 0
b = color & 0xFF           # 0
```

```lua
local color = 0xFF0000  -- 红色

local r = (color >> 16) & 0xFF  -- 255
local g = (color >> 8) & 0xFF   -- 0
local b = color & 0xFF           -- 0
```

**构造颜色值**：

```python
color = (r << 16) | (g << 8) | b
```

```lua
local color = (r << 16) | (g << 8) | b
```

---

## 进程和窗口句柄

**来源**：`process.find()`, `window.find()`, `process.start()`

**说明**：操作系统分配的唯一标识符。

### 进程 ID（PID）

**类型**：整数

**说明**：标识系统中的唯一进程。

**来源**：
- `process.find()` 返回的 PID
- `process.start()` 返回新进程的 PID

**示例**：

```python
from wingman import process

pid, found = process.find("notepad")
if found:
    print(f"记事本 PID: {pid}")

# 启动新进程
new_pid = process.start("notepad.exe")
```

### 窗口句柄（HWND）

**类型**：整数

**说明**：标识系统中的唯一窗口。

**来源**：
- `window.find()` 返回的 HWND
- `window.get_foreground()` 返回的 HWND

**示例**：

```python
from wingman import window

hwnd, found = window.find("记事本")
if found:
    print(f"记事本窗口句柄: {hwnd}")
    window.activate(hwnd)
```

---

## 相关文档

- [API 索引](./index.md) - 所有 API 模块列表
- [wingman.screen](./screen.md) - 屏幕操作模块
- [wingman.uia](./uia/) - UI Automation 模块
- [wingman.http](./http.md) - HTTP 客户端模块
- [wingman.process](./process.md) - 进程管理模块
- [wingman.window](./window.md) - 窗口管理模块
