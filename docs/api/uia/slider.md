# API: UIA Slider

滑块（Slider）控件用于调节数值，如音量、亮度、对比度等。

## 查找滑块

**说明**：滑块通常有描述性名称。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"音量"的滑块
slider = uia.find_by_name("音量")
if slider:
    print("找到滑块")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"音量"的滑块
local slider = wingman.uia.findByName("音量")
if slider then
    print("找到滑块")
end
```

:::

---

## 获取当前值

**说明**：读取滑块的当前值。

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    info = slider.get_info()
    value = info.get('value', 0)
    print(f"当前音量: {value}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local slider = wingman.uia.findByName("音量")
if slider then
    local info = slider:getInfo()
    local value = info.value or 0
    print("当前音量: " .. value)
end
```

:::

---

## 设置滑块值

**说明**：设置滑块的值。值必须在滑块的最小值和最大值之间。

**方法签名**：

```python
UIElement.set_value(value: number) -> None
```

```lua
UIElement:setValue(value: number) -> None
```

**参数**：
- `value` - 要设置的值

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    info = slider.get_info()

    # 设置为 80
    slider.set_value(80)
    print("已设置音量为 80")

    # 设置为最大值
    slider.set_value(info.get('maximum', 100))
    print("已设置为最大音量")

    # 设置为最小值
    slider.set_value(info.get('minimum', 0))
    print("已设置为最小音量（静音）")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local slider = wingman.uia.findByName("音量")
if slider then
    local info = slider:getInfo()

    -- 设置为 80
    slider:setValue(80)
    print("已设置音量为 80")

    -- 设置为最大值
    slider:setValue(info.maximum or 100)
    print("已设置为最大音量")

    -- 设置为最小值
    slider:setValue(info.minimum or 0)
    print("已设置为最小音量（静音）")
end
```

:::

---

## 获取滑块范围

**说明**：查看滑块允许的最小值和最大值。

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

    print(f"音量范围: {minimum} - {maximum}")
    print(f"当前音量: {current}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local slider = wingman.uia.findByName("音量")
if slider then
    local info = slider:getInfo()
    local minimum = info.minimum or 0
    local maximum = info.maximum or 100
    local current = info.value or 0

    print(string.format("音量范围: %d - %d", minimum, maximum))
    print("当前音量: " .. current)
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前值 |
| `set_value(value)` | `:setValue(value)` | 设置滑块值 |
| `get_info()` | `:getInfo()` | 获取滑块信息 |
