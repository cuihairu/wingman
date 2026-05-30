# API: UIA List

列表（List）控件显示可选择的项目集合，常用于：
- 文件列表
- 用户列表
- 项目选择
- 数据表格

## 查找列表

**说明**：列表通常有名称或标题，可以通过名称查找。

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

# 查找名为"文件列表"的列表控件
list_box = uia.find_by_name("文件列表")
if list_box:
    print("找到列表控件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找名为"文件列表"的列表控件
local listBox = uia.findByName("文件列表")
if listBox then
    print("找到列表控件")
end
```

:::

---

## 遍历列表项

### 获取所有列表项

**说明**：使用 `get_children()` 获取列表中的所有项目。

**返回**：列表项元素数组

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    # 获取所有列表项
    items = list_box.get_children()
    print(f"共有 {len(items)} 个项目")

    # 遍历打印
    for i, item in enumerate(items):
        info = item.get_info()
        name = info.get('name', '(无名称)')
        print(f"  [{i}] {name}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    -- 获取所有列表项
    local items = listBox:getChildren()
    print("共有 " .. #items .. " 个项目")

    -- 遍历打印
    for i, item in ipairs(items) do
        local info = item:getInfo()
        local name = info.name or "(无名称)"
        print(string.format("  [%d] %s", i, name))
    end
end
```

:::

### 检查选中状态

**说明**：检查哪些项目被选中。列表可能支持单选或多选。

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("用户列表")
if list_box:
    items = list_box.get_children()

    # 查找选中的项目
    for item in items:
        info = item.get_info()
        name = info.get('name', '(无名称)')
        is_selected = info.get('is_selected', False)

        if is_selected:
            print(f"已选中: {name}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("用户列表")
if listBox then
    local items = listBox:getChildren()

    -- 查找选中的项目
    for i, item in ipairs(items) do
        local info = item:getInfo()
        local name = info.name or "(无名称)"
        local isSelected = info.isSelected

        if isSelected then
            print("已选中: " .. name)
        end
    end
end
```

:::

---

## 选择列表项

### 点击选择

**说明**：通过点击列表项来选中它。

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    items = list_box.get_children()

    # 点击第三个项目（索引从 0 开始）
    if len(items) > 2:
        items[2].click()
        print("已点击第三个项目")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    local items = listBox:getChildren()

    -- 点击第三个项目（索引从 1 开始）
    if #items > 2 then
        items[3]:click()
        print("已点击第三个项目")
    end
end
```

:::

### 按名称查找并选择

**说明**：直接按项目名称查找，然后点击选中。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接按名称查找列表项
item = uia.find_by_name("目标文件.txt")
if item:
    item.click()
    print("已选中：目标文件.txt")
else:
    print("未找到目标文件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接按名称查找列表项
local item = uia.findByName("目标文件.txt")
if item then
    item:click()
    print("已选中：目标文件.txt")
else
    print("未找到目标文件")
end
```

:::

---

## 双击列表项

**说明**：双击列表项通常用于打开或执行该项目的操作。

**使用场景**：
- 打开文件
- 查看详情
- 执行项目

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    items = list_box.get_children()

    # 双击打开项目
    if len(items) > 2:
        items[2].double_click()
        print("已双击打开第三个项目")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    local items = listBox:getChildren()

    -- 双击打开项目
    if #items > 2 then
        items[3]:doubleClick()
        print("已双击打开第三个项目")
    end
end
```

:::

---

## 可用接口

### 查找列表

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 | `name` - 列表名称 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 | `id` - AutomationId |

### 列表操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_children()` | `:getChildren()` | 获取所有列表项 |
| `click()` | `:click()` | 点击列表项 |
| `double_click()` | `:doubleClick()` | 双击列表项 |
| `select()` | `:select()` | 选中列表项 |
| `get_info()` | `:getInfo()` | 获取列表项信息 |
