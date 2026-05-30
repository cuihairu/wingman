# API: UIA Text

文本控件，用于显示静态文本标签。Text 控件通常只读，不能修改。

## 查找文本控件

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找显示标签
label = uia.find_text("用户名：")
if label:
    print("找到文本标签")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找显示标签
local label = uia.findText("用户名：")
if label then
    print("找到文本标签")
end
```

:::

---

## 获取文本内容

Text 控件主要用于读取显示文本：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找欢迎文本
welcome = uia.find_text("欢迎使用")
if welcome:
    info = welcome.get_info()
    print(f"文本内容: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找欢迎文本
local welcome = uia.findText("欢迎使用")
if welcome then
    local info = welcome:getInfo()
    print("文本内容: " .. info.name)
end
```

:::

---

## 作为定位锚点

Text 控件常用于定位其他控件。例如，找到"用户名："标签后，可以在其附近找到输入框：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 找到标签
label = uia.find_text("用户名：")
if label:
    # 获取标签位置
    info = label.get_info()
    if 'bounding_rect' in info:
        rect = info['bounding_rect']
        # 在标签右侧查找编辑框
        # 具体实现取决于应用结构
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 找到标签
local label = uia.findText("用户名：")
if label then
    -- 获取标签位置
    local info = label:getInfo()
    if info.boundingRect then
        local rect = info.boundingRect
        -- 在标签右侧查找编辑框
        -- 具体实现取决于应用结构
    end
end
```

:::

---

## 可用接口

### 查找文本控件

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_text(name)` | `findText(name)` | 按名称查找文本控件 |
| `find_by_name(name)` | `findByName(name)` | 通用查找方法 |

### 文本控件操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_info()` | `:getInfo()` | 获取文本信息（包含 name 属性） |

> **注意**：Text 控件通常只读，不支持修改内容。
