> [中文](scrollbar.md) | **English**

# API: UIA ScrollBar

The ScrollBar control is used to scroll a content area. There are two types: horizontal and vertical.

## Finding a ScrollBar

**Note**: ScrollBars usually have names, or can be located by type.

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
local wingman = require("wingman")

-- 查找垂直滚动条
local vScroll = wingman.uia.find_by_name("垂直滚动条")
if vScroll then
    print("找到垂直滚动条")
end

-- 查找水平滚动条
local hScroll = wingman.uia.find_by_name("水平滚动条")
if hScroll then
    print("找到水平滚动条")
end
```

:::

---

## Getting the scroll position

> **Not implemented (planned)**: The `get_info()` return keys have no scroll value field (no `value`), and `get_value()` returns the element text (implemented as `getText`), not the scroll position. Reading the scroll position is currently unavailable.

---

## Setting the scroll position

> **Not implemented (planned)**: `set_value(number)` cannot set the scroll position — `set_value` is implemented as setting the element text (`setText`), and calling it on a ScrollBar does not change the scroll position. For scrolling, use `input.scroll(x, y, delta)` instead (simulates the mouse wheel, input module).

:::tabs

== Python

```python:line-numbers
from wingman import input

# 在指定坐标处滚动（delta 正负号为滚动方向，按平台约定）
input.scroll(500, 400, -3)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 在指定坐标处滚动（delta 正负号为滚动方向，按平台约定）
wingman.input.scroll(500, 400, -3)
```

:::

---

## Getting the scroll range

> **Not implemented (planned)**: The `get_info()` return keys have no `minimum`/`maximum` fields. Reading the scroll range is currently unavailable.

---

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_by_name(name)` | `find_by_name(name)` | Find by name |

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_info()` | `:get_info()` | Get general information about the scroll bar |

> **Not implemented (planned)**: Reading the scroll position/range (`get_value` numeric semantics, `minimum`/`maximum` fields) and setting it (`set_value` numeric semantics) are both not implemented — `get_value`/`set_value` actually read/set the element text and only work for text-type controls.
