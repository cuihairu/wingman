# API: wingman.uia

UI Automation 模块，用于与 Windows 应用程序的 UI 控件进行自动化交互。

## 什么是 UI Automation

UI Automation (UIA) 是 Microsoft 提供的辅助功能框架，最早出现在 Windows Vista 中。它的设计初衷是帮助视障、听障用户通过屏幕阅读器等辅助技术使用计算机，但后来也被广泛用于自动化测试和 UI 自动化。

### UIA 的工作原理

UIA 通过 **UI Automation Provider** 和 **UI Automation Client** 两个组件工作：

1. **UIA Provider** - 应用程序或操作系统提供的组件，将 UI 元素暴露给外部
2. **UIA Client** - 像我们这样的自动化脚本，通过 UIA 接口访问 UI 元素

当你调用 `uia.from_foreground()` 时：
1. Windows UIA API 返回前台窗口的根元素
2. 根元素是一个 UIElement 对象，代表整个窗口
3. 你可以通过根元素遍历整个 UI 树，查找和操作子元素

### UI 元素树

Windows 应用程序的 UI 被组织成树形结构。以记事本为例：

```
Desktop (桌面)
└── Notepad (记事本窗口) - Window
    ├── Menu Bar (菜单栏) - Menu/MenuBar
    │   ├── File (文件菜单) - MenuItem
    │   │   ├── New (新建) - MenuItem
    │   │   ├── Open (打开) - MenuItem
    │   │   └── Save (保存) - MenuItem
    │   ├── Edit (编辑菜单) - MenuItem
    │   └── Help (帮助菜单) - MenuItem
    ├── Text Editor (文本编辑区) - Edit
    └── Status Bar (状态栏) - Text
```

每个节点都是一个 **UIElement**，具有以下属性：
- **Name** - 控件的显示名称（如"确定"、"用户名"）
- **ControlType** - 控件类型（如 Button、Edit、ComboBox）
- **AutomationId** - 开发者设置的唯一 ID（最稳定的查找方式）
- **BoundingRect** - 控件的屏幕位置和大小
- **IsEnabled** - 控件是否可用
- **IsVisible** - 控件是否可见

### 支持的控件类型

| ControlType | 中文名称 | 典型应用 | 可操作内容 |
|-------------|---------|---------|-----------|
| Button | 按钮 | 确认、取消、提交 | 点击 |
| Edit | 编辑框 | 用户名、密码、搜索 | 读写文本 |
| Text | 静态文本 | 标签、提示信息 | 读取文本 |
| ComboBox | 下拉框 | 国家选择、选项列表 | 选择选项、展开/折叠 |
| List | 列表 | 文件列表、项目选择 | 选择项目、遍历 |
| CheckBox | 复选框 | 同意条款、记住密码 | 勾选/取消 |
| RadioButton | 单选按钮 | 性别选择、唯一选项 | 选择 |
| Tab | 标签页 | 设置分类、多页内容 | 切换页面 |
| Menu | 菜单 | 文件菜单、右键菜单 | 展开、选择菜单项 |
| Tree | 树形控件 | 文件夹树、组织结构 | 展开/折叠节点、选择 |
| Window | 窗口 | 应用程序主窗口、对话框 | 激活、关闭 |
| ScrollBar | 滚动条 | 窗口/面板滚动 | 滚动 |
| ProgressBar | 进度条 | 加载进度、下载进度 | 读取进度（只读） |
| Slider | 滇块 | 音量控制、亮度调节 | 调节值 |
| ToolTip | 工具提示 | 按钮说明、字段帮助 | 读取提示文本 |

### UIA vs 坐标点击

| 特性 | UIA | 坐标点击 |
|-----|-----|---------|
| 稳定性 | 高 - 控件变化时仍可工作 | 低 - UI 移动即失效 |
| 准确性 | 高 - 直接操作目标控件 | 低 - 可能点错位置 |
| 维护性 | 好 - 控件 ID 不变则无需修改 | 差 - 每次调整都需要重新获取坐标 |
| 适用范围 | Windows 原生应用、大多数现代应用 | 任何有图形界面的应用 |
| 局限性 | 某些游戏、自定义 UI 可能不支持 | 无 |

### 为什么使用 UIA

传统的自动化脚本依赖屏幕坐标点击，这种方式存在以下问题：

1. **脆弱**：窗口移动、分辨率改变都会导致坐标失效
2. **不可靠**：UI 变化时容易点击错误位置
3. **难维护**：每次 UI 调整都需要重新获取坐标

UIA 解决了这些问题：
- **稳定**：直接操作控件，不依赖坐标
- **准确**：通过控件名称/ID 精确定位
- **易维护**：UI 结构变化时脚本依然可用

### 查找控件的优先级

推荐按以下优先级查找控件：

1. **AutomationId** - 最稳定，开发者设置的唯一 ID
   ```python
   btn = uia.find_by_id("btnSubmit")
   ```

2. **Name + ControlType** - 次稳定，显示名称可能变化
   ```python
   btn = uia.find_button("确定")
   ```

3. **纯名称** - 可能找到多个同名元素
   ```python
   element = uia.find_by_name("确定")
   ```

---

## 获取根元素

所有 UI 操作都从获取根元素开始。根元素通常是前台窗口。

### 获取前台窗口的根元素

这是最常用的方式，获取当前活动窗口的 UI 根元素：

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"窗口名称: {info['name']}")
    print(f"控件类型: {info['control_type']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
if root then
    local info = root:getInfo()
    print("窗口名称: " .. info.name)
    print("控件类型: " .. info.controlType)
end
```

:::

### 从窗口句柄获取根元素

如果已经知道窗口句柄，可以直接获取其 UI 根元素：

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

可以根据屏幕坐标获取该位置的 UI 元素：

:::tabs

== Python

```python:line-numbers
from wingman import input, uia

x, y = input.get_mouse_pos()
element = uia.from_point(x, y)
if element:
    info = element.get_info()
    print(f"元素名称: {info['name']}")
    print(f"控件类型: {info['control_type']}")
```

== Lua

```lua:line-numbers
local input = require("wingman.input")
local uia = require("wingman.uia")

local x, y = input.getMousePos()
local element = uia.fromPoint(x, y)
if element then
    local info = element:getInfo()
    print("元素名称: " .. info.name)
    print("控件类型: " .. info.controlType)
end
```

:::

---

## 通用查找方法

### 按名称查找

通过控件的 Name 属性查找。这是最直观的方式，但 Name 可能变化或重复。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"文件"的菜单
file_menu = uia.find_by_name("文件")
if file_menu:
    file_menu.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找名为"文件"的菜单
local fileMenu = uia.findByName("文件")
if fileMenu then
    fileMenu:click()
end
```

:::

### 按 AutomationId 查找

通过控件的 AutomationId 查找。这是最稳定的方式，推荐用于生产环境。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 通过 AutomationId 查找（推荐用于生产环境）
btn = uia.find_by_id("btnSubmit")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 通过 AutomationId 查找（推荐用于生产环境）
local btn = uia.findById("btnSubmit")
if btn then
    btn:click()
end
```

:::

### 等待元素出现

当元素可能延迟加载时，可以轮询等待它出现：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待对话框出现（最多等待 3 秒）
dialog = uia.wait_for_name("对话框", 3000)
if dialog:
    print("对话框已出现")
else:
    print("等待超时")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 等待对话框出现（最多等待 3 秒）
local dialog = uia.waitForName("对话框", 3000)
if dialog then
    print("对话框已出现")
else
    print("等待超时")
end
```

:::

---

## UIElement 通用方法

所有 UIA 元素都继承自 UIElement，具有以下通用方法。这些方法适用于所有控件类型。

### 获取元素信息

`get_info()` 返回一个包含元素所有属性的对象：

:::tabs

== Python

```python:line-numbers
from wingman import uia

element = uia.find_button("确定")
if element:
    info = element.get_info()
    print(f"名称: {info.get('name', '')}")
    print(f"类型: {info.get('control_type', '')}")
    print(f"AutomationId: {info.get('automation_id', '')}")
    print(f"启用: {info.get('is_enabled', True)}")
    print(f"可见: {info.get('is_visible', True)}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local element = uia.findButton("确定")
if element then
    local info = element:getInfo()
    print("名称: " .. (info.name or ""))
    print("类型: " .. (info.controlType or ""))
    print("AutomationId: " .. (info.automationId or ""))
    print("启用: " .. tostring(info.isEnabled or true))
    print("可见: " .. tostring(info.isVisible or true))
end
```

:::

### 点击元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("确定")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("确定")
if btn then
    btn:click()
end
```

:::

### 双击元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

item = uia.find_by_name("文件.txt")
if item:
    item.double_click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local item = uia.findByName("文件.txt")
if item then
    item:doubleClick()
end
```

:::

### 设置焦点

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    edit.focus()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("用户名")
if edit then
    edit:focus()
end
```

:::

### 获取/设置值

适用于有值的控件（如编辑框、下拉框等）：

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    # 获取值
    value = edit.get_value()
    print(f"当前值: {value}")

    # 设置值
    edit.set_value("搜索关键词")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("搜索")
if edit then
    -- 获取值
    local value = edit:getValue()
    print("当前值: " .. value)

    -- 设置值
    edit:setValue("搜索关键词")
end
```

:::

### 获取子元素

遍历 UI 树的关键方法：

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()
if root:
    children = root.get_children()
    for i, child in enumerate(children):
        info = child.get_info()
        print(f"[{i}] {info['name']} ({info['control_type']})")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()
if root then
    local children = root:getChildren()
    for i, child in ipairs(children) do
        local info = child:getInfo()
        print(string.format("[%d] %s (%s)", i, info.name, info.controlType))
    end
end
```

:::

### 展开/折叠元素

适用于可展开的控件（如菜单、树节点、下拉框等）：

:::tabs

== Python

```python:line-numbers
from wingman import uia

menu = uia.find_by_name("文件")
if menu:
    # 展开
    menu.expand()

    # 检查是否已展开
    if menu.is_expanded():
        print("菜单已展开")

    # 折叠
    menu.collapse()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local menu = uia.findByName("文件")
if menu then
    -- 展开
    menu:expand()

    -- 检查是否已展开
    if menu:isExpanded() then
        print("菜单已展开")
    end

    -- 折叠
    menu:collapse()
end
```

:::

---

## 事件监听

UIA 支持监听 UI 元素的属性变化和结构变化事件。

### 注册属性变更事件

当元素的属性（如名称、值、启用状态等）发生变化时触发：

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

### 注册结构变更事件

当 UI 树结构发生变化（如添加/删除子元素）时触发：

:::tabs

== Python

```python:line-numbers
from wingman import uia

def on_structure_change():
    print("UI 结构发生变化")

listener_id = uia.on_structure_changed("列表", on_structure_change)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listenerId = uia.onStructureChanged("列表", function()
    print("UI 结构发生变化")
end)
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

### 专用查找

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_button(name)` | `findButton(name)` | 查找按钮控件 |
| `find_edit(name)` | `findEdit(name)` | 查找编辑框控件 |
| `find_text(name)` | `findText(name)` | 查找文本控件 |

### 事件监听

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `on_property_changed(name, callback)` | `onPropertyChanged(name, callback)` | 注册属性变更监听器 |
| `on_structure_changed(name, callback)` | `onStructureChanged(name, callback)` | 注册结构变更监听器 |
| `remove_event_listener(id)` | `removeEventListener(id)` | 移除事件监听器 |

---

## 子模块

- [Button 按钮](./button.md) - 按钮控件的详细文档
- [Edit 编辑框](./edit.md) - 编辑框控件的详细文档
- [ComboBox 下拉框](./combobox.md) - 下拉框控件的详细文档
- [List 列表](./list.md) - 列表控件的详细文档
- [CheckBox 复选框](./checkbox.md) - 复选框控件的详细文档
- [RadioButton 单选按钮](./radiobutton.md) - 单选按钮控件的详细文档
- [Tab 标签页](./tab.md) - 标签页控件的详细文档
- [Menu 菜单](./menu.md) - 菜单控件的详细文档
- [Tree 树形控件](./tree.md) - 树形控件的详细文档
