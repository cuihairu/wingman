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
findByName(name: string) -> UIElement | nil
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
local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    print("找到复选框")
end
```

:::

---

## 勾选/取消勾选

### 设置勾选状态

**说明**：直接设置复选框为勾选或未勾选状态。

**方法签名**：

```python
UIElement.set_value(value: bool) -> None
```

```lua
UIElement:setValue(value: boolean) -> None
```

**参数**：
- `value` - True 勾选，False 取消勾选

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 勾选复选框
    checkbox.set_value(True)
    print("已勾选记住密码")

    # 取消勾选
    checkbox.set_value(False)
    print("已取消勾选")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    -- 勾选复选框
    checkbox:setValue(true)
    print("已勾选记住密码")

    -- 取消勾选
    checkbox:setValue(false)
    print("已取消勾选")
end
```

:::

### 切换状态

**说明**：翻转复选框的当前状态（勾选变未勾选，未勾选变勾选）。

**使用场景**：不确定当前状态时，想要切换状态。

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 获取当前状态
    current = checkbox.get_value()

    # 切换状态
    checkbox.set_value(not current)

    print(f"已切换状态，当前: {checkbox.get_value()}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    -- 获取当前状态
    local current = checkbox:getValue()

    -- 切换状态
    checkbox:setValue(not current)

    print("已切换状态，当前: " .. tostring(checkbox:getValue()))
end
```

:::

### 通过点击切换

**说明**：模拟用户点击复选框来切换状态。效果与 `set_value(not current)` 相同。

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 点击复选框切换状态
    checkbox.click()
    print("已点击切换状态")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    -- 点击复选框切换状态
    checkbox:click()
    print("已点击切换状态")
end
```

:::

---

## 获取复选框状态

### 检查是否勾选

**说明**：获取复选框的当前勾选状态。

**方法签名**：

```python
UIElement.get_value() -> bool
```

```lua
UIElement:getValue() -> boolean
```

**返回**：True 表示已勾选，False 表示未勾选

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 方法 1: 通过 get_value（推荐）
    is_checked = checkbox.get_value()
    print(f"是否勾选: {is_checked}")

    # 方法 2: 通过 get_info 查看详细状态
    info = checkbox.get_info()
    state = info.get('toggle_state', 'Off')
    print(f"详细状态: {state}")  # On/Off/Indeterminate
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("记住密码")
if checkbox then
    -- 方法 1: 通过 getValue（推荐）
    local isChecked = checkbox:getValue()
    print("是否勾选: " .. tostring(isChecked))

    -- 方法 2: 通过 getInfo 查看详细状态
    local info = checkbox:getInfo()
    local state = info.toggleState or "Off"
    print("详细状态: " .. state)  -- On/Off/Indeterminate
end
```

:::

---

## 三态复选框

**说明**：某些复选框支持三种状态：
- **On（选中）** - 明确勾选
- **Off（未选中）** - 明确不勾选
- **Indeterminate（不确定）** - 部分选中状态

**常见场景**：
- "全选"复选框：子项全部选中时为 On，全部未选中为 Off，部分选中为 Indeterminate
- 树形结构中的父节点

### 设置三态

**方法签名**：

```python
UIElement.set_toggle_state(state: str) -> None
```

```lua
UIElement:setToggleState(state: string) -> None
```

**参数**：
- `state` - 状态值：'On'、'Off'、'Indeterminate'

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("全选")
if checkbox:
    # 设置为选中
    checkbox.set_toggle_state('On')
    print("已设置为选中")

    # 设置为未选中
    checkbox.set_toggle_state('Off')
    print("已设置为未选中")

    # 设置为不确定状态（表示部分选中）
    checkbox.set_toggle_state('Indeterminate')
    print("已设置为不确定状态")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local checkbox = wingman.uia.findByName("全选")
if checkbox then
    -- 设置为选中
    checkbox:setToggleState("On")
    print("已设置为选中")

    -- 设置为未选中
    checkbox:setToggleState("Off")
    print("已设置为未选中")

    -- 设置为不确定状态（表示部分选中）
    checkbox:setToggleState("Indeterminate")
    print("已设置为不确定状态")
end
```

:::

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

    # 2. 勾选"同意用户协议"
    agreement = uia.find_by_name("我已阅读并同意用户协议")
    if agreement:
        # 检查当前状态
        if not agreement.get_value():
            # 未勾选，则勾选
            agreement.set_value(True)
            print("已同意用户协议")
        else:
            print("已经同意了协议")

    # 3. 可选：勾选"记住密码"
    remember = uia.find_by_name("记住密码")
    if remember:
        remember.set_value(True)
        print("已设置记住密码")

    # 4. 点击注册按钮
    register_btn = uia.find_button("注册")
    if register_btn:
        register_btn.click()
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

    -- 2. 勾选"同意用户协议"
    local agreement = wingman.uia.findByName("我已阅读并同意用户协议")
    if agreement then
        -- 检查当前状态
        if not agreement:getValue() then
            -- 未勾选，则勾选
            agreement:setValue(true)
            print("已同意用户协议")
        else
            print("已经同意了协议")
        end
    end

    -- 3. 可选：勾选"记住密码"
    local remember = wingman.uia.findByName("记住密码")
    if remember then
        remember:setValue(true)
        print("已设置记住密码")
    end

    -- 4. 点击注册按钮
    local registerBtn = wingman.uia.findButton("注册")
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
| `find_by_name(name)` | `findByName(name)` | 按名称查找 | `name` - 复选框标签 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 复选框操作

| Python 方法 | Lua 方法 | 说明 | 参数 |
|------------|---------|------|-----|
| `get_value()` | `:getValue()` | 获取是否勾选 | 返回 boolean |
| `set_value(bool)` | `:setValue(bool)` | 设置勾选状态 | `bool` - True 勾选，False 取消 |
| `set_toggle_state(state)` | `:setToggleState(state)` | 设置三态 | `state` - 'On'/'Off'/'Indeterminate' |
| `click()` | `:click()` | 点击切换状态 | 无 |
| `get_info()` | `:getInfo()` | 获取所有属性 | 无 |

### 复选框属性

| 属性 | 类型 | 说明 |
|-----|------|------|
| `name` | string | 复选框标签文字 |
| `toggle_state` | string | 三态状态：On/Off/Indeterminate |
| `is_toggle_pattern` | boolean | 是否支持切换（通常为 True） |
