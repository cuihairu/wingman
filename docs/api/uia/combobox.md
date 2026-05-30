# API: UIA ComboBox

下拉框控件，用于从选项列表中选择一个值。

## 查找下拉框

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    info = combo.get_info()
    print(f"当前选择: {info.get('value', '')}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local combo = uia.findByName("国家/地区")
if combo then
    local info = combo:getInfo()
    print("当前选择: " .. (info.value or ""))
end
```

:::

---

## 操作下拉框

### 展开并选择选项

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

combo = uia.find_by_name("国家/地区")
if combo:
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

local combo = uia.findByName("国家/地区")
if combo then
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

### 直接设置值

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    # 直接设置值（如果支持）
    combo.set_value("中国")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local combo = uia.findByName("国家/地区")
if combo then
    -- 直接设置值（如果支持）
    combo:setValue("中国")
end
```

:::

---

## 可编辑下拉框

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
local uia = require("wingman.uia")

-- 可编辑下拉框也是 Edit 类型
local editableCombo = uia.findEdit("搜索")
if editableCombo then
    -- 直接输入文本
    editableCombo:setValue("搜索内容")

    -- 或展开选择
    editableCombo:expand()
end
```

:::

---

## 获取所有选项

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    # 展开下拉框
    combo.expand()
    util.sleep(300)

    # 获取所有选项
    options = uia.find_all_by_control_type("ListItem")
    for opt in options:
        info = opt.get_info()
        print(f"选项: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local combo = uia.findByName("国家/地区")
if combo then
    -- 展开下拉框
    combo:expand()
    util.sleep(300)

    -- 获取所有选项
    local options = uia.findAllByControlType("ListItem")
    for i, opt in ipairs(options) do
        local info = opt:getInfo()
        print("选项: " .. info.name)
    end
end
```

:::

---

## 可用接口

### 查找下拉框

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 下拉框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开下拉框 |
| `collapse()` | `:collapse()` | 折叠下拉框 |
| `set_value(value)` | `:setValue(value)` | 设置选中值 |
| `get_value()` | `:getValue()` | 获取当前选中值 |
