# API: UIA List

列表控件，显示可选择的项目集合。

## 查找列表

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    print("找到列表控件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    print("找到列表控件")
end
```

:::

---

## 遍历列表项

### 获取所有列表项

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    items = list_box.get_children()
    print(f"共有 {len(items)} 个项目")

    for i, item in enumerate(items):
        info = item.get_info()
        print(f"[{i}] {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    local items = listBox:getChildren()
    print("共有 " .. #items .. " 个项目")

    for i, item in ipairs(items) do
        local info = item:getInfo()
        print(string.format("[%d] %s", i, info.name))
    end
end
```

:::

### 检查选中状态

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("用户列表")
if list_box:
    items = list_box.get_children()

    for item in items:
        info = item.get_info()
        if info.get('is_selected', False):
            print(f"已选中: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("用户列表")
if listBox then
    local items = listBox:getChildren()

    for i, item in ipairs(items) do
        local info = item:getInfo()
        if info.isSelected then
            print("已选中: " .. info.name)
        end
    end
end
```

:::

---

## 选择列表项

### 点击选择

:::tabs

== Python

```python:line-numbers
from wingman import uia

list_box = uia.find_by_name("文件列表")
if list_box:
    items = list_box.get_children()

    # 点击第三个项目
    if len(items) > 2:
        items[2].click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local listBox = uia.findByName("文件列表")
if listBox then
    local items = listBox:getChildren()

    -- 点击第三个项目
    if #items > 2 then
        items[3]:click()
    end
end
```

:::

### 按名称选择项目

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接按名称查找列表项
item = uia.find_by_name("目标文件.txt")
if item:
    item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接按名称查找列表项
local item = uia.findByName("目标文件.txt")
if item then
    item:click()
end
```

:::

---

## 双击列表项

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
    end
end
```

:::

---

## 可用接口

### 查找列表

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找列表 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 列表操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_children()` | `:getChildren()` | 获取所有列表项 |
| `click()` | `:click()` | 点击列表项 |
| `double_click()` | `:doubleClick()` | 双击列表项 |
| `select()` | `:select()` | 选中列表项 |
