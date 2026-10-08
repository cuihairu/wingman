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
local slider = wingman.uia.find_by_name("音量")
if slider then
    print("找到滑块")
end
```

:::

---

## 获取当前值

> **未实现（计划中）**：`get_info()` 的返回键中没有数值字段（无 `value`），`get_value()` 返回的是元素文本（实现为 `getText`），不是滑块数值。滑块当前值读取暂不可用。

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

## 设置滑块值

> **未实现（计划中）**：`set_value(number)` 设置滑块值的能力未实现——`set_value` 的实现是设置元素文本（`setText`），对滑块调用不会拖动滑块或改变数值。调值需等待 Range/Value 模式接口补齐；过渡方案可用 `input.scroll()` 或拖拽模拟。

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

## 获取滑块范围

> **未实现（计划中）**：`get_info()` 的返回键中没有 `minimum`/`maximum` 字段，滑块范围读取暂不可用。

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:get_info()` | 获取滑块通用信息 |

> **未实现（计划中）**：滑块值/范围的读取（`get_value` 数值语义、`minimum`/`maximum` 字段）与设置（`set_value` 数值语义）均未实现——`get_value`/`set_value` 实为读取/设置元素文本，仅对文本型控件有效。
