# UI Automation 指南

Wingman 的 UI Automation (UIA) 模块允许你直接与 Windows 应用程序的 UI 控件交互，无需依赖坐标点击。

## 目录

- [基础概念](#基础概念)
- [快速开始](#快速开始)
- [元素查找](#元素查找)
- [控件详解](#控件详解)
- [高级技巧](#高级技巧)

---

## 基础概念

### 什么是 UI Automation

UI Automation (UIA) 是 Microsoft 提供的辅助功能框架，它将应用程序的 UI 暴露为一棵可访问的元素树。每个按钮、编辑框、菜单等都是一个 UI 元素，可以通过程序访问和操作。

### 为什么使用 UIA

传统的自动化脚本依赖屏幕坐标点击，这种方式存在以下问题：
- **脆弱**：窗口移动、分辨率改变都会导致坐标失效
- **不可靠**：UI 变化时容易点击错误位置
- **难维护**：每次 UI 调整都需要重新获取坐标

UIA 解决了这些问题：
- **稳定**：直接操作控件，不依赖坐标
- **准确**：通过控件名称/ID 精确定位
- **易维护**：UI 结构变化时脚本依然可用

### UI 元素树

Windows 应用程序的 UI 被组织成树形结构：

```
桌面
└── 记事本窗口
    ├── 菜单栏
    │   ├── 文件菜单
    │   │   ├── 新建
    │   │   ├── 打开
    │   │   └── 保存
    │   ├── 编辑菜单
    │   └── 帮助菜单
    ├── 文本编辑区 (Edit)
    └── 状态栏 (Text)
```

每个节点都是一个 UIElement 对象，可以获取其属性、执行操作、遍历子元素。

### 控件类型

| ControlType | 中文名称 | 典型应用 |
|-------------|---------|---------|
| Button | 按钮 | 确认、取消、提交 |
| Edit | 编辑框 | 用户名、密码、搜索 |
| Text | 静态文本 | 标签、提示信息 |
| ComboBox | 下拉框 | 国家选择、选项列表 |
| List | 列表 | 文件列表、项目选择 |
| CheckBox | 复选框 | 同意条款、记住密码 |
| RadioButton | 单选按钮 | 性别选择、唯一选项 |
| Tab | 标签页 | 设置分类、多页内容 |
| Menu | 菜单 | 文件菜单、右键菜单 |
| Tree | 树形控件 | 文件夹树、组织结构 |
| Window | 窗口 | 应用程序主窗口 |

---

## 快速开始

### 获取 UI 根元素

所有 UI 操作都从获取根元素开始。根元素通常是前台窗口：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取前台窗口的根元素
root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"窗口名称: {info['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取前台窗口的根元素
local root = wingman.uia.fromForeground()
if root then
    local info = root:getInfo()
    print("窗口名称: " .. info.name)
end
```

:::

### 查找并操作控件

获取根元素后，可以查找并操作具体控件：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"确定"的按钮
btn = uia.find_button("确定")
if btn:
    # 点击按钮
    btn.click()
    print("已点击确定按钮")
else:
    print("未找到确定按钮")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"确定"的按钮
local btn = wingman.uia.findButton("确定")
if btn then
    -- 点击按钮
    btn:click()
    print("已点击确定按钮")
else
    print("未找到确定按钮")
end
```

:::

---

## 元素查找

### 按名称查找

最常用的查找方式是通过控件的显示名称（Name 属性）：

**函数签名**
- Python: `find_by_name(name: str) -> UIElement | None`
- Lua: `findByName(name: string) -> UIElement | nil`

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
local wingman = require("wingman")

-- 查找名为"文件"的菜单
local fileMenu = wingman.uia.findByName("文件")
if fileMenu then
    fileMenu:click()
end
```

:::

### 按 AutomationId 查找

AutomationId 是控件在开发时指定的唯一 ID，比名称更稳定：

**函数签名**
- Python: `find_by_id(id: str) -> UIElement | None`
- Lua: `findById(id: string) -> UIElement | nil`

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
local wingman = require("wingman")

-- 通过 AutomationId 查找（推荐用于生产环境）
local btn = wingman.uia.findById("btnSubmit")
if btn then
    btn:click()
end
```

:::

### 按控件类型查找

当需要查找某一类型的所有控件时：

**函数签名**
- Python: `find_all_by_control_type(control_type: str) -> list[UIElement]`
- Lua: `findAllByControlType(controlType: string) -> table[]`

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找所有按钮
buttons = uia.find_all_by_control_type("Button")
for btn in buttons:
    info = btn.get_info()
    print(f"按钮: {info['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找所有按钮
local buttons = wingman.uia.findAllByControlType("Button")
for i, btn in ipairs(buttons) do
    local info = btn:getInfo()
    print("按钮: " .. info.name)
end
```

:::

### 专用查找函数

对于常用控件类型，提供了专用的查找函数：

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_button(name)` | `findButton(name)` | 查找按钮 |
| `find_edit(name)` | `findEdit(name)` | 查找编辑框 |
| `find_text(name)` | `findText(name)` | 查找文本 |

### 等待元素出现

当元素可能延迟加载时，可以等待它出现：

**函数签名**
- Python: `wait_for_name(name: str, timeout: int) -> UIElement | None`
- Lua: `waitForName(name: string, timeout: number) -> UIElement | nil`

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待对话框出现（最多等待 5 秒）
dialog = uia.wait_for_name("设置", 5000)
if dialog:
    print("对话框已出现")
else:
    print("等待超时")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 等待对话框出现（最多等待 5 秒）
local dialog = wingman.uia.waitForName("设置", 5000)
if dialog then
    print("对话框已出现")
else
    print("等待超时")
end
```

:::

---

## 控件详解

### Button 按钮

按钮是最常见的控件，用于触发操作。

#### 查找按钮

**函数签名**
- Python: `find_button(name: str) -> UIElement | None`
- Lua: `findButton(name: string) -> UIElement | nil`

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 按名称查找
btn = uia.find_button("确定")
if btn:
    btn.click()

# 按 AutomationId 查找（更稳定）
btn = uia.find_by_id("btnOK")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 按名称查找
local btn = wingman.uia.findButton("确定")
if btn then
    btn:click()
end

-- 按 AutomationId 查找（更稳定）
local btn = wingman.uia.findById("btnOK")
if btn then
    btn:click()
end
```

:::

#### 检测按钮状态

按钮可能有启用/禁用状态，操作前应检查：

**相关方法**
- `get_info()` - 获取元素信息，包含 `is_enabled` 字段

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")
if btn:
    info = btn.get_info()
    # 检查是否启用
    if info.get('is_enabled', True):
        btn.click()
        print("已点击")
    else:
        print("按钮已禁用")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.findButton("提交")
if btn then
    local info = btn:getInfo()
    -- 检查是否启用
    if info.isEnabled then
        btn:click()
        print("已点击")
    else
        print("按钮已禁用")
    end
end
```

:::

---

### Edit 编辑框

编辑框用于输入和显示文本。

#### 查找编辑框

**函数签名**
- Python: `find_edit(name: str) -> UIElement | None`
- Lua: `findEdit(name: string) -> UIElement | nil`

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找命名编辑框
edit = uia.find_edit("用户名")
if edit:
    edit.set_value("player123")

# 查找空名称的编辑框（常见于表单）
edit = uia.find_edit("")
if edit:
    edit.set_value("some text")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找命名编辑框
local edit = wingman.uia.findEdit("用户名")
if edit then
    edit:setValue("player123")
end

-- 查找空名称的编辑框（常见于表单）
local edit = wingman.uia.findEdit("")
if edit then
    edit:setValue("some text")
end
```

:::

#### 读写编辑框内容

**相关方法**
- `get_value()` - 获取当前文本内容
- `set_value(text)` - 设置文本内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    # 读取内容
    current_text = edit.get_value()
    print(f"当前内容: {current_text}")

    # 设置内容
    edit.set_value("搜索关键词")

    # 清空内容
    edit.set_value("")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.findEdit("搜索")
if edit then
    -- 读取内容
    local currentText = edit:getValue()
    print("当前内容: " .. currentText)

    -- 设置内容
    edit:setValue("搜索关键词")

    -- 清空内容
    edit:setValue("")
end
```

:::

#### 密码框操作

密码框也是 Edit 类型，但通常无法读取内容：

:::tabs

== Python

```python:line-numbers
from wingman import uia

password_edit = uia.find_edit("密码")
if password_edit:
    # 设置密码
    password_edit.set_value("mypassword123")

    # 注意：密码框内容通常无法直接读取
    # get_value() 会返回空或隐藏字符
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local passwordEdit = wingman.uia.findEdit("密码")
if passwordEdit then
    -- 设置密码
    passwordEdit:setValue("mypassword123")

    -- 注意：密码框内容通常无法直接读取
    -- getValue() 会返回空或隐藏字符
end
```

:::

---

### ComboBox 下拉框

下拉框用于从选项列表中选择一个值。

#### 基本操作

**相关方法**
- `expand()` - 展开下拉框
- `collapse()` - 折叠下拉框

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找下拉框
combo = uia.find_by_name("国家/地区")
if combo:
    # 展开下拉框
    combo.expand()
    util.sleep(300)

    # 选择选项（通过名称）
    option = uia.find_by_name("中国")
    if option:
        option.click()

    # 或直接设置值（如果支持）
    combo.set_value("中国")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找下拉框
local combo = wingman.uia.findByName("国家/地区")
if combo then
    -- 展开下拉框
    combo:expand()
    wingman.util.sleep(300)

    -- 选择选项（通过名称）
    local option = wingman.uia.findByName("中国")
    if option then
        option:click()
    end

    -- 或直接设置值（如果支持）
    combo:setValue("中国")
end
```

:::

#### 可编辑下拉框

某些下拉框允许手动输入，这种情况下控件实际类型是 Edit：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 可编辑下拉框也是 Edit 类型
editable_combo = uia.find_edit("搜索")
if editable_combo:
    # 直接输入文本
    editable_combo.set_value("搜索内容")

    # 或展开选择
    editable_combo.expand()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 可编辑下拉框也是 Edit 类型
local editableCombo = wingman.uia.findEdit("搜索")
if editableCombo then
    -- 直接输入文本
    editableCombo:setValue("搜索内容")

    -- 或展开选择
    editableCombo:expand()
end
```

:::

---

### List 列表

列表显示可选择的项目集合。

#### 遍历列表项

**相关方法**
- `get_children()` - 获取所有子元素

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找列表
list_box = uia.find_by_name("文件列表")
if list_box:
    # 获取所有列表项
    items = list_box.get_children()
    print(f"共有 {len(items)} 个项目")

    # 遍历列表项
    for i, item in enumerate(items):
        info = item.get_info()
        print(f"[{i}] {info['name']}")

        # 检查是否选中
        if info.get('is_selected', False):
            print(f"  -> 已选中")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找列表
local listBox = wingman.uia.findByName("文件列表")
if listBox then
    -- 获取所有列表项
    local items = listBox:getChildren()
    print("共有 " .. #items .. " 个项目")

    -- 遍历列表项
    for i, item in ipairs(items) do
        local info = item:getInfo()
        print(string.format("[%d] %s", i, info.name))

        -- 检查是否选中
        if info.isSelected then
            print("  -> 已选中")
        end
    end
end
```

:::

#### 选择列表项

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("用户列表")
if list_box:
    items = list_box.get_children()

    # 点击第三个项目
    if len(items) > 2:
        items[2].click()

    # 或使用 select 方法（如果支持）
    if len(items) > 2:
        items[2].select()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local listBox = wingman.uia.findByName("用户列表")
if listBox then
    local items = listBox:getChildren()

    -- 点击第三个项目
    if #items > 2 then
        items[3]:click()
    end

    -- 或使用 select 方法（如果支持）
    if #items > 2 then
        items[3]:select()
    end
end
```

:::

---

### CheckBox 复选框

复选框用于多选项选择。

#### 勾选/取消勾选

**相关方法**
- `get_value()` / `set_value(bool)` - 获取/设置勾选状态
- `set_toggle_state(state)` - 设置三态：'On', 'Off', 'Indeterminate'

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 勾选
    checkbox.set_value(True)

    # 取消勾选
    checkbox.set_value(False)

    # 切换状态
    current = checkbox.get_value()
    checkbox.set_value(not current)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    -- 勾选
    checkbox:setValue(true)

    -- 取消勾选
    checkbox:setValue(false)

    -- 切换状态
    local current = checkbox:getValue()
    checkbox:setValue(not current)
end
```

:::

#### 三态复选框

某些复选框有三种状态：选中、未选中、不确定：

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("全选")
if checkbox:
    # 设置为选中
    checkbox.set_toggle_state('On')

    # 设置为未选中
    checkbox.set_toggle_state('Off')

    # 设置为不确定状态
    checkbox.set_toggle_state('Indeterminate')
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("全选")
if checkbox then
    -- 设置为选中
    checkbox:setToggleState("On")

    -- 设置为未选中
    checkbox:setToggleState("Off")

    -- 设置为不确定状态
    checkbox:setToggleState("Indeterminate")
end
```

:::

---

### RadioButton 单选按钮

单选按钮用于从多个选项中选择一个。

#### 选择单选按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 选中单选按钮
radio = uia.find_by_name("男")
if radio:
    # 方法 1: 点击
    radio.click()

    # 方法 2: 设置值
    radio.set_value(True)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 选中单选按钮
local radio = wingman.uia.findByName("男")
if radio then
    -- 方法 1: 点击
    radio:click()

    -- 方法 2: 设置值
    radio:setValue(true)
end
```

:::

#### 获取选中状态

:::tabs

== Python

```python:line-numbers
from wingman import uia

male = uia.find_by_name("男")
female = uia.find_by_name("女")

if male and female:
    male_info = male.get_info()
    female_info = female.get_info()

    if male_info.get('toggle_state') == 'On':
        print("选择了：男")
    elif female_info.get('toggle_state') == 'On':
        print("选择了：女")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local male = wingman.uia.findByName("男")
local female = wingman.uia.findByName("女")

if male and female then
    local maleInfo = male:getInfo()
    local femaleInfo = female:getInfo()

    if maleInfo.toggleState == "On" then
        print("选择了：男")
    elseif femaleInfo.toggleState == "On" then
        print("选择了：女")
    end
end
```

:::

---

### Tab 标签页

标签页用于组织多页内容。

#### 切换标签页

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 方法 1: 直接点击标签页
tab_page = uia.find_by_name("高级")
if tab_page:
    tab_page.click()

# 方法 2: 通过 Tab 控件获取子元素
tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()
    # 切换到第二个标签
    if len(tabs) > 1:
        tabs[1].click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 方法 1: 直接点击标签页
local tabPage = wingman.uia.findByName("高级")
if tabPage then
    tabPage:click()
end

-- 方法 2: 通过 Tab 控件获取子元素
local tab = wingman.uia.findByName("设置")
if tab then
    local tabs = tab:getChildren()
    -- 切换到第二个标签
    if #tabs > 1 then
        tabs[2]:click()
    end
end
```

:::

---

### Menu 菜单

菜单用于组织命令和选项。

#### 展开菜单并选择

**相关方法**
- `expand()` - 展开菜单
- `invoke()` - 执行菜单项

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找并展开菜单
menu = uia.find_by_name("文件")
if menu:
    menu.expand()
    util.sleep(300)

    # 查找并点击菜单项
    new_item = uia.find_by_name("新建")
    if new_item:
        new_item.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找并展开菜单
local menu = wingman.uia.findByName("文件")
if menu then
    menu:expand()
    wingman.util.sleep(300)

    -- 查找并点击菜单项
    local newItem = wingman.uia.findByName("新建")
    if newItem then
        newItem:click()
    end
end
```

:::

#### 右键菜单（上下文菜单）

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 右键打开上下文菜单
input.right_click(100, 100)
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 右键打开上下文菜单
wingman.input.rightClick(100, 100)
wingman.util.sleep(300)

-- 操作菜单项
local copyItem = wingman.uia.findByName("复制")
if copyItem then
    copyItem:click()
end
```

:::

---

### Tree 树形控件

树形控件用于显示层次结构数据。

#### 展开/折叠节点

**相关方法**
- `expand()` - 展开节点
- `collapse()` - 折叠节点

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

tree = uia.find_by_name("文件夹树")
if tree:
    # 展开节点
    folder = uia.find_by_name("文档")
    if folder:
        folder.expand()
        util.sleep(200)

    # 折叠节点
    if folder:
        folder.collapse()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tree = wingman.uia.findByName("文件夹树")
if tree then
    -- 展开节点
    local folder = wingman.uia.findByName("文档")
    if folder then
        folder:expand()
        wingman.util.sleep(200)
    end

    -- 折叠节点
    if folder then
        folder:collapse()
    end
end
```

:::

#### 遍历树节点

:::tabs

== Python

```python:line-numbers
from wingman import uia

def traverse_tree(element, depth=0):
    """递归遍历树节点"""
    indent = "  " * depth
    info = element.get_info()
    print(f"{indent}{'└─' if depth > 0 else ''}{info['name']}")

    # 递归处理子节点
    children = element.get_children()
    for child in children:
        traverse_tree(child, depth + 1)

tree = uia.find_by_name("文件夹树")
if tree:
    traverse_tree(tree)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function traverseTree(element, depth)
    depth = depth or 0
    local indent = string.rep("  ", depth)
    local info = element:getInfo()
    local prefix = depth > 0 and "└─" or ""
    print(indent .. prefix .. info.name)

    -- 递归处理子节点
    local children = element:getChildren()
    for i, child in ipairs(children) do
        traverseTree(child, depth + 1)
    end
end

local tree = wingman.uia.findByName("文件夹树")
if tree then
    traverseTree(tree)
end
```

:::

---

## 高级技巧

### 遍历 UI 树

当需要遍历整个 UI 树时：

:::tabs

== Python

```python:line-numbers
from wingman import uia

def print_tree(element, depth=0):
    """打印 UI 树结构"""
    indent = "  " * depth
    info = element.get_info()
    name = info.get('name', '') or '(无名称)'
    ctrl_type = info.get('control_type', 'Unknown')
    print(f"{indent}{name} ({ctrl_type})")

    # 递归遍历子元素
    children = element.get_children()
    for child in children:
        print_tree(child, depth + 1)

root = uia.from_foreground()
if root:
    print_tree(root)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function printTree(element, depth)
    depth = depth or 0
    local indent = string.rep("  ", depth)
    local info = element:getInfo()
    local name = info.name or "(无名称)"
    local ctrlType = info.controlType or "Unknown"
    print(indent .. name .. " (" .. ctrlType .. ")")

    -- 递归遍历子元素
    local children = element:getChildren()
    for i, child in ipairs(children) do
        printTree(child, depth + 1)
    end
end

local root = wingman.uia.fromForeground()
if root then
    printTree(root)
end
```

:::

### UIElement 对象完整方法列表

#### 获取信息

| 方法 | 说明 |
|-----|------|
| `get_info()` | 获取元素所有属性（名称、类型、边界等） |

#### 操作

| 方法 | 说明 |
|-----|------|
| `click()` | 点击元素 |
| `double_click()` | 双击元素 |
| `focus()` | 设置焦点到元素 |

#### 值操作

| 方法 | 说明 |
|-----|------|
| `get_value()` | 获取当前值 |
| `set_value(value)` | 设置值 |

#### 展开/折叠

| 方法 | 说明 |
|-----|------|
| `expand()` | 展开元素 |
| `collapse()` | 折叠元素 |
| `is_expanded()` | 检查是否已展开 |

#### 子元素

| 方法 | 说明 |
|-----|------|
| `get_children()` | 获取所有直接子元素 |

#### 选择

| 方法 | 说明 |
|-----|------|
| `select()` | 选中元素 |

---

### 完整示例：自动登录

下面是一个完整的登录自动化示例：

:::tabs

== Python

```python:line-numbers
from wingman import window, uia, util

# 激活登录窗口
hwnd, found = window.find("登录")
if found:
    window.activate(hwnd)
    util.sleep(500)

    # 查找用户名输入框
    username = uia.find_edit("用户名")
    if username:
        username.set_value("myusername")

    # 查找密码输入框
    password = uia.find_edit("密码")
    if password:
        password.set_value("mypassword")

    # 点击登录按钮
    login_btn = uia.find_button("登录")
    if login_btn:
        login_btn.click()
        print("登录请求已发送")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 激活登录窗口
local hwnd, found = wingman.window.find("登录")
if found then
    wingman.window.activate(hwnd)
    wingman.util.sleep(500)

    -- 查找用户名输入框
    local username = wingman.uia.findEdit("用户名")
    if username then
        username:setValue("myusername")
    end

    -- 查找密码输入框
    local password = wingman.uia.findEdit("密码")
    if password then
        password:setValue("mypassword")
    end

    -- 点击登录按钮
    local loginBtn = wingman.uia.findButton("登录")
    if loginBtn then
        loginBtn:click()
        print("登录请求已发送")
    end
end
```

:::
