> [中文](slider.md) | **English**

# API: UIA Slider

The Slider control is used to adjust a numeric value, such as volume, brightness, or contrast.

## Finding a Slider

**Note**: Sliders usually have descriptive names.

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
local slider = wingman.uia.find_by_name("音量")
if slider then
    print("找到滑块")
end
```

:::

---

## Getting the current value

> **Not implemented (planned)**: The `get_info()` return keys have no numeric field (no `value`), and `get_value()` returns the element text (implemented as `getText`), not the slider value. Reading the slider's current value is currently unavailable.

:::tabs

== Python

```python:line-numbers
from wingman import uia

slider = uia.find_by_name("音量")
if slider:
    # get_info 无数值字段，这里只读取通用信息
    info = slider["get_info"]()
    print(f"名称: {info.get('name', '')}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local slider = wingman.uia.find_by_name("音量")
if slider then
    -- get_info 无数值字段，这里只读取通用信息
    local info = slider:get_info()
    print("名称: " .. (info.name or ""))
end
```

:::

---

## Setting the slider value

> **Not implemented (planned)**: `set_value(number)` cannot set the slider value — `set_value` is implemented as setting the element text (`setText`), and calling it on a Slider does not move the slider or change its value. Adjusting the value must wait for the Range/Value pattern interfaces; as a transitional workaround, `input.scroll()` or drag simulation can be used.

:::tabs

== Python

```python:line-numbers
from wingman import input

# 过渡方案：鼠标移到滑块上后用滚轮微调（方向取决于应用）
input.move(400, 300)
input.scroll(400, 300, -1)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 过渡方案：鼠标移到滑块上后用滚轮微调（方向取决于应用）
wingman.input.move(400, 300)
wingman.input.scroll(400, 300, -1)
```

:::

---

## Getting the slider range

> **Not implemented (planned)**: The `get_info()` return keys have no `minimum`/`maximum` fields. Reading the slider range is currently unavailable.

---

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_by_name(name)` | `find_by_name(name)` | Find by name |

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_info()` | `:get_info()` | Get general information about the slider |

> **Not implemented (planned)**: Reading the slider value/range (`get_value` numeric semantics, `minimum`/`maximum` fields) and setting it (`set_value` numeric semantics) are both not implemented — `get_value`/`set_value` actually read/set the element text and only work for text-type controls.
