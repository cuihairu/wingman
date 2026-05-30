# API: UIA Text

文本（Text）控件用于显示静态文本标签，如字段标签、提示信息、状态显示等。

**重要**：Text 控件通常是**只读**的，不能修改其内容。

## 查找文本控件

**说明**：文本控件通过其显示的文本来查找。

**函数签名**：
- Python: `find_text(name: str) -> UIElement | None`
- Lua: `findText(name: string) -> UIElement | nil`

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
local uia = require("wingman.uia")

-- 查找显示"用户名："的文本标签
local label = uia.findText("用户名：")
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
    info = welcome.get_info()
    text = info.get('name', '')
    print(f"文本内容: {text}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找欢迎文本
local welcome = uia.findText("欢迎使用")
if welcome then
    local info = welcome:getInfo()
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
    info = label.get_info()
    print(f"找到标签: {info.get('name', '')}")

    # 获取标签位置（可用于坐标定位）
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        # 可以根据标签位置推断输入框位置
        # 例如：输入框可能在标签右侧
        input_x = rect['left'] + rect['width'] + 10
        input_y = rect['top']
        print(f"推测输入框位置: ({input_x}, {input_y})")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 找到标签
local label = uia.findText("用户名：")
if label then
    local info = label:getInfo()
    print("找到标签: " .. (info.name or ""))

    -- 获取标签位置（可用于坐标定位）
    if info.boundingRect then
        local rect = info.boundingRect
        -- 可以根据标签位置推断输入框位置
        local inputX = rect.left + rect.width + 10
        local inputY = rect.top
        print(string.format("推测输入框位置: (%d, %d)", inputX, inputY))
    end
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_text(name)` | `findText(name)` | 按名称查找文本控件 |
| `find_by_name(name)` | `findByName(name)` | 通用查找方法 |

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取文本信息（包含 name 属性） |

> **注意**：Text 控件通常只读，不支持修改内容。
