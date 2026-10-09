# API: wingman.uia

UI Automation 模块，用于与 UI 控件进行自动化交互。

> [**已实现**：根元素获取（`from_foreground`/`from_window`/`from_point`）+ 通用查找（`find_by_name`/`find_by_id`/`find_all_by_control_type`/`wait_for_name`/`wait_for_id`/`wait_for_role`/`wait_for`）+ 专用查找（`find_button`/`find_edit`/`find_text`/`find_check_box`/`find_radio_button`/`find_combo_box`/`find_list`/`find_list_item`/`find_tree`/`find_tree_item`/`find_menu_item`/`find_hyperlink`/`find_image`/`find_slider`/`find_spinner`/`find_progress_bar`/`find_tab`/`find_tab_item`）+ UIElement 对象 15 方法（`get_info`/`click`/`double_click`/`focus`/`get_value`/`set_value`/`get_children`/`get_parent`/`expand`/`collapse`/`is_expanded`/`is_visible`/`is_enabled`/`select`/`get_selection`）+ 事件监听（`on_property_changed`/`on_structure_changed`/`remove_event_listener`）。支持 **Windows UIAutomation（COM 事件处理线程）+ macOS Accessibility（AXObserver run loop）** 双平台（Linux 暂不支持）。]

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

每个节点都是一个 **UIElement**，脚本层通过 `get_info()` 可读取以下属性：
- **name** - 控件的显示名称（如"确定"、"用户名"）
- **id** - 开发者设置的唯一 ID（对应 UIA AutomationId，最稳定的查找方式）
- **className** - 控件类名
- **role** - 控件角色（int，对应 UIARole 枚举，见下方对照表）
- **text** - 控件文本内容
- **is_enabled** - 控件是否可用
- **is_visible** - 控件是否可见
- **has_focus** - 控件是否持有焦点
- **bounds** - 控件的屏幕位置和大小（`{x, y, width, height}`）

### 支持的控件类型与 UIARole 对照

`find_all_by_control_type(role)` 的实参是 **UIARole 角色数值（int）**，不是字符串——传字符串会被当成 0（Unknown），匹配不到任何控件。全部角色取值如下：

| UIARole 数值 | 枚举名 | 中文名称 | 典型应用 | 脚本层可操作 |
|-------------|--------|---------|---------|------------|
| 0 | Unknown | 未知 | 无法归类时 | 通用方法 |
| 1 | Window | 窗口 | 应用程序主窗口、对话框 | 获取信息、遍历子元素 |
| 2 | Button | 按钮 | 确认、取消、提交 | 点击、双击 |
| 3 | TextBox | 编辑框 | 用户名、密码、搜索 | 读写文本 |
| 4 | CheckBox | 复选框 | 同意条款、记住密码 | 点击切换 |
| 5 | RadioButton | 单选按钮 | 性别选择、唯一选项 | 点击选中 |
| 6 | ComboBox | 下拉框 | 国家选择、选项列表 | 展开/折叠、遍历选项 |
| 7 | ListBox | 列表 | 文件列表、项目选择 | 遍历列表项、点击选择 |
| 8 | ListItem | 列表项 | 列表中的单个项目 | 点击、双击 |
| 9 | Menu | 菜单 | 文件菜单、右键菜单 | 展开、点击菜单项 |
| 10 | MenuItem | 菜单项 | 菜单中的单个命令 | 点击 |
| 11 | Table | 表格 | 数据表格 | 遍历子元素 |
| 12 | Tree | 树形控件 | 文件夹树、组织结构 | 展开/折叠节点、遍历 |
| 13 | TreeItem | 树节点 | 树中的单个节点 | 点击、展开/折叠 |
| 14 | Hyperlink | 超链接 | 网页/文档链接 | 点击 |
| 15 | Image | 图像 | 图标、图片控件 | 获取信息 |
| 16 | Slider | 滑块 | 音量、进度调节 | 点击 |
| 17 | Spinner | 微调器 | 数值增减控件 | 点击 |
| 18 | ProgressBar | 进度条 | 加载/安装进度 | 获取信息 |
| 19 | Tab | 标签页容器 | 选项卡控件 | 遍历标签项 |
| 20 | TabItem | 标签项 | 单个选项卡 | 点击切换 |

> **注意**：Text（静态文本）、ScrollBar（滚动条）、ToolTip（工具提示）在 UIARole 枚举中**没有专用角色值**，无法按角色过滤。查找这些控件请使用按名称匹配的 `find_by_name` / `find_text`（`find_text` 不做角色过滤，可能命中任意类型的同名元素）。各类型实际可用的操作见对应子模块文档。

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

#### 1. AutomationId（最推荐）

**什么是 AutomationId？**

AutomationId 是控件开发者在编写代码时设置的唯一标识符。就像每个人都有身份证号一样，每个控件也可以有一个 AutomationId。

**为什么 AutomationId 最稳定？**

- **开发者指定**：AutomationId 是开发者在代码中写死的，通常不会改变
- **唯一性**：在一个窗口内，AutomationId 通常是唯一的
- **语言无关**：即使界面语言从"确定"改成"OK"，AutomationId 仍然不变

**如何获取 AutomationId？**

可以使用以下脚本查看任意控件的 AutomationId：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 获取前台窗口
root = uia.from_foreground()
if root:
    # 递归打印所有控件的 id（对应 UIA AutomationId）
    def print_element_ids(element, depth=0):
        indent = "  " * depth
        info = element["get_info"]()
        name = info.get('name', '') or '(无名称)'
        role = info.get('role', 0)
        auto_id = info.get('id', '')

        print(f"{indent}{name} (role={role})")
        if auto_id:
            print(f"{indent}  └─ id: {auto_id}")

        # 递归子元素
        children = element["get_children"]()
        for child in children:
            print_element_ids(child, depth + 1)

    print_element_ids(root)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取前台窗口
local root = wingman.uia.from_foreground()
if root then
    -- 递归打印所有控件的 id（对应 UIA AutomationId）
    local function printElementIds(element, depth)
        depth = depth or 0
        local indent = string.rep("  ", depth)
        local info = element:get_info()
        local name = info.name or "(无名称)"
        local role = info.role or 0
        local autoId = info.id or ""

        print(indent .. name .. " (role=" .. role .. ")")
        if autoId ~= "" then
            print(indent .. "  └─ id: " .. autoId)
        end

        -- 递归子元素
        local children = element:get_children()
        for i, child in ipairs(children) do
            printElementIds(child, depth + 1)
        end
    end

    printElementIds(root)
end
```

:::

运行上述脚本后，你会看到类似这样的输出：

```
记事本 (role=1)
  └─ id: NotepadWindow
文件 (role=10)
  └─ id: MenuItem_File
新建 (role=10)
  └─ id: MenuItem_New
  (role=3)
  └─ id: TextBox1
```

然后你就可以使用 id（AutomationId）来查找控件：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 使用 AutomationId 查找（最稳定）
btn = uia.find_by_id("btnSubmit")
if btn:
    btn["click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 使用 AutomationId 查找（最稳定）
local btn = wingman.uia.find_by_id("btnSubmit")
if btn then
    btn:click()
end
```

:::

#### 2. Name + ControlType（次推荐）

**什么是 Name？**

Name 是控件的显示文本，也就是用户在界面上看到的文字。比如按钮上显示的"确定"、标签显示的"用户名："等。

**为什么不如 AutomationId 稳定？**

- **可能变化**：界面改版或语言切换时，显示文本可能改变
- **可能重复**：同一个窗口内可能有多个同名控件
- **可能为空**：很多控件没有显示名称

但 Name 的优势是直观，你可以直接看到按钮上写什么就用什么来查找：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 使用专用的查找函数（推荐）
btn = uia.find_button("确定")
if btn:
    btn["click"]()

# 或使用通用查找（不太推荐，可能找到其他控件）
element = uia.find_by_name("确定")
if element and element["get_info"]().get('role', 0) == 2:  # 2 = UIARole Button
    element["click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 使用专用的查找函数（推荐）
local btn = wingman.uia.find_button("确定")
if btn then
    btn:click()
end

-- 或使用通用查找（不太推荐，可能找到其他控件）
local element = wingman.uia.find_by_name("确定")
if element then
    local info = element:get_info()
    if info.role == 2 then  -- 2 = UIARole Button
        element:click()
    end
end
```

:::

#### 3. 纯名称查找（不推荐）

直接按名称查找可能找到多个同名的控件，不够精确：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 先按角色取全部按钮（UIARole 2 = Button），再按名称筛选
elements = uia.find_all_by_control_type(2)
for element in elements:
    info = element["get_info"]()
    if info.get('name') == "确定":
        print(f"找到: {info['name']} (role={info['role']})")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 可能找到多个"确定"按钮
-- 注意：先按角色取全部按钮（UIARole 2 = Button），再按名称筛选
local elements = wingman.uia.find_all_by_control_type(2)
for i, element in ipairs(elements) do
    local info = element:get_info()
    if info.name == "确定" then
        print("找到确定按钮")
    end
end
```

:::

---

## 查找控件的最佳实践

### 推荐做法

1. **开发阶段**：使用上面的脚本打印 AutomationId，记录下关键控件的 ID
2. **生产脚本**：优先使用 AutomationId 查找
3. **备用方案**：当 AutomationId 不存在时，再使用 Name 查找
4. **组合使用**：同时检查 AutomationId 和 Name，确保准确性

### 示例：健壮的控件查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

def find_submit_button():
    """健壮地查找提交按钮"""

    # 方法 1: 优先使用 AutomationId
    btn = uia.find_by_id("btnSubmit")
    if btn:
        return btn

    # 方法 2: 回退到按名称查找（find_button 只在 Button 角色中找）
    btn = uia.find_button("提交")
    if btn:
        return btn

    # 方法 3: 最后尝试纯名称查找并验证角色
    btn = uia.find_by_name("提交")
    if btn:
        info = btn["get_info"]()
        if info.get('role', 0) == 2 and info.get('is_enabled', True):  # 2 = Button
            return btn

    return None

# 使用
btn = find_submit_button()
if btn:
    btn["click"]()
else:
    print("未找到提交按钮")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function findSubmitButton()
    -- 方法 1: 优先使用 AutomationId
    local btn = wingman.uia.find_by_id("btnSubmit")
    if btn then
        return btn
    end

    -- 方法 2: 回退到按名称查找（find_button 只在 Button 角色中找）
    btn = wingman.uia.find_button("提交")
    if btn then
        return btn
    end

    -- 方法 3: 最后尝试纯名称查找并验证角色
    local btn = wingman.uia.find_by_name("提交")
    if btn then
        local info = btn:get_info()
        if info.role == 2 and info.is_enabled then  -- 2 = UIARole Button
            return btn
        end
    end

    return nil
end

-- 使用
local btn = findSubmitButton()
if btn then
    btn:click()
else
    print("未找到提交按钮")
end
```

:::

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
    info = root["get_info"]()
    print(f"窗口名称: {info['name']}")
    print(f"角色: {info['role']}")  # 1 = UIARole Window
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local root = wingman.uia.from_foreground()
if root then
    local info = root:get_info()
    print("窗口名称: " .. info.name)
    print("角色: " .. info.role)  -- 1 = UIARole Window
end
```

:::

### 从窗口句柄获取根元素

如果已经知道窗口句柄，可以直接获取其 UI 根元素：

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

# window.find 返回数组 [handle, found]，Python 列表解包可用
hwnd, found = window.find("记事本")
if found:
    root = uia.from_window(hwnd)
    if root:
        print("记事本 UI 根元素获取成功")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- window.find 返回单值数组 {handle, found}，Lua 需先取数组再解两个元素
local result = wingman.window.find("记事本")
local hwnd, found = result[1], result[2]
if found then
    local root = wingman.uia.from_window(hwnd)
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
from wingman import uia

# 目标坐标（按需修改）
x, y = 500, 300

element = uia.from_point(x, y)
if element:
    info = element["get_info"]()
    print(f"元素名称: {info['name']}")
    print(f"角色: {info['role']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 目标坐标（按需修改）
local x, y = 500, 300

local element = wingman.uia.from_point(x, y)
if element then
    local info = element:get_info()
    print("元素名称: " .. info.name)
    print("角色: " .. info.role)
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
    file_menu["click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"文件"的菜单
local fileMenu = wingman.uia.find_by_name("文件")
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
    btn["click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 通过 AutomationId 查找（推荐用于生产环境）
local btn = wingman.uia.find_by_id("btnSubmit")
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
local wingman = require("wingman")

-- 等待对话框出现（最多等待 3 秒）
local dialog = wingman.uia.wait_for_name("对话框", 3000)
if dialog then
    print("对话框已出现")
else
    print("等待超时")
end
```

:::

按 ID / 角色 / 组合选择器等待同一语义：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待指定 AutomationId
element = uia.wait_for_id("submit-btn", 5000)

# 等待组合选择器（名称子串 + 角色）
element = uia.wait_for({"name": "保存", "role": 2}, 5000)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 等待指定 AutomationId
local element = wingman.uia.wait_for_id("submit-btn", 5000)

-- 等待组合选择器（名称子串 + 角色）
element = wingman.uia.wait_for({ name = "保存", role = 2 }, 5000)
```

:::

---

## UIElement 通用方法

所有 UIA 元素都继承自 UIElement，具有以下通用方法。这些方法适用于所有控件类型。

### 获取元素信息

`get_info()` 返回一个包含元素所有属性的对象，键集固定为：

- `name` / `id` / `className` / `text`
- `role`（int，UIARole 角色值，见[上方对照表](#支持的控件类型与-uiarole-对照)）
- `is_enabled` / `is_visible` / `has_focus`（boolean）
- `bounds`（`{x, y, width, height}`）

:::tabs

== Python

```python:line-numbers
from wingman import uia

element = uia.find_button("确定")
if element:
    info = element["get_info"]()
    print(f"名称: {info.get('name', '')}")
    print(f"id: {info.get('id', '')}")
    print(f"类名: {info.get('className', '')}")
    print(f"角色: {info.get('role', 0)}")
    print(f"启用: {info.get('is_enabled', True)}")
    print(f"可见: {info.get('is_visible', True)}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local element = wingman.uia.find_button("确定")
if element then
    local info = element:get_info()
    print("名称: " .. (info.name or ""))
    print("id: " .. (info.id or ""))
    print("类名: " .. (info.className or ""))
    print("角色: " .. tostring(info.role or 0))
    print("启用: " .. tostring(info.is_enabled or false))
    print("可见: " .. tostring(info.is_visible or false))
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
    btn["click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.find_button("确定")
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
    item["double_click"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local item = wingman.uia.find_by_name("文件.txt")
if item then
    item:double_click()
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
    edit["focus"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("用户名")
if edit then
    edit:focus()
end
```

:::

### 获取/设置值

`get_value()`/`set_value()` 的实现是读取/设置元素文本（getText/setText），**仅对文本型控件有效**（如编辑框）。对非文本控件（复选框、滚动条、滑块等）调用不会产生勾选、滚动、调值等效果：

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    # 获取文本
    value = edit["get_value"]()
    print(f"当前值: {value}")

    # 设置文本
    edit["set_value"]("搜索关键词")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("搜索")
if edit then
    -- 获取文本
    local value = edit:get_value()
    print("当前值: " .. value)

    -- 设置文本
    edit:set_value("搜索关键词")
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
    children = root["get_children"]()
    for i, child in enumerate(children):
        info = child["get_info"]()
        print(f"[{i}] {info['name']} (role={info['role']})")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local root = wingman.uia.from_foreground()
if root then
    local children = root:get_children()
    for i, child in ipairs(children) do
        local info = child:get_info()
        print(string.format("[%d] %s (role=%d)", i, info.name, info.role))
    end
end
```

:::

### 获取父元素

从子元素向上遍历（到根之后返回 `None`/`nil`）：

:::tabs

== Python

```python:line-numbers
from wingman import uia

item = uia.find_list_item("row1")
if item:
    parent = item["get_parent"]()
    if parent:
        print(f"父元素: {parent['get_info']()['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local item = wingman.uia.find_list_item("row1")
if item then
    local parent = item:get_parent()
    if parent then
        print("父元素: " .. parent:get_info().name)
    end
end
```

:::

### 选中元素

编程选中（不移动鼠标）：列表项/树节点/标签项等可选中元素调 `select`；容器调 `get_selection` 查当前选中项。

:::tabs

== Python

```python:line-numbers
from wingman import uia

item = uia.find_list_item("row1")
if item:
    item["select"]()

# 容器查询当前选中
lst = uia.find_list("items")
if lst:
    selected = lst["get_selection"]()
    if selected:
        print(f"当前选中: {selected['get_info']()['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local item = wingman.uia.find_list_item("row1")
if item then
    item:select()
end

-- 容器查询当前选中
local lst = wingman.uia.find_list("items")
if lst then
    local selected = lst:get_selection()
    if selected then
        print("当前选中: " .. selected:get_info().name)
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
    menu["expand"]()

    # 检查是否已展开
    if menu["is_expanded"]():
        print("菜单已展开")

    # 折叠
    menu["collapse"]()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local menu = wingman.uia.find_by_name("文件")
if menu then
    -- 展开
    menu:expand()

    -- 检查是否已展开
    if menu:is_expanded() then
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

> **callback 参数语义**：`callback(prop, value)` 中 `prop` 为触发事件的元素名称，`value` 为元素当前文本。受后端事件接口限制，不区分具体变更的属性名（如 Name/Value/IsEnabled）。`on_structure_changed` 的 `callback()` 无参数。
>
> **线程安全**：事件回调从后台线程触发（Windows UIA RPC 线程 / macOS AXObserver run loop 线程），`callback` 必须是线程安全的（如 Python 函数）。Lua 函数非线程安全，注册时会被拒绝（返回 0 并触发 `uia.error` 事件）。**UIA 事件监听只能用 Python 注册**（见下方说明）。

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 注意：Lua callable 非线程安全，wingman.uia.on_property_changed / on_structure_changed
-- 注册会被拒绝——返回 0（非监听器 ID）并触发 uia.error 事件。
-- UIA 事件监听请使用 Python（见上方 Python 页签），注册成功返回非 0 的监听器 ID。
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
local wingman = require("wingman")

-- 注意：Lua callable 非线程安全，wingman.uia.on_structure_changed 注册会被拒绝
-- （返回 0 并触发 uia.error 事件）。请使用 Python 注册 UIA 事件（见上方 Python 页签）。
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
local wingman = require("wingman")

-- 注意：UIA 事件监听器只能用 Python 注册（Lua callable 非线程安全，注册会被拒绝），
-- 因此 remove_event_listener 也只能在注册方（Python）中调用：
-- uia.remove_event_listener(listener_id)
```

:::

---

## 可用接口

> uia 模块的注册名均为 snake_case，Python 与 Lua 使用相同的函数名：Python 经 `uia.` 前缀属性调用，Lua 经 `wingman.uia.` 前缀调用；UIElement 元素方法 Python 用 `element["方法名"]()`，Lua 用 `element:方法名()`。

### 根元素获取

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `from_foreground()` | `from_foreground()` | 获取前台窗口的根元素 |
| `from_window(hwnd)` | `from_window(hwnd)` | 从窗口句柄获取根元素 |
| `from_point(x, y)` | `from_point(x, y)` | 从屏幕坐标获取元素 |

### 通用查找

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找元素 |
| `find_by_id(id)` | `find_by_id(id)` | 按 AutomationId 查找 |
| `find_all_by_control_type(role)` | `find_all_by_control_type(role)` | 查找所有指定角色的元素（role 为 UIARole 数值，见[对照表](#支持的控件类型与-uiarole-对照)） |
| `wait_for_name(name, timeout)` | `wait_for_name(name, timeout)` | 等待元素出现（按名称） |

### 等待类

轮询直到元素出现或超时（默认 3000ms），超时返回 `None`/`nil`。

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `wait_for_id(id, timeout)` | `wait_for_id(id, timeout)` | 等待指定 AutomationId 的元素出现 |
| `wait_for_role(control_type, timeout)` | `wait_for_role(control_type, timeout)` | 等待指定角色（UIARole 数值）的元素出现 |
| `wait_for(selector, timeout)` | `wait_for(selector, timeout)` | 等待匹配选择器的元素出现；`selector` 为 `{name?, id?, className?, role?, text?}`，`name`/`text` 子串匹配，其余全等，多字段为「与」关系 |

### 专用查找

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_button(name)` | `find_button(name)` | 查找按钮控件 |
| `find_edit(name)` | `find_edit(name)` | 查找编辑框控件 |
| `find_text(name)` | `find_text(name)` | 按名称查找（UIARole 无 Text 角色，不做角色过滤，可能命中任意类型） |
| `find_check_box(name)` | `find_check_box(name)` | 查找复选框控件（CheckBox 角色） |
| `find_radio_button(name)` | `find_radio_button(name)` | 查找单选按钮控件（RadioButton 角色） |
| `find_combo_box(name)` | `find_combo_box(name)` | 查找下拉框控件（ComboBox 角色） |
| `find_list(name)` | `find_list(name)` | 查找列表控件（ListBox 角色） |
| `find_list_item(name)` | `find_list_item(name)` | 查找列表项控件（ListItem 角色） |
| `find_tree(name)` | `find_tree(name)` | 查找树形控件（Tree 角色） |
| `find_tree_item(name)` | `find_tree_item(name)` | 查找树节点控件（TreeItem 角色） |
| `find_menu_item(name)` | `find_menu_item(name)` | 查找菜单项控件（MenuItem 角色） |
| `find_hyperlink(name)` | `find_hyperlink(name)` | 查找超链接控件（Hyperlink 角色） |
| `find_image(name)` | `find_image(name)` | 查找图像控件（Image 角色） |
| `find_slider(name)` | `find_slider(name)` | 查找滑块控件（Slider 角色） |
| `find_spinner(name)` | `find_spinner(name)` | 查找微调器控件（Spinner 角色） |
| `find_progress_bar(name)` | `find_progress_bar(name)` | 查找进度条控件（ProgressBar 角色） |
| `find_tab(name)` | `find_tab(name)` | 查找标签页容器（Tab 角色） |
| `find_tab_item(name)` | `find_tab_item(name)` | 查找标签项控件（TabItem 角色） |

### 事件监听

> [**已实现**：以下事件监听函数已在脚本层实现，后端事件接口与双平台实现（Windows COM + macOS AXObserver）均已就绪。]

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `on_property_changed(name, callback)` | `on_property_changed(name, callback)` | 注册属性变更监听器（仅限线程安全 callable，Lua 会被拒绝） |
| `on_structure_changed(name, callback)` | `on_structure_changed(name, callback)` | 注册结构变更监听器（仅限线程安全 callable，Lua 会被拒绝） |
| `remove_event_listener(id)` | `remove_event_listener(id)` | 移除事件监听器 |

---

## 子模块

- [Button 按钮](./button.md) - 按钮控件的详细文档
- [Edit 编辑框](./edit.md) - 编辑框控件的详细文档
- [Text 文本](./text.md) - 文本控件的详细文档
- [ComboBox 下拉框](./combobox.md) - 下拉框控件的详细文档
- [List 列表](./list.md) - 列表控件的详细文档
- [CheckBox 复选框](./checkbox.md) - 复选框控件的详细文档
- [RadioButton 单选按钮](./radiobutton.md) - 单选按钮控件的详细文档
- [Tab 标签页](./tab.md) - 标签页控件的详细文档
- [Menu 菜单](./menu.md) - 菜单控件的详细文档
- [Tree 树形控件](./tree.md) - 树形控件的详细文档
- [Window 窗口](./window.md) - 窗口控件的详细文档
- [ScrollBar 滚动条](./scrollbar.md) - 滚动条控件的详细文档
- [ProgressBar 进度条](./progressbar.md) - 进度条控件的详细文档
- [Slider 滑块](./slider.md) - 滑块控件的详细文档
- [ToolTip 工具提示](./tooltip.md) - 工具提示控件的详细文档
