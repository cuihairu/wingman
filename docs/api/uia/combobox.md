# API: UIA ComboBox

下拉框（ComboBox）用于从预定义的选项列表中选择一个值。常见场景包括：
- 选择国家/地区
- 选择语言
- 选择分类
- 可搜索的下拉选择

## 查找下拉框

**说明**：下拉框通常有标签文字，可以通过名称查找。

**函数签名**：

```python
find_by_name(name: str) -> UIElement | None
```

```lua
findByName(name: string) -> UIElement | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"国家/地区"的下拉框
combo = uia.find_by_name("国家/地区")
if combo:
    print("找到下拉框")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找名为"国家/地区"的下拉框
local combo = uia.findByName("国家/地区")
if combo then
    print("找到下拉框")
end
```

:::

---

## 操作下拉框

### 获取当前选中值

**说明**：查看下拉框当前选中的值。

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    info = combo.get_info()
    current_value = info.get('value', '')
    print(f"当前选择: {current_value}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local combo = uia.findByName("国家/地区")
if combo then
    local info = combo:getInfo()
    local currentValue = info.value or ""
    print("当前选择: " .. currentValue)
end
```

:::

### 展开并选择选项

**说明**：展开下拉框，然后查找并点击目标选项。

**步骤**：
1. 使用 `expand()` 展开下拉框
2. 等待选项列表出现
3. 查找目标选项并点击

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

combo = uia.find_by_name("国家/地区")
if combo:
    # 1. 展开下拉框
    combo.expand()
    print("已展开下拉框")

    # 2. 等待选项列表出现
    util.sleep(300)

    # 3. 查找并点击目标选项
    option = uia.find_by_name("中国")
    if option:
        option.click()
        print("已选择：中国")
    else:
        print("未找到目标选项")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local combo = uia.findByName("国家/地区")
if combo then
    -- 1. 展开下拉框
    combo:expand()
    print("已展开下拉框")

    -- 2. 等待选项列表出现
    util.sleep(300)

    -- 3. 查找并点击目标选项
    local option = uia.findByName("中国")
    if option then
        option:click()
        print("已选择：中国")
    else
        print("未找到目标选项")
    end
end
```

:::

### 直接设置值（如果支持）

**说明**：某些下拉框支持直接设置值，无需展开。

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    # 尝试直接设置值
    combo.set_value("中国")
    print("已设置为中国")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local combo = uia.findByName("国家/地区")
if combo then
    -- 尝试直接设置值
    combo:setValue("中国")
    print("已设置为中国")
end
```

:::

---

## 可编辑下拉框

**说明**：某些下拉框允许用户手动输入文本，这种情况下控件实际类型是 Edit，而非 ComboBox。

**特点**：
- 既可以输入文本
- 也可以展开选择预设选项
- 常见于搜索框、筛选器等

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 可编辑下拉框通常也是 Edit 类型
editable_combo = uia.find_edit("搜索")
if editable_combo:
    # 方法 1: 直接输入文本
    editable_combo.set_value("搜索关键词")

    # 方法 2: 展开选择预设选项
    # editable_combo.expand()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 可编辑下拉框通常也是 Edit 类型
local editableCombo = uia.findEdit("搜索")
if editableCombo then
    -- 方法 1: 直接输入文本
    editableCombo:setValue("搜索关键词")

    -- 方法 2: 展开选择预设选项
    -- editableCombo:expand()
end
```

:::

---

## 获取所有选项

**说明**：展开下拉框后，可以遍历所有可选选项。

**使用场景**：
- 验证选项是否存在
- 动态选择符合条件的选项
- 调试了解下拉框内容

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

combo = uia.find_by_name("国家/地区")
if combo:
    # 展开下拉框
    combo.expand()
    util.sleep(300)

    # 获取所有 ListItem 类型的选项
    options = uia.find_all_by_control_type("ListItem")

    print(f"共有 {len(options)} 个选项：")
    for i, opt in enumerate(options):
        info = opt.get_info()
        print(f"  [{i}] {info.get('name', '')}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local combo = uia.findByName("国家/地区")
if combo then
    -- 展开下拉框
    combo:expand()
    util.sleep(300)

    -- 获取所有 ListItem 类型的选项
    local options = uia.findAllByControlType("ListItem")

    print("共有 " .. #options .. " 个选项：")
    for i, opt in ipairs(options) do
        local info = opt:getInfo()
        print(string.format("  [%d] %s", i, info.name or ""))
    end
end
```

:::

---

## 可用接口

### 查找下拉框

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 | `name` - 下拉框标签 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 下拉框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开下拉框 |
| `collapse()` | `:collapse()` | 折叠下拉框 |
| `set_value(value)` | `:setValue(value)` | 设置选中值（如果支持） |
| `get_value()` | `:getValue()` | 获取当前选中值 |
