# API: UIA Menu

菜单（Menu）控件用于组织命令和选项，常见于：
- 应用程序顶部菜单栏（文件、编辑、查看...）
- 右键上下文菜单
- 下拉菜单

## 查找菜单

**说明**：菜单通常有名称（如"文件"、"编辑"），可以通过名称查找。

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
local menu = wingman.uia.findByName("文件")
if menu then
    print("找到菜单")
end
```

:::

---

## 展开菜单并选择

### 展开菜单并点击菜单项

**说明**：先展开菜单，然后查找并点击目标菜单项。

**步骤**：
1. 查找菜单控件
2. 使用 `expand()` 展开菜单
3. 等待菜单项出现
4. 查找并点击目标菜单项

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

# 查找并展开"文件"菜单
menu = uia.find_by_name("文件")
if menu:
    # 展开菜单
    menu.expand()
    print("已展开文件菜单")

    # 等待菜单项出现
    util.sleep(300)

    # 查找并点击"新建"菜单项
    new_item = uia.find_by_name("新建")
    if new_item:
        new_item.click()
        print("已点击新建")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找并展开"文件"菜单
local menu = wingman.uia.findByName("文件")
if menu then
    -- 展开菜单
    menu:expand()
    print("已展开文件菜单")

    -- 等待菜单项出现
    wingman.util.sleep(300)

    -- 查找并点击"新建"菜单项
    local newItem = wingman.uia.findByName("新建")
    if newItem then
        newItem:click()
        print("已点击新建")
    end
end
```

:::

### 直接点击菜单项

**说明**：某些应用程序支持直接点击菜单项，无需先展开菜单。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 某些应用支持直接点击菜单项
save_item = uia.find_by_name("保存")
if save_item:
    save_item.click()
    print("已点击保存")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 某些应用支持直接点击菜单项
local saveItem = wingman.uia.findByName("保存")
if saveItem then
    saveItem:click()
    print("已点击保存")
end
```

:::

---

## 右键菜单（上下文菜单）

**说明**：右键菜单是通过鼠标右键触发的上下文菜单。

**操作步骤**：
1. 右键点击目标位置
2. 等待菜单出现
3. 查找并点击菜单项

:::tabs

== Python

```python:line-numbers
from wingman import uia, input, util

# 在指定位置右键
input.right_click(100, 100)
print("已右键")

# 等待上下文菜单出现
util.sleep(300)

# 操作菜单项
copy_item = uia.find_by_name("复制")
if copy_item:
    copy_item.click()
    print("已点击复制")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 在指定位置右键
wingman.input.rightClick(100, 100)
print("已右键")

-- 等待上下文菜单出现
wingman.util.sleep(300)

-- 操作菜单项
local copyItem = wingman.uia.findByName("复制")
if copyItem then
    copyItem:click()
    print("已点击复制")
end
```

:::

---

## 获取菜单项列表

**说明**：展开菜单后，可以遍历所有菜单项。

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

    print(f"文件菜单共有 {len(items)} 个项目：")
    for item in items:
        info = item.get_info()
        name = info.get('name', '(无名称)')
        print(f"  - {name}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local menu = wingman.uia.findByName("文件")
if menu then
    -- 展开菜单
    menu:expand()
    wingman.util.sleep(300)

    -- 获取所有菜单项
    local items = menu:getChildren()

    print("文件菜单共有 " .. #items .. " 个项目：")
    for i, item in ipairs(items) do
        local info = item:getInfo()
        local name = info.name or "(无名称)"
        print("  - " .. name)
    end
end
```

:::

---

## 可用接口

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开菜单 |
| `collapse()` | `:collapse()` | 折叠菜单 |
| `get_children()` | `:getChildren()` | 获取所有菜单项 |
| `click()` | `:click()` | 点击菜单项 |
