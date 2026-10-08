# API: UIA CheckBox

复选框（CheckBox）用于多选项选择场景，用户可以勾选或取消勾选多个选项。常见场景包括：
- 同意用户协议
- 记住登录状态
- 选择多个兴趣标签
- 启用/禁用功能选项

## 查找复选框

**说明**：复选框通常有标签文字（如"记住密码"、"同意协议"），可以通过名称查找。

**函数签名**：

```python
find_by_name(name: str) -> UIElement | None
```

```lua
find_by_name(name: string) -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"记住密码"的复选框
checkbox = uia.find_by_name("记住密码")
if checkbox:
    print("找到复选框")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"记住密码"的复选框
local checkbox = wingman.uia.find_by_name("记住密码")
if checkbox then
    print("找到复选框")
end
```

:::

---

## 勾选/取消勾选

> **未实现（计划中）**：脚本层目前没有直接设置复选框勾选状态的接口。`set_value` 的实现是设置元素文本（`setText`），**仅对文本型控件有效**，对复选框调用不会勾选/取消勾选；翻转勾选状态（toggle）接口同样未实现。

### 通过点击切换

**说明**：模拟用户点击复选框来切换状态（勾选变未勾选，未勾选变勾选）。这是当前唯一可用的勾选操作。

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 点击复选框切换状态
    checkbox["click"]()
    print("已点击切换状态")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.find_by_name("记住密码")
if checkbox then
    -- 点击复选框切换状态
    checkbox:click()
    print("已点击切换状态")
end
```

:::

---

## 获取复选框状态

> **未实现（计划中）**：`get_value()` 返回的是元素文本（实现为 `getText`），不是布尔勾选状态；`get_info()` 的返回键中也没有勾选状态字段（无 `toggle_state`）。脚本层目前**无法读取复选框是否勾选**（`isChecked` 仅存在于 C++ 层 `IUIAElement` 接口，未注册到脚本层）。通用属性见 [get_info 说明](./index.md)。

---

## 三态复选框

**设计意图**：某些复选框支持三种状态：
- **On（选中）** - 明确勾选
- **Off（未选中）** - 明确不勾选
- **Indeterminate（不确定）** - 部分选中状态

**常见场景**：
- "全选"复选框：子项全部选中时为 On，全部未选中为 Off，部分选中为 Indeterminate
- 树形结构中的父节点

> **未实现（计划中）**：`set_toggle_state(state)`（设置 'On'/'Off'/'Indeterminate' 三态）尚未在脚本层实现，此处仅保留设计意图。当前可用操作为 `click()` 切换与通用 12 方法（见 [概述](./index.md#uielement-通用方法)）。

---

## 完整示例

### 用户注册场景

:::tabs

== Python

```python:line-numbers
from wingman import uia

def register_with_agreement():
    """注册并同意协议"""

    # 1. 填写表单（略）
    # ...

    # 2. 点击"同意用户协议"复选框
    # 注意：脚本层无法读取当前勾选状态（见"获取复选框状态"一节），
    # 仅在界面初始为未勾选时点击一次才能达成勾选；状态未知时先人工确认
    agreement = uia.find_by_name("我已阅读并同意用户协议")
    if agreement:
        agreement["click"]()
        print("已点击同意用户协议")

    # 3. 可选：点击"记住密码"复选框
    remember = uia.find_by_name("记住密码")
    if remember:
        remember["click"]()
        print("已点击记住密码")

    # 4. 点击注册按钮
    register_btn = uia.find_button("注册")
    if register_btn:
        register_btn["click"]()
        print("已点击注册按钮")
        return True

    return False

# 执行
if register_with_agreement():
    print("注册流程完成")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function registerWithAgreement()
    -- 1. 填写表单（略）
    -- ...

    -- 2. 点击"同意用户协议"复选框
    -- 注意：脚本层无法读取当前勾选状态（见"获取复选框状态"一节），
    -- 仅在界面初始为未勾选时点击一次才能达成勾选；状态未知时先人工确认
    local agreement = wingman.uia.find_by_name("我已阅读并同意用户协议")
    if agreement then
        agreement:click()
        print("已点击同意用户协议")
    end

    -- 3. 可选：点击"记住密码"复选框
    local remember = wingman.uia.find_by_name("记住密码")
    if remember then
        remember:click()
        print("已点击记住密码")
    end

    -- 4. 点击注册按钮
    local registerBtn = wingman.uia.find_button("注册")
    if registerBtn then
        registerBtn:click()
        print("已点击注册按钮")
        return true
    end

    return false
end

-- 执行
if registerWithAgreement() then
    print("注册流程完成")
end
```

:::

---

## 可用接口

### 查找复选框

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 | `name` - 复选框标签 |
| `find_by_id(id)` | `find_by_id(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 复选框操作

| Python 方法 | Lua 方法 | 说明 | 参数 |
|------------|---------|------|-----|
| `click()` | `:click()` | 点击切换勾选状态 | 无 |
| `get_info()` | `:get_info()` | 获取所有通用属性（不含勾选状态） | 无 |

> **未实现（计划中）**：勾选状态读取（`get_value` 返回布尔勾选态）与勾选设置（`set_value(bool)`、三态 `set_toggle_state`）均未实现——`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效。

### 复选框属性

`get_info()` 返回键集见 [概述](./index.md#uielement-通用方法)（`name`/`id`/`className`/`role`/`text`/`is_enabled`/`is_visible`/`has_focus`/`bounds`），其中**没有**勾选状态字段（无 `toggle_state`/`is_toggle_pattern`）。
