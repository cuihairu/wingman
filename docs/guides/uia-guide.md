# UI Automation 指南

Wingman 的 UI Automation (UIA) 模块允许你直接与 Windows（UIAutomation）/ macOS（Accessibility）应用程序的 UI 控件交互，无需依赖坐标点击。Linux 平台暂不支持。

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

### 获取当前焦点元素

所有 UI 操作都从获取元素开始。`from_foreground()` 返回的是**当前焦点元素**（通常是正在交互的子控件，如输入框），不一定是窗口根元素；需要指定窗口的根元素时用 `from_window(hwnd)`：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取当前焦点元素
root = uia.from_foreground()
if root:
    info = root.get_info()
    print(f"元素名称: {info['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取当前焦点元素
local root = wingman.uia.from_foreground()
if root then
    local info = root.get_info()
    print("元素名称: " .. info.name)
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
local btn = wingman.uia.find_button("确定")
if btn then
    -- 点击按钮
    btn.click()
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
- Lua: `find_by_name(name: string) -> UIElement | nil`

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
local fileMenu = wingman.uia.find_by_name("文件")
if fileMenu then
    fileMenu.click()
end
```

:::

### 按 AutomationId 查找

AutomationId 是控件在开发时指定的唯一 ID，比名称更稳定：

**函数签名**
- Python: `find_by_id(id: str) -> UIElement | None`
- Lua: `find_by_id(id: string) -> UIElement | nil`

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
local btn = wingman.uia.find_by_id("btnSubmit")
if btn then
    btn.click()
end
```

:::

### 按控件类型查找

当需要查找某一类型的所有控件时：

**函数签名**
- Python: `find_all_by_control_type(role: int) -> list[UIElement]`
- Lua: `find_all_by_control_type(role: int) -> table[]`

实参是 **UIARole 数值枚举**（不是类型名字符串）。传字符串（如 `"Button"`）会被当作 0（Unknown）处理，恒返回空数组。

| 数值 | UIARole | 控件类型 |
|-----|---------|---------|
| 0 | Unknown | 未知 |
| 1 | Window | 窗口 |
| 2 | Button | 按钮 |
| 3 | TextBox | 编辑框 |
| 4 | CheckBox | 复选框 |
| 5 | RadioButton | 单选按钮 |
| 6 | ComboBox | 下拉框 |
| 7 | ListBox | 列表 |
| 8 | ListItem | 列表项 |
| 9 | Menu | 菜单 |
| 10 | MenuItem | 菜单项 |
| 11 | Table | 表格 |
| 12 | Tree | 树形控件 |
| 13 | TreeItem | 树节点 |

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找所有按钮（2 = UIARole.Button）
buttons = uia.find_all_by_control_type(2)
for btn in buttons:
    info = btn.get_info()
    print(f"按钮: {info['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找所有按钮（2 = UIARole.Button）
local buttons = wingman.uia.find_all_by_control_type(2)
for i, btn in ipairs(buttons) do
    local info = btn.get_info()
    print("按钮: " .. info.name)
end
```

:::

### 专用查找函数

对于常用控件类型，提供了专用的查找函数：

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_button(name)` | `find_button(name)` | 查找按钮 |
| `find_edit(name)` | `find_edit(name)` | 查找编辑框 |
| `find_text(name)` | `find_text(name)` | 查找文本 |

### 等待元素出现

当元素可能延迟加载时，可以等待它出现：

**函数签名**
- Python: `wait_for_name(name: str, timeout: int) -> UIElement | None`
- Lua: `wait_for_name(name: string, timeout: number) -> UIElement | nil`

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
local dialog = wingman.uia.wait_for_name("设置", 5000)
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
- Lua: `find_button(name: string) -> UIElement | nil`

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
local btn = wingman.uia.find_button("确定")
if btn then
    btn.click()
end

-- 按 AutomationId 查找（更稳定）
local btn = wingman.uia.find_by_id("btnOK")
if btn then
    btn.click()
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

local btn = wingman.uia.find_button("提交")
if btn then
    local info = btn.get_info()
    -- 检查是否启用
    if info.is_enabled then
        btn.click()
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
- Lua: `find_edit(name: string) -> UIElement | nil`

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找命名编辑框
edit = uia.find_edit("用户名")
if edit:
    edit.set_value("player123")

# 注意：name 传空字符串表示"不按名称过滤"，
# 会返回匹配到的第一个编辑框（不论其名称是否为空），
# 并非只查找"名称为空"的控件
edit = uia.find_edit("")
if edit:
    edit.set_value("some text")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找命名编辑框
local edit = wingman.uia.find_edit("用户名")
if edit then
    edit.set_value("player123")
end

-- 注意：name 传空字符串表示"不按名称过滤"，
-- 会返回匹配到的第一个编辑框（不论其名称是否为空），
-- 并非只查找"名称为空"的控件
local edit = wingman.uia.find_edit("")
if edit then
    edit.set_value("some text")
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

local edit = wingman.uia.find_edit("搜索")
if edit then
    -- 读取内容
    local currentText = edit.get_value()
    print("当前内容: " .. currentText)

    -- 设置内容
    edit.set_value("搜索关键词")

    -- 清空内容
    edit.set_value("")
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

local passwordEdit = wingman.uia.find_edit("密码")
if passwordEdit then
    -- 设置密码
    passwordEdit.set_value("mypassword123")

    -- 注意：密码框内容通常无法直接读取
    -- get_value() 会返回空或隐藏字符
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
local combo = wingman.uia.find_by_name("国家/地区")
if combo then
    -- 展开下拉框
    combo.expand()
    wingman.util.sleep(300)

    -- 选择选项（通过名称）
    local option = wingman.uia.find_by_name("中国")
    if option then
        option.click()
    end

    -- 或直接设置值（如果支持）
    combo.set_value("中国")
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
local editableCombo = wingman.uia.find_edit("搜索")
if editableCombo then
    -- 直接输入文本
    editableCombo.set_value("搜索内容")

    -- 或展开选择
    editableCombo.expand()
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

# 注意：get_info() 不含选中状态字段（is_selected 不存在），
# 列表项的选中状态脚本层暂不可查；用脚本变量维护，或点击后以界面反馈确认
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找列表
local listBox = wingman.uia.find_by_name("文件列表")
if listBox then
    -- 获取所有列表项
    local items = listBox.get_children()
    print("共有 " .. #items .. " 个项目")

    -- 遍历列表项
    for i, item in ipairs(items) do
        local info = item.get_info()
        print(string.format("[%d] %s", i, info.name))
    end
end
```

> **注意**：`get_info()` 的返回字段不含选中状态（无 `is_selected` 字段），列表项的选中状态脚本层暂不可查。可用脚本变量维护选中状态，或点击后以界面反馈确认。

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
    # （select() 未实现（计划中），选中列表项的现役替代是 click）
    if len(items) > 2:
        items[2].click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local listBox = wingman.uia.find_by_name("用户列表")
if listBox then
    local items = listBox.get_children()

    -- 点击第三个项目
    -- （select() 未实现（计划中），选中列表项的现役替代是 click）
    if #items > 2 then
        items[3].click()
    end
end
```

:::

---

### CheckBox 复选框

复选框用于多选项选择。

#### 勾选/取消勾选

**相关方法**
- `click()` - 切换勾选状态

> **注意**：勾选状态在脚本层暂不可直接设置——C++ 层有 `setChecked` 接口但未绑定到脚本。`set_value()` 走文本 ValuePattern，对 CheckBox/RadioButton 通常无效（返回 `false`）；`get_value()` 通常也读不到勾选状态。现役做法是用 `click()` 切换，并用脚本变量维护状态。

:::tabs

== Python

```python:line-numbers
from wingman import uia

is_checked = False
checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 用 click() 切换勾选（勾选状态脚本层不可直接设置）
    checkbox.click()
    is_checked = not is_checked
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local isChecked = false
local checkbox = wingman.uia.find_by_name("记住密码")
if checkbox then
    -- 用 click() 切换勾选（勾选状态脚本层不可直接设置）
    checkbox.click()
    isChecked = not isChecked
end
```

:::

#### 三态复选框

某些复选框有三种状态：选中、未选中、不确定。

> **`set_toggle_state()` 未实现（计划中）**。现役替代：`click()` 只能在选中/未选中间切换；"不确定"状态脚本层暂无法设置。

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
    # 用点击选中（set_value 对单选按钮通常无效，见上文 CheckBox 说明）
    radio.click()
    selected = "男"
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local selectedGender = nil

-- 选中单选按钮
local radio = wingman.uia.find_by_name("男")
if radio then
    -- 用点击选中（set_value 对单选按钮通常无效，见上文 CheckBox 说明）
    radio.click()
    selectedGender = "男"
end
```

:::

#### 获取选中状态

> **注意**：`get_info()` 的返回字段不含选中/勾选状态（无 `toggle_state`/`is_selected` 字段），脚本层暂无法直接查询单选按钮是否被选中。可行做法：点击时用脚本变量记录选中项，或通过界面反馈（图像/颜色检测）确认。

:::tabs

== Python

```python:line-numbers
from wingman import uia

selected_gender = None

male = uia.find_by_name("男")
if male and male.click():
    selected_gender = "男"  # 用脚本变量维护选中状态
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local selectedGender = nil

local male = wingman.uia.find_by_name("男")
if male and male.click() then
    selectedGender = "男"  -- 用脚本变量维护选中状态
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
local tabPage = wingman.uia.find_by_name("高级")
if tabPage then
    tabPage.click()
end

-- 方法 2: 通过 Tab 控件获取子元素
local tab = wingman.uia.find_by_name("设置")
if tab then
    local tabs = tab.get_children()
    -- 切换到第二个标签
    if #tabs > 1 then
        tabs[2].click()
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
- `click()` - 点击/执行菜单项（`invoke()` 未实现（计划中），菜单项用 `click()`）

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
local menu = wingman.uia.find_by_name("文件")
if menu then
    menu.expand()
    wingman.util.sleep(300)

    -- 查找并点击菜单项
    local newItem = wingman.uia.find_by_name("新建")
    if newItem then
        newItem.click()
    end
end
```

:::

#### 右键菜单（上下文菜单）

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 右键打开上下文菜单（input.click 第三参：0=左键，1=中键，2=右键）
input.click(100, 100, 2)
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 右键打开上下文菜单（input.click 第三参：0=左键，1=中键，2=右键）
wingman.input.click(100, 100, 2)
wingman.util.sleep(300)

-- 操作菜单项
local copyItem = wingman.uia.find_by_name("复制")
if copyItem then
    copyItem.click()
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

local tree = wingman.uia.find_by_name("文件夹树")
if tree then
    -- 展开节点
    local folder = wingman.uia.find_by_name("文档")
    if folder then
        folder.expand()
        wingman.util.sleep(200)
    end

    -- 折叠节点
    if folder then
        folder.collapse()
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
    local info = element.get_info()
    local prefix = depth > 0 and "└─" or ""
    print(indent .. prefix .. info.name)

    -- 递归处理子节点
    local children = element.get_children()
    for i, child in ipairs(children) do
        traverseTree(child, depth + 1)
    end
end

local tree = wingman.uia.find_by_name("文件夹树")
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
    role = info.get('role', 0)  # role 为 UIARole 数值枚举
    print(f"{indent}{name} (role={role})")

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
    local info = element.get_info()
    local name = info.name or "(无名称)"
    local role = info.role or 0  -- role 为 UIARole 数值枚举
    print(indent .. name .. " (role=" .. role .. ")")

    -- 递归遍历子元素
    local children = element.get_children()
    for i, child in ipairs(children) do
        printTree(child, depth + 1)
    end
end

local root = wingman.uia.from_foreground()
if root then
    printTree(root)
end
```

:::

### 从窗口句柄/坐标获取元素

除了 `from_foreground()`，还可以从窗口句柄获取根元素，或按屏幕坐标取该处的控件：

**函数签名**
- `from_window(hwnd)` - 从窗口句柄获取根元素
- `from_point(x, y)` - 从屏幕坐标获取元素

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

# 从窗口句柄获取根元素
hwnd, found = window.find("记事本")
if found:
    root = uia.from_window(hwnd)
    if root:
        print("记事本 UI 根元素获取成功")

# 从屏幕坐标获取元素（该坐标处的控件）
element = uia.from_point(500, 300)
if element:
    info = element.get_info()
    print(f"元素名称: {info['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 从窗口句柄获取根元素
-- （Lua 侧 window.find 返回单张 table：res[1]=句柄，res[2]=是否找到）
local res = wingman.window.find("记事本")
if res and res[2] then
    local root = wingman.uia.from_window(res[1])
    if root then
        print("记事本 UI 根元素获取成功")
    end
end

-- 从屏幕坐标获取元素（该坐标处的控件）
local element = wingman.uia.from_point(500, 300)
if element then
    local info = element.get_info()
    print("元素名称: " .. info.name)
end
```

:::

### 事件监听

UIA 支持监听元素的属性变化与 UI 树结构变化：

**函数签名**
- `on_property_changed(name, callback)` - 注册属性变更监听器，返回监听器 ID
- `on_structure_changed(name, callback)` - 注册结构变更监听器，返回监听器 ID
- `remove_event_listener(id)` - 移除监听器

> **注意**：事件回调从后台线程触发，**Lua 函数非线程安全，注册时会被拒绝**（触发 `uia.error` 事件、返回 0）。事件监听请用 Python 注册回调。
>
> 回调语义受后端事件接口限制：`on_property_changed` 的 `callback(prop, value)` 中 `prop` 为触发事件的元素名称、`value` 为元素当前文本，不区分具体变更的属性名；`on_structure_changed` 的 `callback()` 无参数。

:::tabs

== Python

```python:line-numbers
from wingman import uia

def on_property_change(prop, value):
    print(f"属性变更: {prop} -> {value}")

listener_id = uia.on_property_changed("编辑框", on_property_change)
if listener_id:
    print(f"监听器已注册，ID: {listener_id}")

def on_structure_change():
    print("UI 结构发生变化")

uia.on_structure_changed("列表", on_structure_change)

# 不再需要时移除监听器
# uia.remove_event_listener(listener_id)
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

#### 状态查询

| 方法 | 说明 |
|-----|------|
| `is_visible()` | 检查元素是否可见 |
| `is_enabled()` | 检查元素是否启用 |

#### 子元素

| 方法 | 说明 |
|-----|------|
| `get_children()` | 获取所有直接子元素 |

#### 选择

> `select()` 未实现（计划中）。选中列表项/单选按钮的现役替代是 `click()`。

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
-- （Lua 侧 window.find 返回单张 table：res[1]=句柄，res[2]=是否找到）
local res = wingman.window.find("登录")
if res and res[2] then
    local hwnd = res[1]
    wingman.window.activate(hwnd)
    wingman.util.sleep(500)

    -- 查找用户名输入框
    local username = wingman.uia.find_edit("用户名")
    if username then
        username.set_value("myusername")
    end

    -- 查找密码输入框
    local password = wingman.uia.find_edit("密码")
    if password then
        password.set_value("mypassword")
    end

    -- 点击登录按钮
    local loginBtn = wingman.uia.find_button("登录")
    if loginBtn then
        loginBtn.click()
        print("登录请求已发送")
    end
end
```

:::
