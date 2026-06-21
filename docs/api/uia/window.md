# API: UIA Window

窗口（Window）控件代表应用程序的主窗口、对话框或弹出窗口。

## 获取窗口

### 获取前台窗口

**说明**：获取当前活动窗口的 UI 根元素。这是最常用的方式。

**函数签名**：

```python
from_foreground() -> UIElement | None
```

```lua
fromForeground() -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取前台窗口
root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"窗口名称: {info.get('name', '')}")
    print(f"控件类型: {info.get('control_type', '')}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取前台窗口
local root = wingman.uia.fromForeground()
if root then
    local info = root:getInfo()
    print("窗口名称: " .. (info.name or ""))
    print("控件类型: " .. (info.controlType or ""))
end
```

:::

### 从窗口句柄获取

**说明**：如果已经知道窗口句柄（HWND），可以直接获取其 UI 根元素。

**函数签名**：

```python
from_window(hwnd: int) -> UIElement | None
```

```lua
fromWindow(hwnd: number) -> UIElement | nil
```

**参数**：
- `hwnd` - 窗口句柄

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

# 先查找窗口句柄
hwnd, found = window.find("记事本")
if found:
    # 从句柄获取 UI 根元素
    root = uia.from_window(hwnd)
    if root:
        print("记事本 UI 根元素获取成功")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 先查找窗口句柄
local hwnd, found = wingman.window.find("记事本")
if found then
    -- 从句柄获取 UI 根元素
    local root = wingman.uia.fromWindow(hwnd)
    if root then
        print("记事本 UI 根元素获取成功")
    end
end
```

:::

---

## 查找子窗口/对话框

**说明**：某些应用包含多个子窗口或对话框。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找所有 Window 类型的控件
windows = uia.find_all_by_control_type("Window")

print(f"找到 {len(windows)} 个窗口：")
for win in windows:
    info = win.get_info()
    print(f"  - {info.get('name', '(无名称)')}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找所有 Window 类型的控件
local windows = wingman.uia.findAllByControlType("Window")

print("找到 " .. #windows .. " 个窗口：")
for i, win in ipairs(windows) do
    local info = win:getInfo()
    print("  - " .. (info.name or "(无名称)"))
end
```

:::

---

## 等待对话框出现

**说明**：对话框可能需要时间加载，可以轮询等待。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待对话框出现（最多 3 秒）
dialog = uia.wait_for_name("设置", 3000)
if dialog:
    info = dialog.get_info()
    if info.get('control_type') == 'Window':
        print("找到对话框窗口")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 等待对话框出现（最多 3 秒）
local dialog = wingman.uia.waitForName("设置", 3000)
if dialog then
    local info = dialog:getInfo()
    if info.controlType == "Window" then
        print("找到对话框窗口")
    end
end
```

:::

---

## 窗口属性

**说明**：获取窗口的各种属性信息。

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    info = root.get_info()

    print(f"窗口标题: {info.get('name', '')}")
    print(f"控件类型: {info.get('control_type', '')}")
    print(f"是否可见: {info.get('is_visible', True)}")
    print(f"是否启用: {info.get('is_enabled', True)}")

    # 位置和大小
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        print(f"位置: ({rect['left']}, {rect['top']})")
        print(f"大小: {rect['width']} x {rect['height']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local root = wingman.uia.fromForeground()
if root then
    local info = root:getInfo()

    print("窗口标题: " .. (info.name or ""))
    print("控件类型: " .. (info.controlType or ""))
    print("是否可见: " .. tostring(info.isVisible or true))
    print("是否启用: " .. tostring(info.isEnabled or true))

    -- 位置和大小
    if info.boundingRect then
        local rect = info.boundingRect
        print(string.format("位置: (%d, %d)", rect.left, rect.top))
        print(string.format("大小: %d x %d", rect.width, rect.height))
    end
end
```

:::

---

## 可用接口

### 获取窗口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `from_foreground()` | `fromForeground()` | 获取前台窗口 |
| `from_window(hwnd)` | `fromWindow(hwnd)` | 从句柄获取窗口 |
| `from_point(x, y)` | `fromPoint(x, y)` | 从坐标获取窗口元素 |

### 窗口操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取窗口信息 |
| `get_children()` | `:getChildren()` | 获取窗口内的子元素 |
