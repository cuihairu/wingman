# API: UIA Button

按钮控件是最常见的 UI 元素，用于触发操作、提交表单、确认对话框等。

## 查找按钮

### 按名称查找按钮

**说明**：按按钮上显示的文本查找。这是最直观的方式，但可能因为界面改版或语言变化而失效。

**函数签名**：

```python
find_button(name: str) -> UIElement | None
```

```lua
findButton(name: string) -> UIElement | nil
```

**参数**：
- `name` - 按钮上显示的文本（如"确定"、"取消"、"提交"）

**返回**：找到的按钮元素，未找到时返回 None/nil

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"确定"的按钮
btn = uia.find_button("确定")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"确定"的按钮
local btn = wingman.uia.findButton("确定")
if btn then
    btn:click()
end
```

:::

### 按 AutomationId 查找（推荐）

**说明**：使用开发者设置的唯一 ID 查找。这是最稳定的方式，不会因界面改版或语言变化而失效。

**如何获取 AutomationId**：参考 [概述文档中的说明](./index.md#查找控件的优先级)

**函数签名**：

```python
find_by_id(id: str) -> UIElement | None
```

```lua
findById(id: string) -> UIElement | nil
```

**参数**：
- `id` - 控件的 AutomationId（如"btnSubmit"、"btnOK"）

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 使用 AutomationId 查找（推荐）
btn = uia.find_by_id("btnSubmit")
if btn:
    btn.click()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 使用 AutomationId 查找（推荐）
local btn = wingman.uia.findById("btnSubmit")
if btn then
    btn:click()
end
```

:::

---

## 操作按钮

### 点击按钮

**说明**：模拟鼠标点击按钮。这是最常见的操作，用于触发按钮关联的动作。

**方法签名**：

```python
UIElement.click() -> None
```

```lua
UIElement:click()
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")
if btn:
    btn.click()
    print("已点击提交按钮")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.findButton("提交")
if btn then
    btn:click()
    print("已点击提交按钮")
end
```

:::

### 双击按钮

**说明**：模拟鼠标双击按钮。某些应用程序的双击按钮有特殊功能（如"运行"按钮可能双击会以调试模式启动）。

**方法签名**：

```python
UIElement.double_click() -> None
```

```lua
UIElement:doubleClick()
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("运行")
if btn:
    btn.double_click()
    print("已双击运行按钮")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.findButton("运行")
if btn then
    btn:doubleClick()
    print("已双击运行按钮")
end
```

:::

---

## 检测按钮状态

### 检查按钮是否启用

**说明**：某些按钮在特定条件下会被禁用（如表单未填写完整时）。操作前检查状态可以避免失败。

**获取状态的方式**：通过 `get_info()` 方法获取按钮信息，其中 `is_enabled` 字段表示按钮是否可用。

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("提交")
if btn:
    # 获取按钮信息
    info = btn.get_info()

    # 检查是否启用
    if info.get('is_enabled', True):
        btn.click()
        print("已点击提交按钮")
    else:
        print("按钮已禁用，可能需要先完成其他操作")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.findButton("提交")
if btn then
    -- 获取按钮信息
    local info = btn:getInfo()

    -- 检查是否启用
    if info.isEnabled then
        btn:click()
        print("已点击提交按钮")
    else
        print("按钮已禁用，可能需要先完成其他操作")
    end
end
```

:::

### 等待按钮启用

**说明**：某些按钮在加载过程中会被禁用，直到某些条件满足才启用。可以使用轮询方式等待按钮变为可用状态。

**使用场景**：
- 等待异步操作完成
- 等待表单验证通过
- 等待数据加载完成

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def wait_for_button_enabled(name, timeout=5000):
    """
    等待按钮启用

    参数:
        name: 按钮名称
        timeout: 超时时间（毫秒），默认 5 秒

    返回:
        启用的按钮元素，超时返回 None
    """
    start = util.time()
    while util.time() - start < timeout:
        btn = uia.find_button(name)
        if btn:
            info = btn.get_info()
            if info.get('is_enabled', True):
                return btn
        util.sleep(200)  # 每 200ms 检查一次
    return None

# 使用示例
btn = wait_for_button_enabled("确定", 10000)
if btn:
    btn.click()
    print("按钮已启用并点击")
else:
    print("等待超时，按钮仍未启用")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function waitForButtonEnabled(name, timeout)
    """
    等待按钮启用

    参数:
        name: 按钮名称
        timeout: 超时时间（毫秒），默认 5 秒

    返回:
        启用的按钮元素，超时返回 nil
    """
    timeout = timeout or 5000
    local start = wingman.util.time()

    while wingman.util.time() - start < timeout do
        local btn = wingman.uia.findButton(name)
        if btn then
            local info = btn:getInfo()
            if info.isEnabled then
                return btn
            end
        end
        wingman.util.sleep(200)  -- 每 200ms 检查一次
    end
    return nil
end

-- 使用示例
local btn = waitForButtonEnabled("确定", 10000)
if btn then
    btn:click()
    print("按钮已启用并点击")
else
    print("等待超时，按钮仍未启用")
end
```

:::

---

## 获取按钮信息

### 查看按钮所有属性

**说明**：`get_info()` 方法返回按钮的所有可用属性，包括名称、类型、状态等。这对于调试和理解按钮结构很有帮助。

**常用属性**：
- `name` - 按钮显示文本
- `control_type` - 控件类型（应为 "Button"）
- `automation_id` - AutomationId（如果有）
- `is_enabled` - 是否启用
- `is_visible` - 是否可见
- `bounding_rect` - 按钮的位置和大小

:::tabs

== Python

```python:line-numbers
from wingman import uia

btn = uia.find_button("确定")
if btn:
    info = btn.get_info()

    # 打印所有属性
    print(f"按钮名称: {info.get('name', '')}")
    print(f"控件类型: {info.get('control_type', '')}")
    print(f"AutomationId: {info.get('automation_id', '')}")
    print(f"是否启用: {info.get('is_enabled', True)}")
    print(f"是否可见: {info.get('is_visible', True)}")

    # 位置信息
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        print(f"位置: ({rect['left']}, {rect['top']})")
        print(f"大小: {rect['width']} x {rect['height']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local btn = wingman.uia.findButton("确定")
if btn then
    local info = btn:getInfo()

    -- 打印所有属性
    print("按钮名称: " .. (info.name or ""))
    print("控件类型: " .. (info.controlType or ""))
    print("AutomationId: " .. (info.automationId or ""))
    print("是否启用: " .. tostring(info.isEnabled or true))
    print("是否可见: " .. tostring(info.isVisible or true))

    -- 位置信息
    if info.boundingRect then
        local rect = info.boundingRect
        print(string.format("位置: (%d, %d)", rect.left, rect.top))
        print(string.format("大小: %d x %d", rect.width, rect.height))
    end
end
```

:::

---

## 完整示例

### 表单提交场景

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def submit_form():
    """填写并提交表单的完整流程"""

    # 1. 填写用户名
    username = uia.find_edit("用户名")
    if username:
        username.set_value("player123")
        print("已填写用户名")

    # 2. 填写密码
    password = uia.find_edit("密码")
    if password:
        password.set_value("mypassword")
        print("已填写密码")

    util.sleep(500)

    # 3. 等待提交按钮启用
    submit_btn = wait_for_button_enabled("提交", 5000)
    if submit_btn:
        submit_btn.click()
        print("已点击提交按钮")
        return True
    else:
        print("提交按钮未启用")
        return False

# 执行
if submit_form():
    print("表单提交成功")
else:
    print("表单提交失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function waitForButtonEnabled(name, timeout)
    timeout = timeout or 5000
    local start = wingman.util.time()
    while wingman.util.time() - start < timeout do
        local btn = wingman.uia.findButton(name)
        if btn then
            local info = btn:getInfo()
            if info.isEnabled then
                return btn
            end
        end
        wingman.util.sleep(200)
    end
    return nil
end

local function submitForm()
    -- 1. 填写用户名
    local username = wingman.uia.findEdit("用户名")
    if username then
        username:setValue("player123")
        print("已填写用户名")
    end

    -- 2. 填写密码
    local password = wingman.uia.findEdit("密码")
    if password then
        password:setValue("mypassword")
        print("已填写密码")
    end

    wingman.util.sleep(500)

    -- 3. 等待提交按钮启用
    local submitBtn = waitForButtonEnabled("提交", 5000)
    if submitBtn then
        submitBtn:click()
        print("已点击提交按钮")
        return true
    else
        print("提交按钮未启用")
        return false
    end
end

-- 执行
if submitForm() then
    print("表单提交成功")
else
    print("表单提交失败")
end
```

:::

---

## 可用接口

### 查找按钮

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_button(name)` | `findButton(name)` | 按名称查找按钮 | `name` - 按钮显示文本 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 | `id` - AutomationId |
| `find_by_name(name)` | `findByName(name)` | 通用查找方法 | `name` - 元素名称 |

### 按钮操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `click()` | `:click()` | 点击按钮 |
| `double_click()` | `:doubleClick()` | 双击按钮 |
| `get_info()` | `:getInfo()` | 获取按钮所有属性 |

### 按钮属性

| 属性 | 类型 | 说明 |
|-----|------|------|
| `name` | string | 按钮显示文本 |
| `control_type` | string | 控件类型（"Button"） |
| `automation_id` | string | AutomationId（可能为空） |
| `is_enabled` | boolean | 是否启用 |
| `is_visible` | boolean | 是否可见 |
| `bounding_rect` | object | 位置和大小 |
