# UI Automation 指南

Wingman 的 UI Automation (UIA) 模块允许你直接与 Windows 应用程序的 UI 控件交互，无需依赖坐标点击。

## 目录

- [基础概念](#基础概念)
- [控件详解](#控件详解)
  - [Button 按钮](#button-按钮)
  - [Edit 编辑框](#edit-编辑框)
  - [Text 静态文本](#text-静态文本)
  - [ComboBox 下拉框](#combobox-下拉框)
  - [ListBox 列表框](#listbox-列表框)
  - [CheckBox 复选框](#checkbox-复选框)
  - [RadioButton 单选按钮](#radiobutton-单选按钮)
  - [Tab 标签页](#tab-标签页)
  - [Menu 菜单](#menu-菜单)
  - [TreeView 树形控件](#treeview-树形控件)
  - [ListView 列表视图](#listview-列表视图)
  - [ScrollBar 滚动条](#scrollbar-滚动条)
  - [ProgressBar 进度条](#progressbar-进度条)
  - [Slider 滑块](#slider-滑块)
  - [ToolTip 工具提示](#tooltip-工具提示)
- [高级技巧](#高级技巧)

## 基础概念

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

### 控件类型

| ControlType | 中文名称 | 典型应用 |
|-------------|---------|---------|
| Button | 按钮 | 确认、取消、提交 |
| Edit | 编辑框 | 用户名、密码、搜索 |
| Text | 静态文本 | 标签、提示信息 |
| ComboBox | 下拉框 | 国家选择、选项列表 |
| ListBox | 列表框 | 文件列表、项目选择 |
| CheckBox | 复选框 | 同意条款、记住密码 |
| RadioButton | 单选按钮 | 性别选择、唯一选项 |
| Tab | 标签页 | 设置分类、多页内容 |
| Menu | 菜单 | 文件菜单、右键菜单 |
| Tree | 树形控件 | 文件夹树、组织结构 |
| List | 列表视图 | 表格、项目列表 |
| ScrollBar | 滚动条 | 窗口/面板滚动 |
| ProgressBar | 进度条 | 加载进度、下载进度 |
| Slider | 滑块 | 音量控制、亮度调节 |
| ToolTip | 工具提示 | 按钮说明、字段帮助 |

---

## 控件详解

### Button 按钮

按钮是最常见的控件，用于触发操作。

#### 属性

```python
{
    'name': '确定',           # 按钮文本
    'control_type': 'Button',
    'is_enabled': True,       # 是否可用
    'is_invoke_pattern': True # 支持点击
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找按钮
btn = uia.find_button("确定")
if btn:
    # 获取按钮信息
    info = btn.get_info()
    print(f"按钮名称: {info['name']}")
    print(f"是否可用: {info.get('is_enabled', True)}")

    # 点击按钮
    btn.click()

# 通过 AutomationId 查找（更稳定）
btn = uia.find_by_id("btnOK")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找按钮
local btn = uia.findButton("确定")
if btn then
    -- 获取按钮信息
    local info = btn:getInfo()
    print("按钮名称: " .. info.name)
    print("是否可用: " .. tostring(info.isEnabled))

    -- 点击按钮
    btn:click()
end

-- 通过 AutomationId 查找（更稳定）
local btn = uia.findById("btnOK")
if btn then
    btn:click()
end
```

:::

#### 检测按钮状态

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")

# 检查是否可用
if btn:
    info = btn.get_info()
    if info.get('is_enabled', True):
        print("按钮可用，可以点击")
        btn.click()
    else:
        print("按钮禁用，不可点击")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local btn = uia.findButton("提交")

-- 检查是否可用
if btn then
    local info = btn:getInfo()
    if info.isEnabled then
        print("按钮可用，可以点击")
        btn:click()
    else
        print("按钮禁用，不可点击")
    end
end
```

:::

---

### Edit 编辑框

编辑框用于输入和显示文本。

#### 属性

```python
{
    'name': '用户名',
    'control_type': 'Edit',
    'value': 'player123',     # 当前值
    'is_readonly': False,      # 是否只读
    'is_password': False       # 是否密码框
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找编辑框
edit = uia.find_edit("用户名")
if edit:
    # 读取内容
    text = edit.get_value()
    print(f"当前内容: {text}")

    # 设置内容
    edit.set_value("player123")

    # 清空内容
    edit.set_value("")

# 查找空名称的编辑框（常见情况）
edit = uia.find_edit("")
if edit:
    edit.set_value("some text")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找编辑框
local edit = uia.findEdit("用户名")
if edit then
    -- 读取内容
    local text = edit:getValue()
    print("当前内容: " .. text)

    -- 设置内容
    edit:setValue("player123")

    -- 清空内容
    edit:setValue("")
end

-- 查找空名称的编辑框（常见情况）
local edit = uia.findEdit("")
if edit then
    edit:setValue("some text")
end
```

:::

#### 密码框操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 密码框通常也是 Edit 类型
password_edit = uia.find_edit("密码")
if password_edit:
    # 设置密码
    password_edit.set_value("mypassword123")

    # 注意：无法直接读取密码框内容
    # get_value() 对密码框返回空或隐藏字符
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 密码框通常也是 Edit 类型
local passwordEdit = uia.findEdit("密码")
if passwordEdit then
    -- 设置密码
    passwordEdit:setValue("mypassword123")

    -- 注意：无法直接读取密码框内容
    -- getValue() 对密码框返回空或隐藏字符
end
```

:::

#### 只读编辑框

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 只读编辑框通常用于显示信息
readonly_edit = uia.find_edit("状态信息")
if readonly_edit:
    info = readonly_edit.get_info()
    if info.get('is_readonly', False):
        text = readonly_edit.get_value()
        print(f"状态: {text}")
        # 不能设置只读编辑框的值
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 只读编辑框通常用于显示信息
local readonlyEdit = uia.findEdit("状态信息")
if readonlyEdit then
    local info = readonlyEdit:getInfo()
    if info.isReadOnly then
        local text = readonlyEdit:getValue()
        print("状态: " .. text)
        -- 不能设置只读编辑框的值
    end
end
```

:::

---

### Text 静态文本

静态文本用于显示标签和提示信息，通常不可交互。

#### 属性

```python
{
    'name': '请输入用户名：',
    'control_type': 'Text',
    'is_readonly': True
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找文本元素
text = uia.find_text("欢迎使用")
if text:
    info = text.get_info()
    print(f"文本内容: {info['name']}")

# Text 主要用于读取，不能修改
label = uia.find_text("用户名：")
if label:
    info = label.get_info()
    print(f"标签文本: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找文本元素
local text = uia.findText("欢迎使用")
if text then
    local info = text:getInfo()
    print("文本内容: " .. info.name)
end

-- Text 主要用于读取，不能修改
local label = uia.findText("用户名：")
if label then
    local info = label:getInfo()
    print("标签文本: " .. info.name)
end
```

:::

---

### ComboBox 下拉框

下拉框用于从选项列表中选择一个值。

#### 属性

```python
{
    'name': '国家/地区',
    'control_type': 'ComboBox',
    'value': '中国',
    'is_expandable': True  # 是否可展开
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找下拉框
combo = uia.find_by_name("国家/地区")
if combo:
    # 获取当前选中值
    info = combo.get_info()
    print(f"当前选择: {info.get('value', '')}")

    # 展开下拉框
    combo.expand()
    util.sleep(300)

    # 选择选项
    option = uia.find_by_name("中国")
    if option:
        option.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 查找下拉框
local combo = uia.findByName("国家/地区")
if combo then
    -- 获取当前选中值
    local info = combo:getInfo()
    print("当前选择: " .. (info.value or ""))

    -- 展开下拉框
    combo:expand()
    util.sleep(300)

    -- 选择选项
    local option = uia.findByName("中国")
    if option then
        option:click()
    end
end
```

:::

#### 可编辑下拉框

某些下拉框允许手动输入：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 可编辑下拉框也是 Edit 类型
editable_combo = uia.find_edit("搜索")
if editable_combo:
    # 直接输入
    editable_combo.set_value("搜索内容")

    # 或展开选择
    editable_combo.expand()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 可编辑下拉框也是 Edit 类型
local editableCombo = uia.findEdit("搜索")
if editableCombo then
    -- 直接输入
    editableCombo:setValue("搜索内容")

    -- 或展开选择
    editableCombo:expand()
end
```

:::

---

### ListBox 列表框

列表框显示可选择的项列表。

#### 属性

```python
{
    'name': '文件列表',
    'control_type': 'ListBox',
    'can_select_multiple': False  # 是否多选
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 查找列表框
list_box = uia.find_by_name("文件列表")
if list_box:
    # 获取所有列表项
    items = list_box.get_children()
    print(f"共有 {len(items)} 个项目")

    # 点击第三个项目
    if len(items) > 2:
        target = items[2]
        target.click()
        util.sleep(200)

        # 双击打开
        input.double_click_from_element(target)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 查找列表框
local listBox = uia.findByName("文件列表")
if listBox then
    -- 获取所有列表项
    local items = listBox:getChildren()
    print("共有 " .. #items .. " 个项目")

    -- 点击第三个项目
    if #items > 2 then
        local target = items[3]
        target:click()
        util.sleep(200)

        -- 双击打开
        input.doubleClickFromElement(target)
    end
end
```

:::

#### 遍历列表项

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("用户列表")
if list_box:
    items = list_box.get_children()

    for i, item in enumerate(items):
        info = item.get_info()
        print(f"[{i}] {info['name']}")

        # 获取选中状态
        if info.get('is_selected', False):
            print(f"  -> 已选中")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("用户列表")
if listBox then
    local items = listBox:getChildren()

    for i, item in ipairs(items) do
        local info = item:getInfo()
        print(string.format("[%d] %s", i, info.name))

        -- 获取选中状态
        if info.isSelected then
            print("  -> 已选中")
        end
    end
end
```

:::

---

### CheckBox 复选框

复选框用于多选项选择。

#### 属性

```python
{
    'name': '记住密码',
    'control_type': 'CheckBox',
    'is_toggle_pattern': True,
    'toggle_state': 'On'  # On/Off/Indeterminate
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找复选框
checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 获取当前状态
    info = checkbox.get_info()
    state = info.get('toggle_state', 'Off')
    print(f"当前状态: {state}")

    # 勾选
    checkbox.set_value(True)

    # 取消勾选
    checkbox.set_value(False)

    # 切换
    checkbox.set_value(not checkbox.get_value())
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找复选框
local checkbox = uia.findByName("记住密码")
if checkbox then
    -- 获取当前状态
    local info = checkbox:getInfo()
    local state = info.toggleState or "Off"
    print("当前状态: " .. state)

    -- 勾选
    checkbox:setValue(true)

    -- 取消勾选
    checkbox:setValue(false)

    -- 切换
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
    # 设置为不确定状态
    checkbox.set_toggle_state('Indeterminate')

    # 设置为选中
    checkbox.set_toggle_state('On')

    # 设置为未选中
    checkbox.set_toggle_state('Off')
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("全选")
if checkbox then
    -- 设置为不确定状态
    checkbox:setToggleState("Indeterminate")

    -- 设置为选中
    checkbox:setToggleState("On")

    -- 设置为未选中
    checkbox:setToggleState("Off")
end
```

:::

---

### RadioButton 单选按钮

单选按钮用于从多个选项中选择一个。

#### 属性

```python
{
    'name': '男',
    'control_type': 'RadioButton',
    'is_toggle_pattern': True,
    'toggle_state': 'On',
    'selection_item': True
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找单选按钮
radio = uia.find_by_name("男")
if radio:
    # 选中单选按钮
    radio.set_value(True)

    # 单选按钮通常通过点击来选中
    radio.click()

# 获取性别选择
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
local uia = require("wingman.uia")

-- 查找单选按钮
local radio = uia.findByName("男")
if radio then
    -- 选中单选按钮
    radio:setValue(true)

    -- 单选按钮通常通过点击来选中
    radio:click()
end

-- 获取性别选择
local male = uia.findByName("男")
local female = uia.findByName("女")

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

#### 属性

```python
{
    'name': '设置',
    'control_type': 'Tab',
    'selection_container': True
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找标签控件
tab = uia.find_by_name("设置")
if tab:
    # 获取所有标签页
    tabs = tab.get_children()
    print(f"共有 {len(tabs)} 个标签页")

    # 切换到第二个标签
    if len(tabs) > 1:
        tabs[1].click()

# 直接点击标签页
general_tab = uia.find_by_name("常规")
if general_tab:
    general_tab.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找标签控件
local tab = uia.findByName("设置")
if tab then
    -- 获取所有标签页
    local tabs = tab:getChildren()
    print("共有 " .. #tabs .. " 个标签页")

    -- 切换到第二个标签
    if #tabs > 1 then
        tabs[2]:click()
    end
end

-- 直接点击标签页
local generalTab = uia.findByName("常规")
if generalTab then
    generalTab:click()
end
```

:::

#### 获取当前活动标签

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()

    for tab_page in tabs:
        info = tab_page.get_info()
        # 检查是否被选中
        if info.get('selection_state', 0) == 1:
            print(f"当前活动标签: {info['name']}")
            break
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    local tabs = tab:getChildren()

    for i, tabPage in ipairs(tabs) do
        local info = tabPage:getInfo()
        -- 检查是否被选中
        if info.selectionState == 1 then
            print("当前活动标签: " .. info.name)
            break
        end
    end
end
```

:::

---

### Menu 菜单

菜单用于组织命令和选项。

#### 属性

```python
{
    'name': '文件',
    'control_type': 'Menu',
    'is_control_element': True
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找菜单
menu = uia.find_by_name("文件")
if menu:
    # 展开菜单
    menu.expand()
    util.sleep(300)

    # 查找菜单项
    new_item = uia.find_by_name("新建")
    if new_item:
        new_item.click()

# 或直接点击菜单项（某些应用支持）
save_item = uia.find_by_name("保存")
if save_item:
    save_item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 查找菜单
local menu = uia.findByName("文件")
if menu then
    -- 展开菜单
    menu:expand()
    util.sleep(300)

    -- 查找菜单项
    local newItem = uia.findByName("新建")
    if newItem then
        newItem:click()
    end
end

-- 或直接点击菜单项（某些应用支持）
local saveItem = uia.findByName("保存")
if saveItem then
    saveItem:click()
end
```

:::

#### 右键菜单（上下文菜单）

:::tabs

== Python

```python:line-numbers
from wingman import uia, input

# 先右键打开上下文菜单
input.right_click(100, 100)

# 等待菜单出现
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 先右键打开上下文菜单
input.rightClick(100, 100)

-- 等待菜单出现
util.sleep(300)

-- 操作菜单项
local copyItem = uia.findByName("复制")
if copyItem then
    copyItem:click()
end
```

:::

---

### TreeView 树形控件

树形控件用于显示层次结构数据。

#### 属性

```python
{
    'name': '文件夹树',
    'control_type': 'Tree',
    'can_select_multiple': False
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 查找树控件
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

    # 双击节点展开/折叠
    folder.double_click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 查找树控件
local tree = uia.findByName("文件夹树")
if tree then
    -- 展开节点
    local folder = uia.findByName("文档")
    if folder then
        folder:expand()
        util.sleep(200)
    end

    -- 折叠节点
    if folder then
        folder:collapse()
    end

    -- 双击节点展开/折叠
    folder:doubleClick()
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
local uia = require("wingman.uia")

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

local tree = uia.findByName("文件夹树")
if tree then
    traverseTree(tree)
end
```

:::

---

### ListView 列表视图

列表视图以表格形式显示数据。

#### 属性

```python
{
    'name': '进程列表',
    'control_type': 'List',
    'can_select_multiple': True
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找列表视图
list_view = uia.find_by_name("进程列表")
if list_view:
    # 获取所有数据项
    items = list_view.get_children()
    print(f"共有 {len(items)} 个进程")

    # 第一项通常是表头
    if items and items[0].get_info().get('control_type') == 'Header':
        items = items[1:]  # 跳过表头

    # 选择第一个项
    if items:
        items[0].select()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找列表视图
local listView = uia.findByName("进程列表")
if listView then
    -- 获取所有数据项
    local items = listView:getChildren()
    print("共有 " .. #items .. " 个进程")

    -- 第一项通常是表头
    if items and items[1]:getInfo().controlType == "Header" then
        table.remove(items, 1)  -- 跳过表头
    end

    -- 选择第一个项
    if #items > 0 then
        items[1]:select()
    end
end
```

:::

#### 获取列表项内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_view = uia.find_by_name("进程列表")
if list_view:
    items = list_view.get_children()

    for item in items:
        # 获取列表项的所有子元素（各列）
        columns = item.get_children()
        row_data = []
        for col in columns:
            info = col.get_info()
            row_data.append(info.get('name', ''))

        print(f"行数据: {' | '.join(row_data)}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listView = uia.findByName("进程列表")
if listView then
    local items = listView:getChildren()

    for i, item in ipairs(items) do
        -- 获取列表项的所有子元素（各列）
        local columns = item:getChildren()
        local rowData = {}
        for j, col in ipairs(columns) do
            local info = col:getInfo()
            table.insert(rowData, info.name or "")
        end

        print("行数据: " .. table.concat(rowData, " | "))
    end
end
```

:::

---

### ScrollBar 滚动条

滚动条用于滚动内容区域。

#### 属性

```python
{
    'name': '垂直滚动条',
    'control_type': 'ScrollBar',
    'is_horizontal': False
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找滚动条
v_scroll = uia.find_by_name("垂直滚动条")
if v_scroll:
    # 获取滚动位置
    info = v_scroll.get_info()
    print(f"当前位置: {info.get('value', 0)}")

    # 设置滚动位置（百分比 0-100）
    v_scroll.set_value(50)  # 滚动到中间

# 水平滚动条
h_scroll = uia.find_by_name("水平滚动条")
if h_scroll:
    h_scroll.set_value(100)  # 滚动到最右侧
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找滚动条
local vScroll = uia.findByName("垂直滚动条")
if vScroll then
    -- 获取滚动位置
    local info = vScroll:getInfo()
    print("当前位置: " .. (info.value or 0))

    -- 设置滚动位置（百分比 0-100）
    vScroll:setValue(50)  -- 滚动到中间
end

-- 水平滚动条
local hScroll = uia.findByName("水平滚动条")
if hScroll then
    hScroll:setValue(100)  -- 滚动到最右侧
end
```

:::

---

### ProgressBar 进度条

进度条显示操作进度。

#### 属性

```python
{
    'name': '下载进度',
    'control_type': 'ProgressBar',
    'value': 75.0,           # 进度值
    'minimum': 0.0,
    'maximum': 100.0
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找进度条
progress = uia.find_by_name("下载进度")
if progress:
    # 获取进度值
    info = progress.get_info()
    value = info.get('value', 0)
    minimum = info.get('minimum', 0)
    maximum = info.get('maximum', 100)

    percent = (value - minimum) / (maximum - minimum) * 100
    print(f"进度: {percent:.1f}%")

    # 进度条通常只读，不能设置
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找进度条
local progress = uia.findByName("下载进度")
if progress then
    -- 获取进度值
    local info = progress:getInfo()
    local value = info.value or 0
    local minimum = info.minimum or 0
    local maximum = info.maximum or 100

    local percent = (value - minimum) / (maximum - minimum) * 100
    print(string.format("进度: %.1f%%", percent))

    -- 进度条通常只读，不能设置
end
```

:::

#### 等待进度完成

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def wait_for_progress_complete(progress_name, timeout=30000):
    """等待进度完成"""
    start_time = util.time()

    while util.time() - start_time < timeout:
        progress = uia.find_by_name(progress_name)
        if progress:
            info = progress.get_info()
            value = info.get('value', 0)
            maximum = info.get('maximum', 100)

            if value >= maximum:
                print("进度完成！")
                return True

        util.sleep(500)

    print("等待超时")
    return False

# 使用
wait_for_progress_complete("安装进度")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local function waitForProgressComplete(progressName, timeout)
    timeout = timeout or 30000
    local startTime = util.time()

    while util.time() - startTime < timeout do
        local progress = uia.findByName(progressName)
        if progress then
            local info = progress:getInfo()
            local value = info.value or 0
            local maximum = info.maximum or 100

            if value >= maximum then
                print("进度完成！")
                return true
            end
        end

        util.sleep(500)
    end

    print("等待超时")
    return false
end

-- 使用
waitForProgressComplete("安装进度")
```

:::

---

### Slider 滑块

滑块用于调节数值。

#### 属性

```python
{
    'name': '音量',
    'control_type': 'Slider',
    'value': 50,
    'minimum': 0,
    'maximum': 100
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找滑块
slider = uia.find_by_name("音量")
if slider:
    # 获取当前值
    info = slider.get_info()
    print(f"当前值: {info.get('value', 0)}")

    # 设置值
    slider.set_value(80)  # 设置为 80

    # 设置为最大值
    slider.set_value(info.get('maximum', 100))

    # 设置为最小值
    slider.set_value(info.get('minimum', 0))
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找滑块
local slider = uia.findByName("音量")
if slider then
    -- 获取当前值
    local info = slider:getInfo()
    print("当前值: " .. (info.value or 0))

    -- 设置值
    slider:setValue(80)  -- 设置为 80

    -- 设置为最大值
    slider:setValue(info.maximum or 100)

    -- 设置为最小值
    slider:setValue(info.minimum or 0)
end
```

:::

---

### ToolTip 工具提示

工具提示显示鼠标悬停时的帮助信息。

#### 属性

```python
{
    'name': '点击查看详情',
    'control_type': 'ToolTip',
    'help_text': '...'
}
```

#### 基本操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 悬停在按钮上触发工具提示
btn = uia.find_button("帮助")
if btn:
    # 移动鼠标到按钮
    info = btn.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        center_x = rect['left'] + rect['width'] // 2
        center_y = rect['top'] + rect['height'] // 2
        input.move_mouse(center_x, center_y)

        # 等待工具提示出现
        util.sleep(1000)

        # 查找工具提示
        tooltip = uia.find_by_control_type("ToolTip")
        if tooltip:
            text = tooltip.get_name()
            print(f"工具提示: {text}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 悬停在按钮上触发工具提示
local btn = uia.findButton("帮助")
if btn then
    -- 移动鼠标到按钮
    local info = btn:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2
        input.moveMouse(centerX, centerY)

        -- 等待工具提示出现
        util.sleep(1000)

        -- 查找工具提示
        local tooltip = uia.findByControlType("ToolTip")
        if tooltip then
            local text = tooltip:getInfo().name
            print("工具提示: " .. text)
        end
    end
end
```

:::

---

## 高级技巧

### 按控件类型查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()

# 查找所有按钮
buttons = uia.find_all_by_control_type("Button")
for btn in buttons:
    info = btn.get_info()
    print(f"按钮: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()

-- 查找所有按钮
local buttons = uia.findAllByControlType("Button")
for i, btn in ipairs(buttons) do
    local info = btn:getInfo()
    print("按钮: " .. info.name)
end
```

:::

### 等待控件出现

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待按钮出现（最多5秒）
btn = uia.wait_for_name("确定", 5000)
if btn:
    btn.click()
else:
    print("按钮未出现")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 等待按钮出现（最多5秒）
local btn = uia.waitForName("确定", 5000)
if btn then
    btn:click()
else
    print("按钮未出现")
end
```

:::

### 事件监听

:::tabs

== Python

```python:line-numbers
from wingman import uia

def on_property_change(prop_name, value):
    print(f"属性 {prop_name} 变为: {value}")

def on_structure_change():
    print("UI 结构变化")

# 注册监听器
prop_id = uia.on_property_changed("编辑框", on_property_change)
struct_id = uia.on_structure_changed("列表", on_structure_change)

# ... 等待事件 ...

# 移除监听器
uia.remove_event_listener(prop_id)
uia.remove_event_listener(struct_id)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local function onPropertyChanged(propName, value)
    print(string.format("属性 %s 变为: %s", propName, value))
end

local function onStructureChanged()
    print("UI 结构变化")
end

-- 注册监听器
local propId = uia.onPropertyChanged("编辑框", onPropertyChanged)
local structId = uia.onStructureChanged("列表", onStructureChanged)

-- ... 等待事件 ...

-- 移除监听器
uia.removeEventListener(propId)
uia.removeEventListener(structId)
```

:::
