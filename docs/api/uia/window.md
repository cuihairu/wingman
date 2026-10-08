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

### 从窗口句柄获取

**说明**：如果已经知道窗口句柄（HWND），可以直接获取其 UI 根元素。

**函数签名**：

```python
from_window(hwnd: int) -> UIElement | None
```

```lua
from_window(hwnd: number) -> UIElement | nil
```

**参数**：
- `hwnd` - 窗口句柄

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

## 查找子窗口/对话框

**说明**：某些应用包含多个子窗口或对话框。

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

## 等待对话框出现

**说明**：对话框可能需要时间加载，可以轮询等待。

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

## 窗口属性

**说明**：获取窗口的各种属性信息。

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

## 可用接口

### 获取窗口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `from_foreground()` | `from_foreground()` | 获取前台窗口 |
| `from_window(hwnd)` | `from_window(hwnd)` | 从句柄获取窗口 |
| `from_point(x, y)` | `from_point(x, y)` | 从坐标获取窗口元素 |

### 窗口操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:get_info()` | 获取窗口信息 |
| `get_children()` | `:get_children()` | 获取窗口内的子元素 |
