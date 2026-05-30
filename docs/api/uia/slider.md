# API: UIA Slider

滑块控件，用于调节数值（如音量、亮度等）。

## 查找滑块

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    print("找到滑块")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local slider = uia.findByName("音量")
if slider then
    print("找到滑块")
end
```

:::

---

## 获取当前值

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    info = slider.get_info()
    value = info.get('value', 0)
    print(f"当前值: {value}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local slider = uia.findByName("音量")
if slider then
    local info = slider:getInfo()
    local value = info.value or 0
    print("当前值: " .. value)
end
```

:::

---

## 设置滑块值

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    info = slider.get_info()

    # 设置为 80
    slider.set_value(80)

    # 设置为最大值
    slider.set_value(info.get('maximum', 100))

    # 设置为最小值
    slider.set_value(info.get('minimum', 0))
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local slider = uia.findByName("音量")
if slider then
    local info = slider:getInfo()

    -- 设置为 80
    slider:setValue(80)

    -- 设置为最大值
    slider:setValue(info.maximum or 100)

    -- 设置为最小值
    slider:setValue(info.minimum or 0)
end
```

:::

---

## 获取滑块范围

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    info = slider.get_info()
    minimum = info.get('minimum', 0)
    maximum = info.get('maximum', 100)
    current = info.get('value', 0)

    print(f"范围: {minimum} - {maximum}")
    print(f"当前值: {current}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local slider = uia.findByName("音量")
if slider then
    local info = slider:getInfo()
    local minimum = info.minimum or 0
    local maximum = info.maximum or 100
    local current = info.value or 0

    print(string.format("范围: %d - %d", minimum, maximum))
    print("当前值: " .. current)
end
```

:::

---

## 可用接口

### 查找滑块

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 滑块操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前值 |
| `set_value(value)` | `:setValue(value)` | 设置滑块值 |
| `get_info()` | `:getInfo()` | 获取滑块信息（包含 minimum/maximum） |
