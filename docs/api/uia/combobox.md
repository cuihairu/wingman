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
find_by_name(name: string) -> UIElement | nil
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
local wingman = require("wingman")

-- 查找名为"国家/地区"的下拉框
local combo = wingman.uia.find_by_name("国家/地区")
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
    # get_value 返回元素文本（对下拉框通常是当前选中项的文字）
    current_value = combo["get_value"]()
    print(f"当前选择: {current_value}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local combo = wingman.uia.find_by_name("国家/地区")
if combo then
    -- get_value 返回元素文本（对下拉框通常是当前选中项的文字）
    local currentValue = combo:get_value()
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
local wingman = require("wingman")

local combo = wingman.uia.find_by_name("国家/地区")
if combo then
    -- 1. 展开下拉框
    combo:expand()
    print("已展开下拉框")

    -- 2. 等待选项列表出现
    wingman.util.sleep(300)

    -- 3. 查找并点击目标选项
    local option = wingman.uia.find_by_name("中国")
    if option then
        option:click()
        print("已选择：中国")
    else
        print("未找到目标选项")
    end
end
```

:::

### 直接设置值（仅文本型有效）

**说明**：`set_value` 的实现是设置元素文本（`setText`），仅对支持文本设置的控件生效（如可编辑下拉框）；对纯选择型下拉框无效，选择选项请用「展开并选择」方式。

:::tabs

== Python

```python:line-numbers
from wingman import uia

combo = uia.find_by_name("国家/地区")
if combo:
    # 仅对支持文本设置的下拉框生效（实为设置文本）
    combo["set_value"]("中国")
    print("已设置为中国")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local combo = wingman.uia.find_by_name("国家/地区")
if combo then
    -- 仅对支持文本设置的下拉框生效（实为设置文本）
    combo:set_value("中国")
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
    # 方法 1: 直接输入文本（set_value 实为设置文本）
    editable_combo["set_value"]("搜索关键词")

    # 方法 2: 展开选择预设选项
    # editable_combo.expand()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 可编辑下拉框通常也是 Edit 类型
local editableCombo = wingman.uia.findEdit("搜索")
if editableCombo then
    -- 方法 1: 直接输入文本（set_value 实为设置文本）
    editableCombo:set_value("搜索关键词")

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

    # 获取所有 ListItem 角色（UIARole 8）的元素
    options = uia.find_all_by_control_type(8)

    print(f"共有 {len(options)} 个选项：")
    for i, opt in enumerate(options):
        info = opt["get_info"]()
        print(f"  [{i}] {info.get('name', '')}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local combo = wingman.uia.find_by_name("国家/地区")
if combo then
    -- 展开下拉框
    combo:expand()
    wingman.util.sleep(300)

    -- 获取所有 ListItem 角色（UIARole 8）的元素
    local options = wingman.uia.find_all_by_control_type(8)

    print("共有 " .. #options .. " 个选项：")
    for i, opt in ipairs(options) do
        local info = opt:get_info()
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
| `find_by_name(name)` | `find_by_name(name)` | 按名称查找 | `name` - 下拉框标签 |
| `find_by_id(id)` | `find_by_id(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 下拉框操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开下拉框 |
| `collapse()` | `:collapse()` | 折叠下拉框 |
| `set_value(text)` | `:set_value(text)` | 设置文本（仅对支持文本设置的控件生效，实为 setText） |
| `get_value()` | `:get_value()` | 获取元素文本（通常是当前选中项文字） |
