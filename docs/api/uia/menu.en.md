> [中文](menu.md) | **English**

# API: UIA Menu

The Menu control organizes commands and options. It is commonly seen in:
- Application top menu bars (File, Edit, View, ...)
- Right-click context menus
- Dropdown menus

## Finding a menu

**Note**: Menus usually have names (such as "文件" or "编辑") and can be found by name.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"文件"的菜单
menu = uia.find_by_name("文件")
if menu:
    print("找到菜单")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找名为"文件"的菜单
local menu = wingman.uia.find_by_name("文件")
if menu then
    print("找到菜单")
end
```

:::

---

## Expanding a menu and selecting

### Expand the menu and click a menu item

**Note**: First expand the menu, then find and click the target menu item.

**Steps**:
1. Find the menu control
2. Use `expand()` to expand the menu
3. Wait for the menu items to appear
4. Find and click the target menu item

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找并展开"文件"菜单
menu = uia.find_by_name("文件")
if menu:
    # 展开菜单
    menu["expand"]()
    print("已展开文件菜单")

    # 等待菜单项出现
    util.sleep(300)

    # 查找并点击"新建"菜单项
    new_item = uia.find_by_name("新建")
    if new_item:
        new_item["click"]()
        print("已点击新建")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找并展开"文件"菜单
local menu = wingman.uia.find_by_name("文件")
if menu then
    -- 展开菜单
    menu:expand()
    print("已展开文件菜单")

    -- 等待菜单项出现
    wingman.util.sleep(300)

    -- 查找并点击"新建"菜单项
    local newItem = wingman.uia.find_by_name("新建")
    if newItem then
        newItem:click()
        print("已点击新建")
    end
end
```

:::

### Clicking a menu item directly

**Note**: Some applications support clicking menu items directly without expanding the menu first.

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 某些应用支持直接点击菜单项
save_item = uia.find_by_name("保存")
if save_item:
    save_item["click"]()
    print("已点击保存")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 某些应用支持直接点击菜单项
local saveItem = wingman.uia.find_by_name("保存")
if saveItem then
    saveItem:click()
    print("已点击保存")
end
```

:::

---

## Right-click menu (context menu)

**Note**: The right-click menu is a context menu triggered by the right mouse button.

**Steps**:
1. Right-click the target location
2. Wait for the menu to appear
3. Find and click a menu item

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 在指定位置右键（第 3 参 button=2 为右键）
input.click(100, 100, 2)
print("已右键")

# 等待上下文菜单出现
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item["click"]()
    print("已点击复制")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 在指定位置右键（第 3 参 button=2 为右键）
wingman.input.click(100, 100, 2)
print("已右键")

-- 等待上下文菜单出现
wingman.util.sleep(300)

-- 操作菜单项
local copyItem = wingman.uia.find_by_name("复制")
if copyItem then
    copyItem:click()
    print("已点击复制")
end
```

:::

---

## Getting the menu item list

**Note**: After expanding the menu, you can iterate over all menu items.

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

menu = uia.find_by_name("文件")
if menu:
    # 展开菜单
    menu["expand"]()
    util.sleep(300)

    # 获取所有菜单项
    items = menu["get_children"]()

    print(f"文件菜单共有 {len(items)} 个项目：")
    for item in items:
        info = item["get_info"]()
        name = info.get('name', '(无名称)')
        print(f"  - {name}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local menu = wingman.uia.find_by_name("文件")
if menu then
    -- 展开菜单
    menu:expand()
    wingman.util.sleep(300)

    -- 获取所有菜单项
    local items = menu:get_children()

    print("文件菜单共有 " .. #items .. " 个项目：")
    for i, item in ipairs(items) do
        local info = item:get_info()
        local name = info.name or "(无名称)"
        print("  - " .. name)
    end
end
```

:::

---

## Available Interfaces

| Python method | Lua method | Description |
|----------------|-------------|-------------|
| `expand()` | `:expand()` | Expand the menu |
| `collapse()` | `:collapse()` | Collapse the menu |
| `get_children()` | `:get_children()` | Get all menu items |
| `click()` | `:click()` | Click a menu item |
