# API: UIA RadioButton

单选按钮控件，用于从多个选项中选择一个。

## 查找单选按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    print("找到单选按钮")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    print("找到单选按钮")
end
```

:::

---

## 选择单选按钮

### 通过点击选中

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    radio.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    radio:click()
end
```

:::

### 通过设置值选中

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    radio.set_value(True)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    radio:setValue(true)
end
```

:::

---

## 获取选中状态

### 检查单个单选按钮

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    info = radio.get_info()
    state = info.get('toggle_state', 'Off')
    is_selected = state == 'On'
    print(f"是否选中: {is_selected}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    local info = radio:getInfo()
    local state = info.toggleState or "Off"
    local isSelected = state == "On"
    print("是否选中: " .. tostring(isSelected))
end
```

:::

### 获取一组单选按钮的选择

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
local uia = require("wingman.uia")

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

## 可用接口

### 查找单选按钮

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 单选按钮操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `click()` | `:click()` | 点击选中 |
| `set_value(bool)` | `:setValue(bool)` | 设置选中状态 |
| `get_value()` | `:getValue()` | 获取是否选中 |
