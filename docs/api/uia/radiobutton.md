# API: UIA RadioButton

单选按钮（RadioButton）用于从多个选项中**选择一个**。同一组内的单选按钮是互斥的——选中一个会自动取消其他选项。

常见场景：
- 选择性别
- 选择支付方式
- 选择单选题答案
- 选择主题颜色

## 查找单选按钮

**说明**：单选按钮通常有标签文字，可以通过名称查找。

**函数签名**：

```python
find_by_name(name: str) -> UIElement | None
```

```lua
find_by_name(name: string) -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"男"的单选按钮
radio = uia.find_by_name("男")
if radio:
    print("找到单选按钮")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"男"的单选按钮
local radio = wingman.uia.find_by_name("男")
if radio then
    print("找到单选按钮")
end
```

:::

---

## 选择单选按钮

### 通过点击选中

**说明**：模拟用户点击单选按钮来选中它。这是最常用的方式。

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    # 点击选中
    radio["click"]()
    print("已选中：男")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local radio = wingman.uia.find_by_name("男")
if radio then
    -- 点击选中
    radio:click()
    print("已选中：男")
end
```

:::

### 通过设置值选中

> **未实现（计划中）**：`set_value(bool)` 选中单选按钮的能力未实现——`set_value` 的实现是设置元素文本（`setText`），**仅对文本型控件有效**，对单选按钮调用不会选中它。选中请用 `click()`（见上节）。

---

## 获取选中状态

> **未实现（计划中）**：`get_info()` 的返回键中没有 `toggle_state` 字段，`get_value()` 返回的也是元素文本而非选中状态——脚本层目前**无法读取单选按钮是否被选中**，也就无法遍历找出现被选中的那个。选中验证需等待选中状态接口（如 is_checked）补齐。

---

## 完整示例

### 选择性别

:::tabs

== Python

```python:line-numbers
from wingman import uia

def select_gender(gender):
    """选择性别"""

    # 可用的性别选项
    options = {
        "男": "male",
        "女": "female",
        "其他": "other"
    }

    if gender not in options:
        print(f"无效的性别选项: {gender}")
        return False

    # 查找并点击对应的单选按钮
    radio = uia.find_by_name(gender)
    if radio:
        radio["click"]()
        print(f"已选择性别: {gender}")
        # 脚本层暂无选中状态读取接口（get_info 无 toggle_state），无法回读验证
        return True
    else:
        print(f"未找到性别选项: {gender}")
        return False

# 使用
if select_gender("女"):
    print("性别选择成功")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function selectGender(gender)
    -- 可用的性别选项
    local options = {
        ["男"] = true,
        ["女"] = true,
        ["其他"] = true
    }

    if not options[gender] then
        print("无效的性别选项: " .. gender)
        return false
    end

    -- 查找并点击对应的单选按钮
    local radio = wingman.uia.find_by_name(gender)
    if radio then
        radio:click()
        print("已选择性别: " .. gender)
        -- 脚本层暂无选中状态读取接口（get_info 无 toggle_state），无法回读验证
        return true
    else
        print("未找到性别选项: " .. gender)
        return false
    end
end

-- 使用
if selectGender("女") then
    print("性别选择成功")
end
```

:::

---

## 可用接口

### 查找单选按钮

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 | `name` - 单选按钮标签 |
| `find_by_id(id)` | `find_by_id(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 单选按钮操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `click()` | `:click()` | 点击选中 |
| `get_info()` | `:get_info()` | 获取所有通用属性（不含选中状态） |

> **未实现（计划中）**：`set_value(bool)` 设置选中与选中状态读取（`get_value` 返回布尔态、`toggle_state` 字段）均未实现——`set_value`/`get_value` 实为设置/读取元素文本，仅对文本型控件有效。

### 单选按钮属性

`get_info()` 返回键集见 [概述](./index.md#uielement-通用方法)（`name`/`id`/`className`/`role`/`text`/`is_enabled`/`is_visible`/`has_focus`/`bounds`），其中**没有**选中状态字段（无 `toggle_state`/`is_toggle_pattern`）。
