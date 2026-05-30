# API: UIA Edit

编辑框控件，用于输入和显示文本。

## 查找编辑框

### 按名称查找

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    edit.set_value("player123")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("用户名")
if edit then
    edit:setValue("player123")
end
```

:::

### 查找空名称编辑框（常见）

很多编辑框没有名称，可以传入空字符串：

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("")
if edit:
    edit.set_value("some text")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("")
if edit then
    edit:setValue("some text")
end
```

:::

---

## 读写编辑框内容

### 获取内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    text = edit.get_value()
    print(f"当前内容: {text}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("搜索")
if edit then
    local text = edit:getValue()
    print("当前内容: " .. text)
end
```

:::

### 设置内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    edit.set_value("player123")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("用户名")
if edit then
    edit:setValue("player123")
end
```

:::

### 清空内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    edit.set_value("")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("搜索")
if edit then
    edit:setValue("")
end
```

:::

### 追加内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("日志")
if edit:
    current = edit.get_value()
    edit.set_value(current + "\n新行内容")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("日志")
if edit then
    local current = edit:getValue()
    edit:setValue(current .. "\n新行内容")
end
```

:::

---

## 特殊编辑框类型

### 密码框

密码框也是 Edit 类型，但通常无法读取内容：

:::tabs

== Python

```python:line-numbers
from wingman import uia

password_edit = uia.find_edit("密码")
if password_edit:
    # 设置密码
    password_edit.set_value("mypassword123")

    # 注意：密码框内容通常无法直接读取
    # get_value() 会返回空或隐藏字符
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local passwordEdit = uia.findEdit("密码")
if passwordEdit then
    -- 设置密码
    passwordEdit:setValue("mypassword123")

    -- 注意：密码框内容通常无法直接读取
    -- getValue() 会返回空或隐藏字符
end
```

:::

### 只读编辑框

只读编辑框用于显示信息，不能修改：

:::tabs

== Python

```python:line-numbers
from wingman import uia

readonly_edit = uia.find_edit("状态信息")
if readonly_edit:
    info = readonly_edit.get_info()
    if info.get('is_readonly', False):
        text = readonly_edit.get_value()
        print(f"状态: {text}")
        # 不能设置只读编辑框的值
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local readonlyEdit = uia.findEdit("状态信息")
if readonlyEdit then
    local info = readonlyEdit:getInfo()
    if info.isReadOnly then
        local text = readonlyEdit:getValue()
        print("状态: " .. text)
        -- 不能设置只读编辑框的值
    end
end
```

:::

### 多行编辑框

多行编辑框（文本区域）的操作方式与单行相同：

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 多行编辑框通常也是 Edit 类型
textarea = uia.find_edit("描述")
if textarea:
    # 设置多行文本
    textarea.set_value("第一行\n第二行\n第三行")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 多行编辑框通常也是 Edit 类型
local textarea = uia.findEdit("描述")
if textarea then
    -- 设置多行文本
    textarea:setValue("第一行\n第二行\n第三行")
end
```

:::

---

## 编辑框操作

### 设置焦点

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    edit.focus()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("用户名")
if edit then
    edit:focus()
end
```

:::

### 全选文本

:::tabs

== Python

```python:line-numbers
from wingman import uia, input

edit = uia.find_edit("搜索")
if edit:
    edit.focus()
    # 使用 Ctrl+A 全选
    input.send_keys("^a")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")

local edit = uia.findEdit("搜索")
if edit then
    edit:focus()
    -- 使用 Ctrl+A 全选
    input.sendKeys("^a")
end
```

:::

---

## 获取编辑框信息

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    info = edit.get_info()
    print(f"名称: {info.get('name', '')}")
    print(f"当前值: {info.get('value', '')}")
    print(f"只读: {info.get('is_readonly', False)}")
    print(f"密码: {info.get('is_password', False)}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local edit = uia.findEdit("用户名")
if edit then
    local info = edit:getInfo()
    print("名称: " .. (info.name or ""))
    print("当前值: " .. (info.value or ""))
    print("只读: " .. tostring(info.isReadOnly or false))
    print("密码: " .. tostring(info.isPassword or false))
end
```

:::

---

## 可用接口

### 查找编辑框

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_edit(name)` | `findEdit(name)` | 按名称查找编辑框 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 编辑框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:getValue()` | 获取当前文本 |
| `set_value(text)` | `:setValue(text)` | 设置文本 |
| `focus()` | `:focus()` | 设置焦点 |
| `get_info()` | `:getInfo()` | 获取编辑框信息 |
