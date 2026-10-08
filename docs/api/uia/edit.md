# API: UIA Edit

编辑框（Edit）是最常用的输入控件，用于单行文本输入、多行文本区域、密码输入等场景。

## 查找编辑框

### 按名称查找

**说明**：按编辑框的标签名称查找。通常编辑框旁边会有标签提示（如"用户名："），标签文字就是 Name。

**函数签名**：

```python
find_edit(name: str) -> UIElement | None
```

```lua
find_edit(name: string) -> UIElement | nil
```

**参数**：
- `name` - 编辑框的标签名称（如"用户名"、"搜索"、"密码"）

**返回**：找到的编辑框元素，未找到时返回 None/nil

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"用户名"的编辑框
edit = uia.find_edit("用户名")
if edit:
    # 找到了，可以操作
    edit["set_value"]("player123")
    print("已填写用户名")
else:
    print("未找到用户名输入框")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"用户名"的编辑框
local edit = wingman.uia.find_edit("用户名")
if edit then
    -- 找到了，可以操作
    edit:set_value("player123")
    print("已填写用户名")
else
    print("未找到用户名输入框")
end
```

:::

### 查找空名称编辑框

**说明**：传入空字符串时 `find_edit` **不按名称过滤**，返回查找到的第一个 Edit 角色控件（名称不限，不一定是没有名称的那个）。

**注意事项**：
- 如果界面有多个编辑框，命中的不一定是目标控件
- 建议配合 AutomationId 或位置信息来精确定位

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 返回第一个 Edit 控件（不区分名称）
edit = uia.find_edit("")
if edit:
    edit["set_value"]("some text")
    print("已填写内容")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 返回第一个 Edit 控件（不区分名称）
local edit = wingman.uia.find_edit("")
if edit then
    edit:set_value("some text")
    print("已填写内容")
end
```

:::

---

## 读写编辑框内容

### 获取当前内容

**说明**：读取编辑框中当前的文本内容。

**方法签名**：

```python
UIElement.get_value() -> str
```

```lua
UIElement:get_value() -> string
```

**返回**：编辑框中的文本内容

**使用场景**：
- 验证输入是否正确
- 读取搜索结果
- 获取用户填写的表单数据

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    # 读取当前搜索关键词
    current_text = edit["get_value"]()
    print(f"当前搜索内容: {current_text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("搜索")
if edit then
    -- 读取当前搜索关键词
    local currentText = edit:get_value()
    print("当前搜索内容: " .. currentText)
end
```

:::

### 设置内容

**说明**：设置编辑框的文本内容。这会**替换**原有的所有内容。

**方法签名**：

```python
UIElement.set_value(text: str) -> None
```

```lua
UIElement:set_value(text: string) -> None
```

**参数**：
- `text` - 要设置的文本内容

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    # 设置用户名
    edit["set_value"]("player123")
    print("已填写用户名")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("用户名")
if edit then
    -- 设置用户名
    edit:set_value("player123")
    print("已填写用户名")
end
```

:::

### 清空内容

**说明**：清空编辑框的所有内容。相当于 `set_value("")`。

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("搜索")
if edit:
    # 清空搜索框
    edit["set_value"]("")
    print("已清空搜索框")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("搜索")
if edit then
    -- 清空搜索框
    edit:set_value("")
    print("已清空搜索框")
end
```

:::

### 追加内容

**说明**：在现有内容后面添加新内容，而不是替换。需要先读取现有内容，再拼接。

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("日志")
if edit:
    # 读取现有内容
    current = edit["get_value"]()

    # 追加新行
    new_content = current + "\n[INFO] 新的日志条目"

    # 设置回编辑框
    edit["set_value"](new_content)
    print("已追加日志")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("日志")
if edit then
    -- 读取现有内容
    local current = edit:get_value()

    -- 追加新行
    local newContent = current .. "\n[INFO] 新的日志条目"

    -- 设置回编辑框
    edit:set_value(newContent)
    print("已追加日志")
end
```

:::

---

## 特殊编辑框类型

### 密码框

**说明**：密码框本质上也是 Edit 控件，但有以下特点：
- 输入时显示为圆点或星号
- 通常无法通过 UIA 读取密码内容（安全限制）
- 可以通过 `set_value()` 设置密码

**注意事项**：
- 不要尝试用 `get_value()` 读取密码，会返回空或隐藏字符
- 密码框的 Name 通常是"密码"或"Password"

:::tabs

== Python

```python:line-numbers
from wingman import uia

password_edit = uia.find_edit("密码")
if password_edit:
    # 设置密码（这是允许的）
    password_edit["set_value"]("mypassword123")
    print("已设置密码")

    # 注意：无法读取密码内容
    # content = password_edit["get_value"]()  # 返回空或 "*"
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local passwordEdit = wingman.uia.find_edit("密码")
if passwordEdit then
    -- 设置密码（这是允许的）
    passwordEdit:set_value("mypassword123")
    print("已设置密码")

    -- 注意：无法读取密码内容
    -- local content = passwordEdit:get_value()  -- 返回空或 "*"
end
```

:::

### 只读编辑框

**说明**：只读编辑框用于显示信息，不允许用户修改。常见于状态显示、日志输出等场景。

**识别方式**：`get_info()` 的返回键中**没有只读标记**（无 `is_readonly` 字段），脚本层目前无法直接识别只读编辑框；对只读编辑框调用 `set_value` 是否生效由应用侧决定。

:::tabs

== Python

```python:line-numbers
from wingman import uia

readonly_edit = uia.find_edit("状态信息")
if readonly_edit:
    # get_info 无只读字段，脚本层无法识别只读属性
    # 只读编辑框只能读取，写入是否生效由应用侧决定
    text = readonly_edit["get_value"]()
    print(f"状态信息: {text}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local readonlyEdit = wingman.uia.find_edit("状态信息")
if readonlyEdit then
    -- get_info 无只读字段，脚本层无法识别只读属性
    -- 只读编辑框只能读取，写入是否生效由应用侧决定
    local text = readonlyEdit:get_value()
    print("状态信息: " .. text)
end
```

:::

### 多行编辑框

**说明**：多行编辑框（也称为文本区域、TextArea）用于输入多行文本，如描述、评论、日志等。

**特点**：
- 支持 `\n` 换行符
- 通常有滚动条
- 可能显示为较大的输入区域

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 多行编辑框通常也是 Edit 类型
textarea = uia.find_edit("描述")
if textarea:
    # 设置多行文本（使用 \n 分隔各行）
    multi_line_text = """第一行内容
第二行内容
第三行内容"""

    textarea["set_value"](multi_line_text)
    print("已填写多行描述")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 多行编辑框通常也是 Edit 类型
local textarea = wingman.uia.find_edit("描述")
if textarea then
    -- 设置多行文本（使用 \n 分隔各行）
    local multiLineText = "第一行内容\n第二行内容\n第三行内容"

    textarea:set_value(multiLineText)
    print("已填写多行描述")
end
```

:::

---

## 编辑框操作

### 设置焦点

**说明**：将输入焦点设置到编辑框，使后续的键盘输入会进入这个编辑框。

**使用场景**：
- 准备输入前确保焦点正确
- 配合 `input.key()` / `input.type()` 使用

**方法签名**：

```python
UIElement.focus() -> None
```

```lua
UIElement:focus() -> None
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    # 设置焦点到用户名输入框
    edit["focus"]()
    print("焦点已设置到用户名输入框")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("用户名")
if edit then
    -- 设置焦点到用户名输入框
    edit:focus()
    print("焦点已设置到用户名输入框")
end
```

:::

### 全选文本

**说明**：选中编辑框中的所有文本。通常配合复制、删除操作使用。

**实现方式**：使用快捷键 Ctrl+A（或 Command+A on Mac）

:::tabs

== Python

```python:line-numbers
from wingman import uia, input

edit = uia.find_edit("搜索")
if edit:
    # 1. 先设置焦点
    edit["focus"]()

    # 2. 发送 Ctrl+A 全选（keyDown/keyUp 组合：0x11=Ctrl，0x41=A）
    input.key_down(0x11)
    input.key(0x41)
    input.key_up(0x11)

    # 3. 可以继续操作，如输入替换文本
    # input.type("新的搜索词")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("搜索")
if edit then
    -- 1. 先设置焦点
    edit:focus()

    -- 2. 发送 Ctrl+A 全选（keyDown/keyUp 组合：0x11=Ctrl，0x41=A）
    wingman.input.keyDown(0x11)
    wingman.input.key(0x41)
    wingman.input.keyUp(0x11)
end
```

:::

---

## 获取编辑框信息

### 查看所有属性

**说明**：`get_info()` 方法返回编辑框的所有可用属性，帮助你了解编辑框的状态和特性。

**常用属性**：
- `name` - 编辑框名称（可能为空）
- `text` - 当前文本内容
- `id` - AutomationId（如果有）
- `className` - 控件类名
- `role` - 角色数值（int，TextBox = 3）
- `is_enabled` / `is_visible` / `has_focus` - 状态位
- `bounds` - 位置和大小（`{x, y, width, height}`）

> get_info 返回键中**没有** `is_readonly`（只读）与 `is_password`（密码框）标记。

:::tabs

== Python

```python:line-numbers
from wingman import uia

edit = uia.find_edit("用户名")
if edit:
    info = edit["get_info"]()

    print(f"编辑框名称: {info.get('name', '')}")
    print(f"当前内容: {info.get('text', '')}")
    print(f"id: {info.get('id', '')}")
    print(f"类名: {info.get('className', '')}")
    print(f"角色: {info.get('role', 0)}")  # 3 = UIARole TextBox
    print(f"是否启用: {info.get('is_enabled', True)}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local edit = wingman.uia.find_edit("用户名")
if edit then
    local info = edit:get_info()

    print("编辑框名称: " .. (info.name or ""))
    print("当前内容: " .. (info.text or ""))
    print("id: " .. (info.id or ""))
    print("类名: " .. (info.className or ""))
    print("角色: " .. tostring(info.role or 0))  -- 3 = UIARole TextBox
    print("是否启用: " .. tostring(info.is_enabled or false))
end
```

:::

---

## 完整示例

### 登录表单填写

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

def login(username, password):
    """填写登录表单"""

    # 1. 填写用户名
    username_edit = uia.find_edit("用户名")
    if username_edit:
        username_edit["set_value"](username)
        print(f"已填写用户名: {username}")
    else:
        print("未找到用户名输入框")
        return False

    # 2. 填写密码
    password_edit = uia.find_edit("密码")
    if password_edit:
        password_edit["set_value"](password)
        print("已填写密码")
    else:
        print("未找到密码输入框")
        return False

    # 3. 点击登录按钮
    login_btn = uia.find_button("登录")
    if login_btn:
        login_btn["click"]()
        print("已点击登录按钮")
        return True
    else:
        print("未找到登录按钮")
        return False

# 执行登录
if login("player123", "mypassword"):
    print("登录表单填写完成")
else:
    print("登录失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function login(username, password)
    -- 1. 填写用户名
    local usernameEdit = wingman.uia.find_edit("用户名")
    if usernameEdit then
        usernameEdit:set_value(username)
        print("已填写用户名: " .. username)
    else
        print("未找到用户名输入框")
        return false
    end

    -- 2. 填写密码
    local passwordEdit = wingman.uia.find_edit("密码")
    if passwordEdit then
        passwordEdit:set_value(password)
        print("已填写密码")
    else
        print("未找到密码输入框")
        return false
    end

    -- 3. 点击登录按钮
    local loginBtn = wingman.uia.find_button("登录")
    if loginBtn then
        loginBtn:click()
        print("已点击登录按钮")
        return true
    else
        print("未找到登录按钮")
        return false
    end
end

-- 执行登录
if login("player123", "mypassword") then
    print("登录表单填写完成")
else
    print("登录失败")
end
```

:::

---

## 可用接口

### 查找编辑框

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_edit(name)` | `find_edit(name)` | 按名称查找编辑框 | `name` - 编辑框名称（传空串返回第一个 Edit，不区分名称） |
| `find_by_id(id)` | `find_by_id(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 编辑框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_value()` | `:get_value()` | 获取当前文本内容 |
| `set_value(text)` | `:set_value(text)` | 设置文本内容 |
| `focus()` | `:focus()` | 设置焦点到编辑框 |
| `get_info()` | `:get_info()` | 获取编辑框所有属性 |

### 编辑框属性

`get_info()` 返回键集：

| 属性 | 类型 | 说明 |
|-----|------|------|
| `name` | string | 编辑框名称（可能为空） |
| `text` | string | 当前文本内容 |
| `id` | string | AutomationId（可能为空） |
| `className` | string | 控件类名 |
| `role` | int | 角色数值（TextBox = 3） |
| `is_enabled` | boolean | 是否启用 |
| `is_visible` | boolean | 是否可见 |
| `has_focus` | boolean | 是否持有焦点 |
| `bounds` | object | 位置和大小 `{x, y, width, height}` |

> 无 `is_readonly` / `is_password` 字段。
