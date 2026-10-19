> [中文](text.md) | **English**

# API: UIA Text

The Text control is used to display static text labels, such as field labels, hints, and status displays.

**Important**: Text controls are usually **read-only**; their content cannot be modified.

## Finding a Text control

**Note**: Text controls are found by the text they display. Note: the UIARole enum **has no Text-specific role value**; `find_text` matches by name and **does not filter by role**, so it may hit a same-named element of any type.

**Function signature**:

```python
find_text(name: str) -> UIElement | None
```

```lua
find_text(name: string) -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找显示"用户名："的文本标签
label = uia.find_text("用户名：")
if label:
    print("找到文本标签")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找显示"用户名："的文本标签
local label = wingman.uia.find_text("用户名：")
if label then
    print("找到文本标签")
end
```

:::

---

## Getting the text content

**Note**: Reads the content displayed by the Text control. Commonly used for verification or information retrieval.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找欢迎文本
welcome = uia.find_text("欢迎使用")
if welcome:
    info = welcome["get_info"]()
    text = info.get('name', '')
    print(f"文本内容: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找欢迎文本
local welcome = wingman.uia.find_text("欢迎使用")
if welcome then
    local info = welcome.get_info()
    local text = info.name or ""
    print("文本内容: " .. text)
end
```

:::

---

## As a positioning anchor

**Note**: Text controls are often used to locate other controls. For example, after finding the "用户名：" label, you know the input field is nearby.

**Note**: This must be combined with other locating methods, since a Text control cannot directly "point to" other controls.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 找到标签
label = uia.find_text("用户名：")
if label:
    info = label["get_info"]()
    print(f"找到标签: {info.get('name', '')}")

    # 获取标签位置（可用于坐标定位）
    if 'bounds' in info:
        rect = info['bounds']
        # 可以根据标签位置推断输入框位置
        # 例如：输入框可能在标签右侧
        input_x = rect['x'] + rect['width'] + 10
        input_y = rect['y']
        print(f"推测输入框位置: ({input_x}, {input_y})")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 找到标签
local label = wingman.uia.find_text("用户名：")
if label then
    local info = label.get_info()
    print("找到标签: " .. (info.name or ""))

    -- 获取标签位置（可用于坐标定位）
    if info.bounds then
        local rect = info.bounds
        -- 可以根据标签位置推断输入框位置
        local inputX = rect.x + rect.width + 10
        local inputY = rect.y
        print(string.format("推测输入框位置: (%d, %d)", inputX, inputY))
    end
end
```

:::

---

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_text(name)` | `find_text(name)` | Find by name (no Text-specific role; no role filtering) |
| `find_by_name(name)` | `find_by_name(name)` | General finding method |

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `get_info()` | `:get_info()` | Get text information (includes the name attribute) |

> **Note**: Text controls are usually read-only and do not support content modification.
