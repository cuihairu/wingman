# API: UIA CheckBox

复选框控件，用于多选项选择。

## 查找复选框

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    print("找到复选框")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("记住密码")
if checkbox then
    print("找到复选框")
end
```

:::

---

## 勾选/取消勾选

### 设置勾选状态

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
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("记住密码")
if checkbox then
    -- 勾选
    checkbox:setValue(true)

    -- 取消勾选
    checkbox:setValue(false)
end
```

:::

### 切换状态

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 切换状态
    current = checkbox.get_value()
    checkbox.set_value(not current)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("记住密码")
if checkbox then
    -- 切换状态
    local current = checkbox:getValue()
    checkbox:setValue(not current)
end
```

:::

### 通过点击切换

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 点击切换
    checkbox.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("记住密码")
if checkbox then
    -- 点击切换
    checkbox:click()
end
```

:::

---

## 获取复选框状态

:::tabs

== Python

```python:line-numbers
from wingman import uia

checkbox = uia.find_by_name("记住密码")
if checkbox:
    # 方法 1: 通过 get_value
    is_checked = checkbox.get_value()
    print(f"是否勾选: {is_checked}")

    # 方法 2: 通过 get_info
    info = checkbox.get_info()
    state = info.get('toggle_state', 'Off')
    print(f"状态: {state}")  # On/Off/Indeterminate
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local checkbox = uia.findByName("记住密码")
if checkbox then
    -- 方法 1: 通过 getValue
    local isChecked = checkbox:getValue()
    print("是否勾选: " .. tostring(isChecked))

    -- 方法 2: 通过 getInfo
    local info = checkbox:getInfo()
    local state = info.toggleState or "Off"
    print("状态: " .. state)  -- On/Off/Indeterminate
end
```

:::

---

## 三态复选框

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
local uia = require("wingman.uia")

local checkbox = uia.findByName("全选")
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

## 可用接口

### 查找复选框

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 复选框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取是否勾选 |
| `set_value(bool)` | `:setValue(bool)` | 设置勾选状态 |
| `set_toggle_state(state)` | `:setToggleState(state)` | 设置三态：'On', 'Off', 'Indeterminate' |
| `click()` | `:click()` | 点击切换状态 |
