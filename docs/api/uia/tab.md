# API: UIA Tab

标签页（Tab）控件用于组织多页内容，常见于设置界面、分类浏览等场景。

## 查找标签控件

**说明**：标签控件通常有名称，可以通过名称查找。

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

**说明**：最简单的方式是直接按名称查找并点击目标标签页。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接查找并点击"高级"标签页
tab_page = uia.find_by_name("高级")
if tab_page:
    tab_page.click()
    print("已切换到高级标签页")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接查找并点击"高级"标签页
local tabPage = uia.findByName("高级")
if tabPage then
    tabPage:click()
    print("已切换到高级标签页")
end
```

:::

### 通过 Tab 控件获取子标签

**说明**：先找到 Tab 控件，然后获取其所有子元素（标签页），再点击目标。

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    # 获取所有标签页
    tabs = tab.get_children()
    print(f"共有 {len(tabs)} 个标签页")

    # 切换到第二个标签（索引从 0 开始）
    if len(tabs) > 1:
        tabs[1].click()
        print("已切换到第二个标签页")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    -- 获取所有标签页
    local tabs = tab:getChildren()
    print("共有 " .. #tabs .. " 个标签页")

    -- 切换到第二个标签（索引从 1 开始）
    if #tabs > 1 then
        tabs[2]:click()
        print("已切换到第二个标签页")
    end
end
```

:::

---

## 获取所有标签页

**说明**：遍历 Tab 控件的所有子元素，列出所有标签页的名称。

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()

    print(f"共有 {len(tabs)} 个标签页：")
    for i, tab_page in enumerate(tabs):
        info = tab_page.get_info()
        name = info.get('name', '(无名称)')
        print(f"  [{i}] {name}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tab = uia.findByName("设置")
if tab then
    local tabs = tab:getChildren()

    print("共有 " .. #tabs .. " 个标签页：")
    for i, tabPage in ipairs(tabs) do
        local info = tabPage:getInfo()
        local name = info.name or "(无名称)"
        print(string.format("  [%d] %s", i, name))
    end
end
```

:::

---

## 获取当前活动标签

**说明**：找出当前被激活的标签页。

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab.get_children()

    for tab_page in tabs:
        info = tab_page.get_info()
        # selection_state 为 1 表示选中
        if info.get('selection_state', 0) == 1:
            print(f"当前活动标签: {info.get('name', '')}")
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
        -- selectionState 为 1 表示选中
        if info.selectionState == 1 then
            print("当前活动标签: " .. (info.name or ""))
            break
        end
    end
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |
| `get_children()` | `:getChildren()` | 获取所有标签页 |
| `click()` | `:click()` | 切换到该标签页 |
