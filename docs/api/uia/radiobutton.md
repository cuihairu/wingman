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
- Python: `find_by_name(name: str) -> UIElement | None`
- Lua: `findByName(name: string) -> UIElement | nil`

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
local uia = require("wingman.uia")

-- 查找名为"男"的单选按钮
local radio = uia.findByName("男")
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
    radio.click()
    print("已选中：男")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    -- 点击选中
    radio:click()
    print("已选中：男")
end
```

:::

### 通过设置值选中

**说明**：使用 `set_value(True)` 来选中单选按钮。

**方法签名**：
- Python: `set_value(value: bool) -> None`
- Lua: `:setValue(value: boolean) -> None`

**参数**：
- `value` - True 选中，False 取消选中

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    # 设置为选中
    radio.set_value(True)
    print("已选中：男")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    -- 设置为选中
    radio:setValue(true)
    print("已选中：男")
end
```

:::

---

## 获取选中状态

### 检查单个单选按钮

**说明**：检查某个单选按钮是否被选中。

:::tabs

== Python

```python:line-numbers
from wingman import uia

radio = uia.find_by_name("男")
if radio:
    info = radio.get_info()
    state = info.get('toggle_state', 'Off')

    # toggle_state 为 'On' 表示选中
    is_selected = state == 'On'

    print(f"是否选中: {is_selected}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local radio = uia.findByName("男")
if radio then
    local info = radio:getInfo()
    local state = info.toggleState or "Off"

    -- toggle_state 为 "On" 表示选中
    local isSelected = state == "On"

    print("是否选中: " .. tostring(isSelected))
end
```

:::

### 获取一组单选按钮的选择

**说明**：遍历所有单选按钮，找出被选中的那个。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找所有性别相关的单选按钮
male = uia.find_by_name("男")
female = uia.find_by_name("女")
other = uia.find_by_name("其他")

# 检查哪个被选中
for radio, label in [(male, "男"), (female, "女"), (other, "其他")]:
    if radio:
        info = radio.get_info()
        if info.get('toggle_state') == 'On':
            print(f"当前选择: {label}")
            break
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找所有性别相关的单选按钮
local male = uia.findByName("男")
local female = uia.findByName("女")
local other = uia.findByName("其他")

-- 检查哪个被选中
local options = {
    {male, "男"},
    {female, "女"},
    {other, "其他"}
}

for i, option in ipairs(options) do
    local radio, label = option[1], option[2]
    if radio then
        local info = radio:getInfo()
        if info.toggleState == "On" then
            print("当前选择: " .. label)
            break
        end
    end
end
```

:::

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
        radio.click()
        print(f"已选择性别: {gender}")

        # 验证是否成功选中
        info = radio.get_info()
        if info.get('toggle_state') == 'On':
            return True
        else:
            print("验证失败：单选按钮未选中")
            return False
    else:
        print(f"未找到性别选项: {gender}")
        return False

# 使用
if select_gender("女"):
    print("性别选择成功")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

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
    local radio = uia.findByName(gender)
    if radio then
        radio:click()
        print("已选择性别: " .. gender)

        -- 验证是否成功选中
        local info = radio:getInfo()
        if info.toggleState == "On" then
            return true
        else
            print("验证失败：单选按钮未选中")
            return false
        end
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
| `find_by_name(name)` | `findByName(name)` | 按名称查找 | `name` - 单选按钮标签 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 单选按钮操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `click()` | `:click()` | 点击选中 |
| `set_value(bool)` | `:setValue(bool)` | 设置选中状态 |
| `get_value()` | `:getValue()` | 获取是否选中 |
| `get_info()` | `:getInfo()` | 获取所有属性 |

### 单选按钮属性

| 属性 | 类型 | 说明 |
|-----|------|------|
| `name` | string | 单选按钮标签 |
| `toggle_state` | string | 状态：On（选中）/Off（未选中） |
| `is_toggle_pattern` | boolean | 是否支持切换（通常为 True） |
