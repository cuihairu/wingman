# API: UIA Menu

菜单控件，用于组织命令和选项。

## 查找菜单

:::tabs

== Python

```python:line-numbers
from wingman import uia

menu = uia.find_by_name("文件")
if menu:
    print("找到菜单")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local menu = uia.findByName("文件")
if menu then
    print("找到菜单")
end
```

:::

---

## 展开菜单并选择

### 展开菜单并点击菜单项

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找并展开菜单
menu = uia.find_by_name("文件")
if menu:
    menu.expand()
    util.sleep(300)

    # 查找并点击菜单项
    new_item = uia.find_by_name("新建")
    if new_item:
        new_item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

-- 查找并展开菜单
local menu = uia.findByName("文件")
if menu then
    menu:expand()
    util.sleep(300)

    -- 查找并点击菜单项
    local newItem = uia.findByName("新建")
    if newItem then
        newItem:click()
    end
end
```

:::

### 直接点击菜单项（某些应用支持）

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 某些应用支持直接点击菜单项
save_item = uia.find_by_name("保存")
if save_item:
    save_item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 某些应用支持直接点击菜单项
local saveItem = uia.findByName("保存")
if saveItem then
    saveItem:click()
end
```

:::

---

## 右键菜单（上下文菜单）

### 打开并操作右键菜单

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 右键打开上下文菜单
input.right_click(100, 100)
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local input = require("wingman.input")
local util = require("wingman.util")

-- 右键打开上下文菜单
input.rightClick(100, 100)
util.sleep(300)

-- 操作菜单项
local copyItem = uia.findByName("复制")
if copyItem then
    copyItem:click()
end
```

:::

---

## 获取菜单项列表

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

menu = uia.find_by_name("文件")
if menu:
    # 展开菜单
    menu.expand()
    util.sleep(300)

    # 获取所有菜单项
    items = menu.get_children()
    for item in items:
        info = item.get_info()
        print(f"菜单项: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local menu = uia.findByName("文件")
if menu then
    -- 展开菜单
    menu:expand()
    util.sleep(300)

    -- 获取所有菜单项
    local items = menu:getChildren()
    for i, item in ipairs(items) do
        local info = item:getInfo()
        print("菜单项: " .. info.name)
    end
end
```

:::

---

## 可用接口

### 查找菜单

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找菜单 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 菜单操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开菜单 |
| `collapse()` | `:collapse()` | 折叠菜单 |
| `get_children()` | `:getChildren()` | 获取所有菜单项 |
| `click()` | `:click()` | 点击菜单项 |
