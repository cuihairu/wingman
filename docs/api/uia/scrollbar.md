# API: UIA ScrollBar

滚动条控件，用于滚动内容区域。

## 查找滚动条

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找垂直滚动条
v_scroll = uia.find_by_name("垂直滚动条")
if v_scroll:
    print("找到垂直滚动条")

# 查找水平滚动条
h_scroll = uia.find_by_name("水平滚动条")
if h_scroll:
    print("找到水平滚动条")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找垂直滚动条
local vScroll = uia.findByName("垂直滚动条")
if vScroll then
    print("找到垂直滚动条")
end

-- 查找水平滚动条
local hScroll = uia.findByName("水平滚动条")
if hScroll then
    print("找到水平滚动条")
end
```

:::

---

## 获取滚动位置

:::tabs

== Python

```python:line-numbers
from wingman import uia

v_scroll = uia.find_by_name("垂直滚动条")
if v_scroll:
    info = v_scroll.get_info()
    value = info.get('value', 0)
    print(f"当前滚动位置: {value}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local vScroll = uia.findByName("垂直滚动条")
if vScroll then
    local info = vScroll:getInfo()
    local value = info.value or 0
    print("当前滚动位置: " .. value)
end
```

:::

---

## 设置滚动位置

:::tabs

== Python

```python:line-numbers
from wingman import uia

v_scroll = uia.find_by_name("垂直滚动条")
if v_scroll:
    # 滚动到顶部（0%）
    v_scroll.set_value(0)

    # 滚动到中间（50%）
    v_scroll.set_value(50)

    # 滚动到底部（100%）
    v_scroll.set_value(100)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local vScroll = uia.findByName("垂直滚动条")
if vScroll then
    -- 滚动到顶部（0%）
    vScroll:setValue(0)

    -- 滚动到中间（50%）
    vScroll:setValue(50)

    -- 滚动到底部（100%）
    vScroll:setValue(100)
end
```

:::

---

## 获取滚动范围

:::tabs

== Python

```python:line-numbers
from wingman import uia

v_scroll = uia.find_by_name("垂直滚动条")
if v_scroll:
    info = v_scroll.get_info()
    minimum = info.get('minimum', 0)
    maximum = info.get('maximum', 100)
    current = info.get('value', 0)

    print(f"滚动范围: {minimum} - {maximum}")
    print(f"当前位置: {current}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local vScroll = uia.findByName("垂直滚动条")
if vScroll then
    local info = vScroll:getInfo()
    local minimum = info.minimum or 0
    local maximum = info.maximum or 100
    local current = info.value or 0

    print(string.format("滚动范围: %d - %d", minimum, maximum))
    print("当前位置: " .. current)
end
```

:::

---

## 可用接口

### 查找滚动条

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 滚动条操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前滚动位置 |
| `set_value(value)` | `:setValue(value)` | 设置滚动位置（0-100） |
| `get_info()` | `:getInfo()` | 获取滚动条信息（包含 minimum/maximum） |
