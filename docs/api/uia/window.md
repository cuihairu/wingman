# API: UIA Window

窗口控件，代表应用程序的主窗口或对话框。

## 查找窗口

窗口通常作为 UI 树的根元素，通过以下方式获取：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取前台窗口（最常用）
root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"窗口名称: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 获取前台窗口（最常用）
local root = uia.fromForeground()
if root then
    local info = root:getInfo()
    print("窗口名称: " .. info.name)
end
```

:::

---

## 从窗口句柄获取

如果已经知道窗口句柄：

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

hwnd, found = window.find("记事本")
if found:
    root = uia.from_window(hwnd)
    if root:
        print("记事本 UI 根元素获取成功")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")

local hwnd, found = window.find("记事本")
if found then
    local root = uia.fromWindow(hwnd)
    if root then
        print("记事本 UI 根元素获取成功")
    end
end
```

:::

---

## 查找子窗口

某些应用包含多个子窗口或对话框：

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    # 查找所有子窗口
    windows = uia.find_all_by_control_type("Window")
    for win in windows:
        info = win.get_info()
        print(f"子窗口: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
if root then
    -- 查找所有子窗口
    local windows = uia.findAllByControlType("Window")
    for i, win in ipairs(windows) do
        local info = win:getInfo()
        print("子窗口: " .. info.name)
    end
end
```

:::

---

## 查找对话框

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待对话框出现
dialog = uia.wait_for_name("设置", 3000)
if dialog:
    info = dialog.get_info()
    if info.get('control_type') == 'Window':
        print("找到对话框窗口")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 等待对话框出现
local dialog = uia.waitForName("设置", 3000)
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

    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        print(f"位置: ({rect['left']}, {rect['top']})")
        print(f"大小: {rect['width']} x {rect['height']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
if root then
    local info = root:getInfo()
    print("窗口标题: " .. (info.name or ""))
    print("控件类型: " .. (info.controlType or ""))
    print("是否可见: " .. tostring(info.isVisible or true))
    print("是否启用: " .. tostring(info.isEnabled or true))

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
