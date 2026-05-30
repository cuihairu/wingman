# API: UIA Tab

标签页控件，用于组织多页内容。

## 查找标签控件

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    print("找到标签控件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    print("找到标签控件")
end
```

:::

---

## 切换标签页

### 直接点击标签页

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接按名称查找并点击标签页
tab_page = uia.find_by_name("高级")
if tab_page:
    tab_page.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接按名称查找并点击标签页
local tabPage = uia.findByName("高级")
if tabPage then
    tabPage:click()
end
```

:::

### 通过 Tab 控件切换

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    # 获取所有标签页
    tabs = tab.get_children()

    # 切换到第二个标签
    if len(tabs) > 1:
        tabs[1].click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    -- 获取所有标签页
    local tabs = tab:getChildren()

    -- 切换到第二个标签
    if #tabs > 1 then
        tabs[2]:click()
    end
end
```

:::

---

## 获取所有标签页

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()
    print(f"共有 {len(tabs)} 个标签页")

    for i, tab_page in enumerate(tabs):
        info = tab_page.get_info()
        print(f"[{i}] {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    local tabs = tab:getChildren()
    print("共有 " .. #tabs .. " 个标签页")

    for i, tabPage in ipairs(tabs) do
        local info = tabPage:getInfo()
        print(string.format("[%d] %s", i, info.name))
    end
end
```

:::

---

## 获取当前活动标签

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()

    for tab_page in tabs:
        info = tab_page.get_info()
        # 检查是否被选中
        if info.get('selection_state', 0) == 1:
            print(f"当前活动标签: {info['name']}")
            break
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    local tabs = tab:getChildren()

    for i, tabPage in ipairs(tabs) do
        local info = tabPage:getInfo()
        -- 检查是否被选中
        if info.selectionState == 1 then
            print("当前活动标签: " .. info.name)
            break
        end
    end
end
```

:::

---

## 可用接口

### 查找标签

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找标签控件 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 标签操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `get_children()` | `:getChildren()` | 获取所有标签页 |
| `click()` | `:click()` | 切换到该标签页 |
