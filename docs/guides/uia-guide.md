# UI Automation 指南

Wingman 的 UI Automation (UIA) 模块允许你直接与 Windows 应用程序的 UI 控件交互，无需依赖坐标点击。

## 什么是 UI Automation？

UI Automation 是 Microsoft 提供的无障碍访问框架，最初用于屏幕阅读器等辅助技术。Wingman 将其封装为脚本可用的 API，让你可以：

- 查找和操作按钮、文本框、列表等控件
- 读取和设置控件属性
- 监听 UI 事件
- 实现更稳定的自动化脚本

## 适用场景

**适合使用 UIA：**
- 需要与 Windows 原生程序交互
- 控件位置可能变化，但名称/ID 固定
- 需要读取文本框内容
- 需要与复杂 UI（如树形控件、表格）交互

**不适合使用 UIA：**
- 游戏中的自定义渲染界面
- 基于 DirectX/OpenGL 的界面
- 图形密集型应用（建议用图像识别）

## 基础概念

### UI 元素树

Windows 应用程序的 UI 被组织成树形结构：

```
桌面
└── 记事本窗口
    ├── 菜单栏
    │   ├── 文件菜单
    │   ├── 编辑菜单
    │   └── ...
    ├── 文本编辑区
    └── 状态栏
```

### 控件类型

常见的控件类型包括：

| ControlType | 说明 | 示例 |
|-------------|------|------|
| Window | 窗口 | 应用程序主窗口、对话框 |
| Button | 按钮 | 确定、取消、提交 |
| Edit | 文本输入框 | 用户名输入、搜索框 |
| Text | 静态文本 | 标签、提示信息 |
| ComboBox | 下拉框 | 选择器、选项列表 |
| ListBox | 列表框 | 项目列表 |
| CheckBox | 复选框 | 同意条款、记住密码 |
| RadioButton | 单选按钮 | 性别选择、选项 |
| Tab | 标签页 | 设置页面的分类 |
| Menu | 菜单 | 文件菜单、右键菜单 |
| TreeItem | 树形项 | 文件夹树、组织结构 |
| DataItem | 数据项 | 表格行、列表项 |
| ScrollBar | 滚动条 | 窗口滚动条 |

## 快速开始

### 获取窗口元素

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

# 查找记事本窗口
hwnd, found = window.find("记事本")
if not found:
    print("未找到记事本")
    exit(1)

# 从窗口获取 UIA 根元素
root = uia.from_window(hwnd)
if not root:
    print("无法获取 UIA 元素")
    exit(1)

print("UIA 初始化成功")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")

-- 查找记事本窗口
local hwnd, found = window.find("记事本")
if not found then
    print("未找到记事本")
    return
end

-- 从窗口获取 UIA 核心元素
local root = uia.fromWindow(hwnd)
if not root then
    print("无法获取 UIA 元素")
    return
end

print("UIA 初始化成功")
```

:::

### 查找控件

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找按钮
ok_btn = uia.find_button("确定")
if ok_btn:
    ok_btn.click()

# 查找编辑框
edit = uia.find_edit("")
if edit:
    # 读取内容
    text = edit.get_value()
    print(f"当前内容: {text}")

    # 设置内容
    edit.set_value("Hello, World!")

# 按名称查找任意元素
element = uia.find_by_name("我的文本")
if element:
    info = element.get_info()
    print(f"控件类型: {info['control_type']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找按钮
local okBtn = uia.findButton("确定")
if okBtn then
    okBtn:click()
end

-- 查找编辑框
local edit = uia.findEdit("")
if edit then
    -- 读取内容
    local text = edit:getValue()
    print("当前内容: " .. text)

    -- 设置内容
    edit:setValue("Hello, World!")
end

-- 按名称查找任意元素
local element = uia.findByName("我的文本")
if element then
    local info = element:getInfo()
    print("控件类型: " .. info.controlType)
end
```

:::

## 进阶用法

### 遍历 UI 树

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

hwnd, _ = window.find("记事本")
root = uia.from_window(hwnd)

# 递归遍历所有子元素
def traverse(element, depth=0):
    indent = "  " * depth
    info = element.get_info()
    print(f"{indent}{info['name']} ({info['control_type']})")

    children = element.get_children()
    for child in children:
        traverse(child, depth + 1)

traverse(root)
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")

local hwnd, _ = window.find("记事本")
local root = uia.fromWindow(hwnd)

-- 递归遍历所有子元素
local function traverse(element, depth)
    depth = depth or 0
    local indent = string.rep("  ", depth)
    local info = element:getInfo()
    print(string.format("%s%s (%s)", indent, info.name, info.controlType))

    local children = element:getChildren()
    for i, child in ipairs(children) do
        traverse(child, depth + 1)
    end
end

traverse(root)
```

:::

### 使用条件查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

root = uia.from_foreground()

# 自定义查找条件
def find_by_control_type(element, control_type):
    """查找指定类型的所有元素"""
    results = []
    info = element.get_info()
    if info['control_type'] == control_type:
        results.append(element)

    children = element.get_children()
    for child in children:
        results.extend(find_by_control_type(child, control_type))

    return results

# 查找所有按钮
buttons = find_by_control_type(root, "Button")
for i, btn in enumerate(buttons):
    info = btn.get_info()
    print(f"[{i}] {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local root = uia.fromForeground()

-- 自定义查找条件
local function findByControlType(element, controlType, results)
    results = results or {}
    local info = element:getInfo()
    if info.controlType == controlType then
        table.insert(results, element)
    end

    local children = element:getChildren()
    for i, child in ipairs(children) do
        findByControlType(child, controlType, results)
    end

    return results
end

-- 查找所有按钮
local buttons = findByControlType(root, "Button")
for i, btn in ipairs(buttons) do
    local info = btn:getInfo()
    print(string.format("[%d] %s", i, info.name))
end
```

:::

### 事件监听

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 监听属性变化
def on_property_changed(prop_name, value):
    print(f"属性 {prop_name} 变为: {value}")

# 监听结构变化
def on_structure_changed():
    print("UI 结构发生变化")

# 注册监听器
prop_id = uia.on_property_changed("编辑框", on_property_changed)
struct_id = uia.on_structure_changed("按钮", on_structure_changed)

# ... 等待事件 ...

# 移除监听器
uia.remove_event_listener(prop_id)
uia.remove_event_listener(struct_id)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 监听属性变化
local function onPropertyChanged(propName, value)
    print(string.format("属性 %s 变为: %s", propName, value))
end

-- 监听结构变化
local function onStructureChanged()
    print("UI 结构发生变化")
end

-- 注册监听器
local propId = uia.onPropertyChanged("编辑框", onPropertyChanged)
local structId = uia.onStructureChanged("按钮", onStructureChanged)

-- ... 等待事件 ...

-- 移除监听器
uia.removeEventListener(propId)
uia.removeEventListener(structId)
```

:::

## 完整示例

### 自动填写表单

:::tabs

== Python

```python:line-numbers
from wingman import window, uia, util, input

# 查找目标窗口
hwnd, found = window.find("设置")
if not found:
    print("请先打开设置窗口")
    exit(1)

# 激活窗口
window.activate(hwnd)
util.sleep(500)

# 获取 UIA 根元素
root = uia.from_window(hwnd)
if not root:
    print("无法初始化 UIA")
    exit(1)

# 填写用户名
username_edit = uia.find_edit("用户名")
if username_edit:
    username_edit.set_value("player123")
    print("已填写用户名")
else:
    print("未找到用户名输入框")

# 填写邮箱
email_edit = uia.find_edit("邮箱")
if email_edit:
    email_edit.set_value("player@example.com")
    print("已填写邮箱")

# 勾选同意条款
agree_checkbox = uia.find_by_name("同意用户协议")
if agree_checkbox:
    agree_checkbox.set_value(True)
    print("已勾选协议")

# 点击注册按钮
register_btn = uia.find_button("注册")
if register_btn:
    register_btn.click()
    print("已点击注册按钮")
else:
    print("未找到注册按钮")
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 查找目标窗口
local hwnd, found = window.find("设置")
if not found then
    print("请先打开设置窗口")
    return
end

-- 激活窗口
window.activate(hwnd)
util.sleep(500)

-- 获取 UIA 根元素
local root = uia.fromWindow(hwnd)
if not root then
    print("无法初始化 UIA")
    return
end

-- 填写用户名
local usernameEdit = uia.findEdit("用户名")
if usernameEdit then
    usernameEdit:setValue("player123")
    print("已填写用户名")
else
    print("未找到用户名输入框")
end

-- 填写邮箱
local emailEdit = uia.findEdit("邮箱")
if emailEdit then
    emailEdit:setValue("player@example.com")
    print("已填写邮箱")
end

-- 勾选同意条款
local agreeCheckbox = uia.findByName("同意用户协议")
if agreeCheckbox then
    agreeCheckbox:setValue(true)
    print("已勾选协议")
end

-- 点击注册按钮
local registerBtn = uia.findButton("注册")
if registerBtn then
    registerBtn:click()
    print("已点击注册按钮")
else
    print("未找到注册按钮")
end
```

:::

### 下拉框操作

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 展开下拉框
combo = uia.find_by_name("国家/地区")
if combo:
    combo.expand()
    util.sleep(300)

    # 选择"中国"选项
    china_option = uia.find_by_name("中国")
    if china_option:
        china_option.click()
        print("已选择中国")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 展开下拉框
local combo = uia.findByName("国家/地区")
if combo then
    combo:expand()
    util.sleep(300)

    -- 选择"中国"选项
    local chinaOption = uia.findByName("中国")
    if chinaOption then
        chinaOption:click()
        print("已选择中国")
    end
end
```

:::

### 列表框操作

:::tabs

== Python

```python:line-numbers
from wingman import uia, input

# 获取列表框
list_box = uia.find_by_name("文件列表")
if not list_box:
    print("未找到列表框")
    exit(1)

# 获取所有列表项
items = list_box.get_children()
print(f"共有 {len(items)} 个文件")

# 双击第3项
if len(items) > 2:
    target = items[2]
    # 获取项的位置
    info = target.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        center_x = rect['left'] + rect['width'] // 2
        center_y = rect['top'] + rect['height'] // 2

        # 先单击选中
        input.click(center_x, center_y)
        util.sleep(200)

        # 再双击打开
        input.double_click(center_x, center_y)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 获取列表框
local listBox = uia.findByName("文件列表")
if not listBox then
    print("未找到列表框")
    return
end

-- 获取所有列表项
local items = listBox:getChildren()
print("共有 " .. #items .. " 个文件")

-- 双击第3项
if #items > 2 then
    local target = items[3]
    -- 获取项的位置
    local info = target:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        local centerX = rect.left + rect.width / 2
        local centerY = rect.top + rect.height / 2

        -- 先单击选中
        input.click(centerX, centerY)
        util.sleep(200)

        -- 再双击打开
        input.doubleClick(centerX, centerY)
    end
end
```

:::

## 调试技巧

### UIA 检查工具

使用 Windows SDK 提供的 **Accessibility Insights for Windows** 或 **Inspect.exe** 来查看应用的 UI 结构：

1. **Accessibility Insights**
   - 下载：https://accessibilityinsights.io/
   - 实时查看 UI 元素树
   - 查看元素属性和模式

2. **Inspect.exe** (Windows SDK)
   - 路径：`C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64\inspect.exe`
   - 悬停查看元素信息
   - 查看 UI 树结构

### 打印 UI 树

:::tabs

== Python

```python:line-numbers
from wingman import window, uia

def print_ui_tree(element, depth=0, max_depth=10):
    """打印 UI 树结构"""
    if depth > max_depth:
        return

    indent = "  " * depth
    info = element.get_info()
    name = info['name'] or "(无名称)"
    ctrl_type = info['control_type']
    class_name = info.get('class_name', '')

    # 打印元素信息
    print(f"{indent}[{ctrl_type}] {name} ({class_name})")

    # 递归打印子元素
    children = element.get_children()
    for child in children:
        print_ui_tree(child, depth + 1, max_depth)

# 使用示例
hwnd, _ = window.find("记事本")
root = uia.from_window(hwnd)
print_ui_tree(root)
```

== Lua

```lua:line-numbers
local window = require("wingman.window")
local uia = require("wingman.uia")

local function printUITree(element, depth, maxDepth)
    depth = depth or 0
    maxDepth = maxDepth or 10

    if depth > maxDepth then
        return
    end

    local indent = string.rep("  ", depth)
    local info = element:getInfo()
    local name = info.name or "(无名称)"
    local ctrlType = info.controlType
    local className = info.className or ""

    -- 打印元素信息
    print(string.format("%s[%s] %s (%s)", indent, ctrlType, name, className))

    -- 递归打印子元素
    local children = element:getChildren()
    for i, child in ipairs(children) do
        printUITree(child, depth + 1, maxDepth)
    end
end

-- 使用示例
local hwnd, _ = window.find("记事本")
local root = uia.fromWindow(hwnd)
printUITree(root)
```

:::

### 查找失败的常见原因

1. **名称包含不可见字符**
   - 使用 `inspect.exe` 查看确切名称
   - 尝试使用部分名称匹配

2. **控件未加载**
   - 等待控件出现：`uia.wait_for_name("按钮", 5000)`

3. **需要激活父窗口**
   - 先激活窗口：`window.activate(hwnd)`

4. **控件不支持 UI Automation**
   - 某些自定义控件未实现 UIA 接口
   - 考虑使用图像识别作为备选方案

## 最佳实践

### 1. 优先使用稳定的属性

- **AutomationId** > Name > ClassName > Position
- AutomationId 通常由开发人员设置，最稳定

### 2. 等待控件出现

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 等待按钮出现（最多5秒）
btn = uia.wait_for_name("确定", 5000)
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 等待按钮出现（最多5秒）
local btn = uia.waitForName("确定", 5000)
if btn then
    btn:click()
end
```

:::

### 3. 组合使用 UIA 和坐标

```python
# 先用 UIA 定位元素
element = uia.find_button("提交")

# 再用坐标操作（更可靠）
info = element.get_info()
if 'bounding_rect' in info:
    rect = info['bounding_rect']
    center_x = rect['left'] + rect['width'] // 2
    center_y = rect['top'] + rect['height'] // 2
    input.click(center_x, center_y)
```

### 4. 错误处理

:::tabs

== Python

```python:line-numbers
from wingman import uia

def safe_click(name):
    """安全点击，带重试"""
    for i in range(3):
        btn = uia.find_button(name)
        if btn:
            btn.click()
            return True
        util.sleep(500)

    print(f"无法找到按钮: {name}")
    return False

# 使用
if not safe_click("确定"):
    # 备选方案
    input.click(500, 300)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local function safeClick(name)
    -- 安全点击，带重试
    for i = 1, 3 do
        local btn = uia.findButton(name)
        if btn then
            btn:click()
            return true
        end
        util.sleep(500)
    end

    print("无法找到按钮: " .. name)
    return false
end

-- 使用
if not safeClick("确定") then
    -- 备选方案
    input.click(500, 300)
end
```

:::

## 限制与注意事项

1. **游戏界面**：大多数游戏使用自定义渲染，不支持 UIA
2. **跨进程**：UIA 可以跨进程访问，但某些应用有安全限制
3. **性能**：频繁遍历 UI 树会影响性能，建议缓存常用元素
4. **权限**：某些系统应用需要管理员权限才能访问
5. **延迟**：UI 更新和脚本读取之间存在延迟，需要适当等待

## 参考资源

- [Microsoft UI Automation 文档](https://docs.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32)
- [Accessibility Insights for Windows](https://accessibilityinsights.io/)
- [UI Automation 控件类型参考](https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-controltype-ids)
