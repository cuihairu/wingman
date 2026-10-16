> [中文](window.md) | **English**

# API: UIA Window

The Window control represents an application's main window, dialog, or popup window.

## Getting a window

### Getting the foreground window

**Note**: Gets the UI root element of the currently active window. This is the most commonly used approach.

**Function signature**:

```python
from_foreground() -> UIElement | None
```

```lua
from_foreground() -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取前台窗口
root = uia.from_foreground()
if root:
    info = root["get_info"]()
    print(f"窗口名称: {info.get('name', '')}")
    print(f"角色: {info.get('role', 0)}")  # 1 = UIARole Window
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取前台窗口
local root = wingman.uia.from_foreground()
if root then
    local info = root:get_info()
    print("窗口名称: " .. (info.name or ""))
    print("角色: " .. tostring(info.role or 0))  -- 1 = UIARole Window
end
```

:::

### Getting from a window handle

**Note**: If you already know the window handle (HWND), you can get its UI root element directly.

**Function signature**:

```python
from_window(hwnd: int) -> UIElement | None
```

```lua
from_window(hwnd: number) -> UIElement | nil
```

**Parameters**:
- `hwnd` - window handle

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

# 先查找窗口句柄（返回数组 [handle, found]，Python 列表解包可用）
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

-- 先查找窗口句柄（window.find 返回单值数组 {handle, found}，Lua 需先取数组再解两个元素）
local result = wingman.window.find("记事本")
local hwnd, found = result[1], result[2]
if found then
    -- 从句柄获取 UI 根元素
    local root = wingman.uia.from_window(hwnd)
    if root then
        print("记事本 UI 根元素获取成功")
    end
end
```

:::

---

## Finding child windows/dialogs

**Note**: Some applications contain multiple child windows or dialogs.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找所有 Window 角色（UIARole 1）的控件
windows = uia.find_all_by_control_type(1)

print(f"找到 {len(windows)} 个窗口：")
for win in windows:
    info = win["get_info"]()
    print(f"  - {info.get('name', '(无名称)')}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找所有 Window 角色（UIARole 1）的控件
local windows = wingman.uia.find_all_by_control_type(1)

print("找到 " .. #windows .. " 个窗口：")
for i, win in ipairs(windows) do
    local info = win:get_info()
    print("  - " .. (info.name or "(无名称)"))
end
```

:::

---

## Waiting for a dialog to appear

**Note**: A dialog may need time to load; you can poll and wait.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待对话框出现（最多 3 秒）
dialog = uia.wait_for_name("设置", 3000)
if dialog:
    info = dialog["get_info"]()
    if info.get('role', 0) == 1:  # 1 = UIARole Window
        print("找到对话框窗口")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 等待对话框出现（最多 3 秒）
local dialog = wingman.uia.wait_for_name("设置", 3000)
if dialog then
    local info = dialog:get_info()
    if info.role == 1 then  -- 1 = UIARole Window
        print("找到对话框窗口")
    end
end
```

:::

---

## Window properties

**Note**: Gets various property information of the window.

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    info = root["get_info"]()

    print(f"窗口标题: {info.get('name', '')}")
    print(f"角色: {info.get('role', 0)}")  # 1 = UIARole Window
    print(f"是否可见: {info.get('is_visible', True)}")
    print(f"是否启用: {info.get('is_enabled', True)}")

    # 位置和大小
    if 'bounds' in info:
        rect = info['bounds']
        print(f"位置: ({rect['x']}, {rect['y']})")
        print(f"大小: {rect['width']} x {rect['height']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local root = wingman.uia.from_foreground()
if root then
    local info = root:get_info()

    print("窗口标题: " .. (info.name or ""))
    print("角色: " .. tostring(info.role or 0))  -- 1 = UIARole Window
    print("是否可见: " .. tostring(info.is_visible or false))
    print("是否启用: " .. tostring(info.is_enabled or false))

    -- 位置和大小
    if info.bounds then
        local rect = info.bounds
        print(string.format("位置: (%d, %d)", rect.x, rect.y))
        print(string.format("大小: %d x %d", rect.width, rect.height))
    end
end
```

:::

---

## Available Interfaces

### Getting a window

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `from_foreground()` | `from_foreground()` | Get the foreground window |
| `from_window(hwnd)` | `from_window(hwnd)` | Get the window from a handle |
| `from_point(x, y)` | `from_point(x, y)` | Get the window element from coordinates |

### Window operations

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_info()` | `:get_info()` | Get window information |
| `get_children()` | `:get_children()` | Get the child elements inside the window |
