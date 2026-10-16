> [中文](tab.md) | **English**

# API: UIA Tab

The Tab control organizes multiple pages of content. It is commonly seen in settings windows, category browsing, and similar scenarios.

## Finding a tab control

**Note**: Tab controls usually have names and can be found by name.

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
local wingman = require("wingman")

local tab = wingman.uia.find_by_name("设置")
if tab then
    print("找到标签控件")
end
```

:::

---

## Switching tabs

### Click the tab directly

**Note**: The simplest way is to find the target tab by name and click it directly.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接查找并点击"高级"标签页
tab_page = uia.find_by_name("高级")
if tab_page:
    tab_page["click"]()
    print("已切换到高级标签页")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 直接查找并点击"高级"标签页
local tabPage = wingman.uia.find_by_name("高级")
if tabPage then
    tabPage:click()
    print("已切换到高级标签页")
end
```

:::

### Getting child tabs via the Tab control

**Note**: First find the Tab control, then get all of its children (the tab pages), and click the target.

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    # 获取所有标签页
    tabs = tab["get_children"]()
    print(f"共有 {len(tabs)} 个标签页")

    # 切换到第二个标签（索引从 0 开始）
    if len(tabs) > 1:
        tabs[1]["click"]()
        print("已切换到第二个标签页")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tab = wingman.uia.find_by_name("设置")
if tab then
    -- 获取所有标签页
    local tabs = tab:get_children()
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

## Getting all tabs

**Note**: Iterate over all children of the Tab control and list the names of all tabs.

:::tabs

== Python

```python:line-numbers
from wingman import uia

tab = uia.find_by_name("设置")
if tab:
    tabs = tab["get_children"]()

    print(f"共有 {len(tabs)} 个标签页：")
    for i, tab_page in enumerate(tabs):
        info = tab_page["get_info"]()
        name = info.get('name', '(无名称)')
        print(f"  [{i}] {name}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tab = wingman.uia.find_by_name("设置")
if tab then
    local tabs = tab:get_children()

    print("共有 " .. #tabs .. " 个标签页：")
    for i, tabPage in ipairs(tabs) do
        local info = tabPage:get_info()
        local name = info.name or "(无名称)"
        print(string.format("  [%d] %s", i, name))
    end
end
```

:::

---

## Getting the active tab

Calling `get_selection()` on a **tab page container** reads the currently active tab item (returns the first selected child; returns `None`/`nil` when nothing is selected):

:::tabs

== Python

```python:line-numbers
from wingman import uia

tabs = uia.find_tab("设置")
if tabs:
    active = tabs["get_selection"]()
    if active:
        print(f"当前标签页: {active['get_info']()['name']}")
    else:
        print("当前无激活标签页")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tabs = wingman.uia.find_tab("设置")
if tabs then
    local active = tabs:get_selection()
    if active then
        print("当前标签页: " .. active:get_info().name)
    else
        print("当前无激活标签页")
    end
end
```

:::

> `get_info()` return keys still have no `selection_state` field; the tab page container must support the Selection pattern (Windows) / expose readable AXSelectedRows/Children (macOS). You can also use the dedicated finder `uia.find_tab("名称")` (shipped 2026-10-09, Tab role).

---

## Available Interfaces

| Python function | Lua function | Description |
|----------------|-------------|-------------|
| `find_tab(name)` | `find_tab(name)` | Find the tab page container by name (Tab role, shipped 2026-10-09) |
| `find_tab_item(name)` | `find_tab_item(name)` | Find the tab item by name (TabItem role, shipped 2026-10-09) |
| `find_by_name(name)` | `find_by_name(name)` | Find by name (substring match; may hit any type) |
| `get_selection()` | `:get_selection()` | Read the currently active tab item (container) |
| `find_by_id(id)` | `find_by_id(id)` | Find by AutomationId |
| `get_children()` | `:get_children()` | Get all tabs |
| `click()` | `:click()` | Switch to that tab |
