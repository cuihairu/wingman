# API: UIA ScrollBar

滚动条（ScrollBar）控件用于滚动内容区域，有水平和垂直两种类型。

## 查找滚动条

**说明**：滚动条通常有名称或可通过类型查找。

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

## 获取滚动位置

> **未实现（计划中）**：`get_info()` 的返回键中没有滚动数值字段（无 `value`），`get_value()` 返回的是元素文本（实现为 `getText`），不是滚动位置。滚动位置读取暂不可用。

---

## 设置滚动位置

> **未实现（计划中）**：`set_value(number)` 设置滚动位置的能力未实现——`set_value` 的实现是设置元素文本（`setText`），对滚动条调用不会改变滚动位置。滚动请改用 `input.scroll(x, y, delta)`（模拟鼠标滚轮，input 模块）。

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

## 获取滚动范围

> **未实现（计划中）**：`get_info()` 的返回键中没有 `minimum`/`maximum` 字段，滚动范围读取暂不可用。

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:get_info()` | 获取滚动条通用信息 |

> **未实现（计划中）**：滚动位置/范围的读取（`get_value` 数值语义、`minimum`/`maximum` 字段）与设置（`set_value` 数值语义）均未实现——`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效。
