# API: wingman.uia

UI Automation 模块，用于与 Windows 应用程序进行自动化交互。

> 基于 Microsoft UI Automation API，支持与大部分 Windows 应用程序的控件进行交互。

## 概述

UI Automation (UIA) 允许脚本直接操作应用程序的 UI 控件，而不是依赖坐标点击。

## 获取根元素

### 获取前台窗口 UI 根元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"Foreground window: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
if root then
    local info = root:getInfo()
    print("Foreground window: " .. info.name)
end
```

:::

### 从窗口句柄获取根元素

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

### 从坐标获取元素

:::tabs

== Python

```python:line-numbers
from wingman import input, uia

x, y = input.get_mouse_pos()
element = uia.from_point(x, y)
if element:
    info = element.get_info()
    print(f"Element: {info['name']} ({info['control_type']})")
```

== Lua

```lua:line-numbers
local input = require("wingman.input")
local uia = require("wingman.uia")

local x, y = input.getMousePos()
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print("Element: " .. info.name .. " (" .. info.controlType .. ")")
end
```

:::

---

## 通用查找方法

### 按名称查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

file_menu = uia.find_by_name("文件")
if file_menu:
    file_menu.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local fileMenu = uia.findByName("文件")
if fileMenu then
    fileMenu:click()
end
```

:::

### 按 AutomationId 查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_by_id("btnSubmit")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findById("btnSubmit")
if btn then
    btn:click()
end
```

:::

### 等待元素出现

:::tabs

== Python

```python:line-numbers
from wingman import uia

dialog = uia.wait_for_name("对话框", 3000)
if dialog:
    print("对话框已出现")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local dialog = uia.waitForName("对话框", 3000)
if dialog then
    print("对话框已出现")
end
```

:::

---

## 事件监听

### 注册属性变更事件

:::tabs

== Python

```python:line-numbers
from wingman import uia

def on_property_change(prop, value):
    print(f"属性 {prop} 变更为: {value}")

listener_id = uia.on_property_changed("编辑框", on_property_change)
if listener_id:
    print(f"监听器已注册，ID: {listener_id}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listenerId = uia.onPropertyChanged("编辑框", function(propertyName, value)
    print(string.format("属性 %s 变更为: %s", propertyName, value))
end)

if listenerId then
    print("监听器已注册，ID: " .. listenerId)
end
```

:::

### 移除事件监听器

:::tabs

== Python

```python:line-numbers
from wingman import uia

listener_id = uia.on_property_changed("按钮", lambda prop, val: print(f"属性变化: {prop}"))

if listener_id:
    uia.remove_event_listener(listener_id)
    print("监听器已移除")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listenerId = uia.onPropertyChanged("按钮", function(prop, val)
    print("属性变化: " .. prop)
end)

if listenerId then
    uia.removeEventListener(listenerId)
    print("监听器已移除")
end
```

:::

---

## UIElement 通用方法

所有 UIA 元素都继承自 UIElement，具有以下通用方法：

### 获取信息

| 方法 | 说明 |
|-----|------|
| `get_info()` | 获取元素所有属性（名称、类型、边界等） |

### 操作

| 方法 | 说明 |
|-----|------|
| `click()` | 点击元素 |
| `double_click()` | 双击元素 |
| `focus()` | 设置焦点到元素 |

### 值操作

| 方法 | 说明 |
|-----|------|
| `get_value()` | 获取当前值 |
| `set_value(value)` | 设置值 |

### 展开/折叠

| 方法 | 说明 |
|-----|------|
| `expand()` | 展开元素 |
| `collapse()` | 折叠元素 |
| `is_expanded()` | 检查是否已展开 |

### 子元素

| 方法 | 说明 |
|-----|------|
| `get_children()` | 获取所有直接子元素 |

### 选择

| 方法 | 说明 |
|-----|------|
| `select()` | 选中元素 |

---

## 可用接口

### 根元素获取

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `from_foreground()` | `fromForeground()` | 获取前台窗口的根元素 |
| `from_window(hwnd)` | `fromWindow(hwnd)` | 从窗口句柄获取根元素 |
| `from_point(x, y)` | `fromPoint(x, y)` | 从屏幕坐标获取元素 |

### 通用查找

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找元素 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |
| `find_all_by_control_type(type)` | `findAllByControlType(type)` | 查找所有指定类型的元素 |
| `wait_for_name(name, timeout)` | `waitForName(name, timeout)` | 等待元素出现 |

### 事件监听

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `on_property_changed(name, callback)` | `onPropertyChanged(name, callback)` | 注册属性变更监听器 |
| `on_structure_changed(name, callback)` | `onStructureChanged(name, callback)` | 注册结构变更监听器 |
| `remove_event_listener(id)` | `removeEventListener(id)` | 移除事件监听器 |

---

## 子模块

- [Button 按钮](./button.md)
- [Edit 编辑框](./edit.md)
- [ComboBox 下拉框](./combobox.md)
- [List 列表](./list.md)
- [CheckBox 复选框](./checkbox.md)
- [RadioButton 单选按钮](./radiobutton.md)
- [Tab 标签页](./tab.md)
- [Menu 菜单](./menu.md)
- [Tree 树形控件](./tree.md)
