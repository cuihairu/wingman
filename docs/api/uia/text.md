# API: UIA Text

文本（Text）控件用于显示静态文本标签，如字段标签、提示信息、状态显示等。

**重要**：Text 控件通常是**只读**的，不能修改其内容。

## 查找文本控件

**说明**：文本控件通过其显示的文本来查找。注意：UIARole 枚举**没有 Text 专用角色值**，`find_text` 按名称匹配且**不做角色过滤**，可能命中任意类型的同名元素。

**函数签名**：

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

## 获取文本内容

**说明**：读取 Text 控件显示的内容。通常用于验证或获取信息。

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
    local info = welcome:get_info()
    local text = info.name or ""
    print("文本内容: " .. text)
end
```

:::

---

## 作为定位锚点

**说明**：Text 控件常用于定位其他控件。例如，找到"用户名："标签后，可以知道输入框就在附近。

**注意**：这需要配合其他定位方式，因为 Text 控件本身不能直接"指向"其他控件。

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
    local info = label:get_info()
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

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_text(name)` | `find_text(name)` | 按名称查找（无 Text 专用角色，不做角色过滤） |
| `find_by_name(name)` | `find_by_name(name)` | 通用查找方法 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:get_info()` | 获取文本信息（包含 name 属性） |

> **注意**：Text 控件通常只读，不支持修改内容。
