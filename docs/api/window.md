# API: wingman.window

窗口管理模块，提供窗口查找、激活、获取信息等功能。

> **💡 提示**：查看 [数据类型参考](./types.md) 了解窗口句柄（HWND）和 Bounds 对象的详细说明。

## 模块概述

window 模块用于管理与操作系统窗口相关的操作。主要功能包括：

- **窗口查找**：根据标题查找窗口
- **窗口激活**：将窗口置顶并获取焦点
- **窗口信息**：获取窗口标题、位置、大小等信息
- **窗口等待**：等待指定窗口出现

### 窗口句柄（HWND）

Windows 系统中，每个窗口都有一个唯一的标识符称为窗口句柄（HWND）。在 Wingman 中，窗口句柄用数字表示，所有窗口操作都需要传入窗口句柄。

### 坐标系统

窗口位置使用屏幕坐标系统：
- 左上角为原点 (0, 0)
- X 轴向右递增
- Y 轴向下递增

窗口边界包含以下信息：
- `x, y` - 窗口左上角坐标
- `width, height` - 窗口宽度和高度

---

## 查找窗口

### find(title)

**说明**：根据窗口标题查找窗口。支持部分匹配，即标题中包含指定字符串即可匹配。

**函数签名**：

```python
find(title: str) -> tuple[int, bool]
```

```lua
find(title: string) -> number, boolean
```

**参数**：
- `title` - 窗口标题（支持部分匹配）

**返回**：
- Python: `(hwnd, found)` 元组，hwnd 为窗口句柄，found 表示是否找到
- Lua: `hwnd, found` 两个返回值

:::tabs

== Python

```python:line-numbers
from wingman import window

# 查找记事本窗口
hwnd, found = window.find("记事本")
if found:
    print(f"找到记事本窗口，句柄: {hwnd}")
else:
    print("未找到记事本窗口")

# 查找 Chrome 窗口（标题中包含 "Chrome" 即可）
hwnd, found = window.find("Chrome")
if found:
    print(f"找到 Chrome 窗口，句柄: {hwnd}")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")

-- 查找记事本窗口
local hwnd, found = window.find("记事本")
if found then
    print("找到记事本窗口，句柄:", hwnd)
else
    print("未找到记事本窗口")
end

-- 查找 Chrome 窗口
local hwnd, found = window.find("Chrome")
if found then
    print("找到 Chrome 窗口，句柄:", hwnd)
end
```

:::

---

## 激活窗口

### activate(hwnd)

**说明**：激活指定窗口，将其置于前台并获取焦点。

**函数签名**：

```python
activate(hwnd: int) -> bool
```

```lua
activate(hwnd: number) -> boolean
```

**参数**：
- `hwnd` - 窗口句柄

**返回**：是否成功激活

:::tabs

== Python

```python:line-numbers
from wingman import window

# 查找并激活记事本
hwnd, found = window.find("记事本")
if found:
    success = window.activate(hwnd)
    if success:
        print("记事本已激活")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")

-- 查找并激活记事本
local hwnd, found = window.find("记事本")
if found then
    local success = window.activate(hwnd)
    if success then
        print("记事本已激活")
    end
end
```

:::

---

## 获取前台窗口

### get_foreground() / getForeground()

**说明**：获取当前前台（活动）窗口的句柄。

**函数签名**：

```python
get_foreground() -> int
```

```lua
getForeground() -> number
```

**返回**：前台窗口的句柄

:::tabs

== Python

```python:line-numbers
from wingman import window

# 获取前台窗口
hwnd = window.get_foreground()
title = window.get_title(hwnd)
print(f"前台窗口: {title}")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")

-- 获取前台窗口
local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("前台窗口:", title)
```

:::

---

## 获取窗口标题

### get_title(hwnd) / getTitle(hwnd)

**说明**：获取指定窗口的标题文本。

**函数签名**：

```python
get_title(hwnd: int) -> str
```

```lua
getTitle(hwnd: number) -> string
```

**参数**：
- `hwnd` - 窗口句柄

**返回**：窗口标题

:::tabs

== Python

```python:line-numbers
from wingman import window

# 获取前台窗口标题
hwnd = window.get_foreground()
title = window.get_title(hwnd)
print(f"标题: {title}")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")

-- 获取前台窗口标题
local hwnd = window.getForeground()
local title = window.getTitle(hwnd)
print("标题:", title)
```

:::

---

## 获取窗口边界

### get_bounds(hwnd) / getBounds(hwnd)

**说明**：获取窗口的位置和大小信息。

**函数签名**：

```python
get_bounds(hwnd: int) -> dict
```

```lua
getBounds(hwnd: number) -> table
```

**参数**：
- `hwnd` - 窗口句柄

**返回**：包含位置和大小信息的字典/表格
- Python: `{"x": int, "y": int, "width": int, "height": int}`
- Lua: `{x: number, y: number, width: number, height: number}`

:::tabs

== Python

```python:line-numbers
from wingman import window

hwnd, found = window.find("记事本")
if found:
    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']})")
    print(f"大小: {bounds['width']}x{bounds['height']}")

    # 计算窗口中心点
    center_x = bounds['x'] + bounds['width'] // 2
    center_y = bounds['y'] + bounds['height'] // 2
    print(f"中心点: ({center_x}, {center_y})")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")

local hwnd, found = window.find("记事本")
if found then
    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d)", bounds.x, bounds.y))
    print(string.format("大小: %dx%d", bounds.width, bounds.height))

    -- 计算窗口中心点
    local centerX = bounds.x + bounds.width / 2
    local centerY = bounds.y + bounds.height / 2
    print(string.format("中心点: (%d, %d)", centerX, centerY))
end
```

:::

---

## 等待窗口出现

### wait_for(title, timeout?) / waitFor(title, timeout?)

**说明**：等待指定标题的窗口出现。每隔一段时间检查一次，直到找到窗口或超时。

**函数签名**：

```python
wait_for(title: str, timeout: int = 5000) -> bool
```

```lua
waitFor(title: string, timeout: number = 5000) -> boolean
```

**参数**：
- `title` - 窗口标题（支持部分匹配）
- `timeout` - 可选，超时时间（毫秒），默认 5000

**返回**：是否在超时前找到窗口

**使用场景**：
- 启动应用程序后等待主窗口出现
- 等待对话框弹出
- 等待窗口加载完成

:::tabs

== Python

```python:line-numbers
from wingman import window, process

# 启动记事本
process.start("notepad.exe")

# 等待记事本窗口出现（最多等待 5 秒）
if window.wait_for("记事本", 5000):
    print("记事本已启动")

    # 查找并激活窗口
    hwnd, found = window.find("记事本")
    if found:
        window.activate(hwnd)
else:
    print("超时：记事本未启动")

# 使用默认超时时间
if window.wait_for("记事本"):
    print("记事本已出现")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local process = require("wingman.process")

-- 启动记事本
process.start("notepad.exe")

-- 等待记事本窗口出现（最多等待 5 秒）
if window.waitFor("记事本", 5000) then
    print("记事本已启动")

    -- 查找并激活窗口
    local hwnd, found = window.find("记事本")
    if found then
        window.activate(hwnd)
    end
else
    print("超时：记事本未启动")
end

-- 使用默认超时时间
if window.waitFor("记事本") then
    print("记事本已出现")
end
```

:::

---

## 完整示例

### 启动应用程序并操作

这个示例展示了如何启动应用程序、等待窗口出现并获取窗口信息：

:::tabs

== Python

```python:line-numbers
from wingman import window, process, util, input

# 查找记事本
hwnd, found = window.find("记事本")

if not found:
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")

    # 等待窗口出现
    if not window.wait_for("记事本", 3000):
        print("启动失败")
        exit(1)

    hwnd, found = window.find("记事本")

if found:
    # 激活窗口
    window.activate(hwnd)
    util.sleep(200)

    # 获取窗口信息
    title = window.get_title(hwnd)
    print(f"标题: {title}")

    bounds = window.get_bounds(hwnd)
    print(f"位置: ({bounds['x']}, {bounds['y']})")
    print(f"大小: {bounds['width']}x{bounds['height']}")

    # 计算窗口中心点
    center_x = bounds['x'] + bounds['width'] // 2
    center_y = bounds['y'] + bounds['height'] // 2

    # 点击窗口中心
    input.click(center_x, center_y)
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local process = require("wingman.process")
local util = require("wingman.util")
local input = require("wingman.input")

-- 查找记事本
local hwnd, found = window.find("记事本")

if not found then
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")

    -- 等待窗口出现
    if not window.waitFor("记事本", 3000) then
        print("启动失败")
        return
    end

    hwnd, found = window.find("记事本")
end

if found then
    -- 激活窗口
    window.activate(hwnd)
    util.sleep(200)

    -- 获取窗口信息
    local title = window.getTitle(hwnd)
    print("标题:", title)

    local bounds = window.getBounds(hwnd)
    print(string.format("位置: (%d, %d)", bounds.x, bounds.y))
    print(string.format("大小: %dx%d", bounds.width, bounds.height))

    -- 计算窗口中心点
    local centerX = bounds.x + bounds.width / 2
    local centerY = bounds.y + bounds.height / 2

    -- 点击窗口中心
    input.click(centerX, centerY)
end
```

:::

---

## 可用接口

### 窗口查找与激活

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find(title)` | `find(title)` | 查找窗口 | title: 窗口标题<br>返回: (hwnd, found) |
| `activate(hwnd)` | `activate(hwnd)` | 激活窗口 | hwnd: 窗口句柄<br>返回: 是否成功 |
| `wait_for(title, timeout?)` | `waitFor(title, timeout?)` | 等待窗口 | title: 窗口标题<br>timeout: 超时(ms) |

### 窗口信息获取

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get_foreground()` | `getForeground()` | 获取前台窗口 | 返回: 窗口句柄 |
| `get_title(hwnd)` | `getTitle(hwnd)` | 获取窗口标题 | hwnd: 窗口句柄<br>返回: 标题 |
| `get_bounds(hwnd)` | `getBounds(hwnd)` | 获取窗口边界 | hwnd: 窗口句柄<br>返回: {x, y, width, height} |
