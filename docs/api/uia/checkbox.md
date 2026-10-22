> [中文](checkbox.md) | **English**

# API: UIA CheckBox

The CheckBox is used in multi-select scenarios, where the user can check or uncheck multiple options. Common scenarios include:
- Agreeing to a user agreement
- Remembering login state
- Selecting multiple interest tags
- Enabling/disabling feature options

## Finding a CheckBox

**Note**: CheckBoxes usually have label text (such as "记住密码" or "同意协议") and can be found by name.

**Function signature**:

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

## Checking/unchecking

> `set_checked(bool)` sets the checked state directly (shipped 2026-10-09: win Toggle pattern / mac AXChecked):

:::tabs

== Python

```python:line-numbers
from wingman import uia

agree = uia.find_check_box("同意条款")
if agree:
    agree["set_checked"](True)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local agree = wingman.uia.find_check_box("同意条款")
if agree then
    agree.set_checked(true)
end
```

:::

> `set_value` is implemented as setting the element text (`setText`) and **only works for text-type controls**; calling it on a CheckBox does not check/uncheck it — use `set_checked` instead.

### Toggling by clicking

**Note**: Simulate a user clicking the checkbox to toggle its state (checked becomes unchecked, unchecked becomes checked). This is currently the only available check operation.

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
    checkbox.click()
    print("已点击切换状态")
end
```

:::

---

## Getting the CheckBox state

> `is_checked()` reads the checked state (shipped 2026-10-09: win Toggle pattern / mac AXChecked):

:::tabs

== Python

```python:line-numbers
from wingman import uia

agree = uia.find_check_box("同意条款")
if agree:
    print("已勾选" if agree["is_checked"]() else "未勾选")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local agree = wingman.uia.find_check_box("同意条款")
if agree then
    if agree.is_checked() then
        print("已勾选")
    else
        print("未勾选")
    end
end
```

:::

> `get_value()` returns the element text (implemented as `getText`), not the boolean checked state; `get_info()` return keys also have no checked field — use `is_checked` to read the checked state.

---

## Three-state CheckBox

**Design intent**: Some checkboxes support three states:
- **On (checked)** - explicitly checked
- **Off (unchecked)** - explicitly unchecked
- **Indeterminate** - partially checked state

**Common scenarios**:
- "Select All" checkbox: On when all children are checked, Off when none are checked, Indeterminate when some are checked
- Parent nodes in tree structures

> `set_toggle_state(state)` (explicit three-state modeling of 'On'/'Off'/'Indeterminate') is not yet implemented; the Indeterminate state cannot be set directly — for two-state scenarios use `set_checked(bool)`. Currently available operations: `set_checked` / `click()` toggling and the general methods (see [Overview](./index.md#uielement-通用方法)).

---

## Complete examples

### User registration scenario

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
        agreement.click()
        print("已点击同意用户协议")
    end

    -- 3. 可选：点击"记住密码"复选框
    local remember = wingman.uia.find_by_name("记住密码")
    if remember then
        remember.click()
        print("已点击记住密码")
    end

    -- 4. 点击注册按钮
    local registerBtn = wingman.uia.find_button("注册")
    if registerBtn then
        registerBtn.click()
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

## Available Interfaces

### Finding a CheckBox

| Python function | Lua function | Description | Parameters |
|----------------|-------------|-------------|------------|
| `find_by_name(name)` | `find_by_name(name)` | Find by name | `name` - checkbox label |
| `find_by_id(id)` | `find_by_id(id)` | Find by AutomationId | `id` - AutomationId |

### CheckBox operations

| Python method | Lua method | Description | Parameters |
|----------------|-------------|-------------|------------|
| `click()` | `:click()` | Click to toggle the checked state | None |
| `get_info()` | `:get_info()` | Get all general properties (no checked state) | None |

> Use `is_checked` / `set_checked` to read and set the checked state (see above); `get_value`/`set_value` actually read/set the element text and only work for text-type controls; the three-state `set_toggle_state` is not implemented.

### CheckBox properties

The `get_info()` return key set is documented in [Overview](./index.md#uielement-通用方法) (`name`/`id`/`className`/`role`/`text`/`is_enabled`/`is_visible`/`has_focus`/`bounds`), which contains **no** checked-state field (no `toggle_state`/`is_toggle_pattern`).
