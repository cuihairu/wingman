# API: wingman.uia

UI Automation 模块，用于与 Windows 应用程序进行自动化交互。

> 基于 Microsoft UI Automation API，支持与大部分 Windows 应用程序的控件进行交互。

## 概述

UI Automation (UIA) 允许脚本直接操作应用程序的 UI 控件，而不是依赖坐标点击。

### 支持的控件类型

- **Button** - 按钮
- **Edit** - 文本输入框
- **Text** - 静态文本
- **ComboBox** - 下拉框
- **List** - 列表
- **CheckBox** - 复选框
- **RadioButton** - 单选按钮
- **Tab** - 标签页
- **Menu** - 菜单
- **Window** - 窗口

## 获取前台窗口 UI 根元素

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

## 从窗口句柄获取根元素

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

## 从坐标获取元素

:::tabs

== Python

```python:line-numbers
from wingman import input, uia

# 获取鼠标位置
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

-- 获取鼠标位置
local x, y = input.getMousePos()
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print("Element: " .. info.name .. " (" .. info.controlType .. ")")
end
```

:::

## 按名称查找元素

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

## 查找按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

ok_btn = uia.find_button("确定")
if ok_btn:
    ok_btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local okBtn = uia.findButton("确定")
if okBtn then
    okBtn:click()
end
```

:::

## 查找编辑框

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("")
if edit:
    edit.set_value("Hello, UI Automation!")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("")
if edit then
    edit:setValue("Hello, UI Automation!")
end
```

:::

## 等待元素出现

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

## 注册属性变更事件

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

## 移除事件监听器

:::tabs

== Python

```python:line-numbers
from wingman import uia

listener_id = uia.on_property_changed("按钮", lambda prop, val: print(f"属性变化: {prop}"))

# 稍后移除监听器
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

-- 稍后移除监听器
if listenerId then
    uia.removeEventListener(listenerId)
    print("监听器已移除")
end
```

:::

---

## UIElement 对象方法

### 获取元素信息

:::tabs

== Python

```python:line-numbers
from wingman import uia

element = uia.from_foreground()
info = element.get_info()
print(f"Name: {info['name']}, Type: {info['control_type']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local element = uia.fromForeground()
local info = element:getInfo()
print(string.format("Name: %s, Type: %s", info.name, info.controlType))
```

:::

### 点击元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

element.click()
```

== Lua

```lua:line-numbers
element:click()
```

:::

### 设置焦点

:::tabs

== Python

```python:line-numbers
from wingman import uia

element.focus()
```

== Lua

```lua:line-numbers
element:focus()
```

:::

### 获取/设置值

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取值
value = element.get_value()

# 设置值
element.set_value("Hello World")
```

== Lua

```lua:line-numbers
-- 获取值
local value = element:getValue()

-- 设置值
element:setValue("Hello World")
```

:::

### 获取子元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
children = root.get_children()
for i, child in enumerate(children):
    info = child.get_info()
    print(f"[{i}] {info['name']} - {info['control_type']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
local children = root:getChildren()
for i, child in ipairs(children) do
    local info = child:getInfo()
    print(string.format("[%d] %s - %s", i, info.name, info.controlType))
end
```

:::

### 展开/折叠元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

tree = uia.find_by_name("树形控件")
if tree and not tree.is_expanded():
    tree.expand()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tree = uia.findByName("树形控件")
if tree and not tree:isExpanded() then
    tree:expand()
end
```

:::

---

## 完整示例

:::tabs

== Python

```python:line-numbers
from wingman import window, uia, process, util

# 等待记事本窗口
print("等待记事本窗口...")
hwnd, found = window.find("记事本")
if not found:
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")
    util.sleep(1000)
    hwnd, found = window.find("记事本")

if not found:
    print("错误: 无法找到记事本窗口")
    exit(1)

print("找到记事本窗口")

# 激活窗口
window.activate(hwnd)
util.sleep(200)

# 获取 UI 根元素
root = uia.from_foreground()
if not root:
    print("错误: 无法获取 UI Automation 元素")
    exit(1)

print("UI Automation 已初始化")

# 获取所有子元素
print("\n=== 记事本 UI 元素 ===")
children = root.get_children()
for i, child in enumerate(children):
    info = child.get_info()
    print(f"[{i}] {info['name']} - {info['class_name']} (类型: {info['control_type']})")

# 查找编辑框
print("\n=== 查找编辑框 ===")
edit = uia.find_edit("")
if edit:
    edit.set_value("Hello from UI Automation!\n这是通过 Python 脚本输入的文本。\n")
    print("已设置文本")

    value = edit.get_value()
    print(f"当前文本长度: {len(value)}")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")
local process = require("wingman.process")
local util = require("wingman.util")

-- 等待记事本窗口
print("等待记事本窗口...")
local hwnd, found = window.find("记事本")
if not found then
    print("未找到记事本，尝试启动...")
    process.start("notepad.exe")
    util.sleep(1000)
    hwnd, found = window.find("记事本")
end

if not found then
    print("错误: 无法找到记事本窗口")
    return
end

print("找到记事本窗口")

-- 激活窗口
window.activate(hwnd)
util.sleep(200)

-- 获取 UI 根元素
local root = uia.fromForeground()
if not root then
    print("错误: 无法获取 UI Automation 元素")
    return
end

print("UI Automation 已初始化")

-- 获取所有子元素
print("\n=== 记事本 UI 元素 ===")
local children = root:getChildren()
for i, child in ipairs(children) do
    local info = child:getInfo()
    print(string.format("[%d] %s - %s (类型: %s)",
        i, info.name, info.className, info.controlType))
end

-- 查找编辑框
print("\n=== 查找编辑框 ===")
local edit = uia.findEdit("")
if edit then
    edit:setValue("Hello from UI Automation!\n这是通过 Lua 脚本输入的文本。\n")
    print("已设置文本")

    local value = edit:getValue()
    print("当前文本长度: " .. #value)
end
```

:::

---

## 可用接口

### `from_foreground()` / `fromForeground()`

获取前台窗口的 UI Automation 根元素。

### `from_window(hwnd)` / `fromWindow(hwnd)`

从窗口句柄获取 UI Automation 根元素。

### `from_point(x, y)` / `fromPoint(x, y)`

从屏幕坐标获取 UI 元素。

### `find_by_name(name)` / `findByName(name)`

在前台窗口中查找指定名称的元素。

### `find_by_id(id)` / `findById(id)`

在前台窗口中查找指定 Automation ID 的元素。

### `find_button(name)` / `findButton(name)`

查找按钮控件。

### `find_edit(name)` / `findEdit(name)`

查找编辑框控件。

### `find_text(name)` / `findText(name)`

查找文本控件。

### `wait_for_name(name, timeout)` / `waitForName(name, timeout)`

等待指定名称的元素出现。

### `on_property_changed(name, callback)` / `onPropertyChanged(name, callback)`

注册属性变更事件监听器。

### `on_structure_changed(name, callback)` / `onStructureChanged(name, callback)`

注册结构变更事件监听器。

### `remove_event_listener(listener_id)` / `removeEventListener(listenerId)`

移除事件监听器。
