# API: UIA Tree

树形控件，用于显示层次结构数据。

## 查找树控件

:::tabs

== Python

```python:line-numbers
from wingman import uia

tree = uia.find_by_name("文件夹树")
if tree:
    print("找到树控件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tree = uia.findByName("文件夹树")
if tree then
    print("找到树控件")
end
```

:::

---

## 展开/折叠节点

### 展开节点

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

tree = uia.find_by_name("文件夹树")
if tree:
    # 查找并展开节点
    folder = uia.find_by_name("文档")
    if folder:
        folder.expand()
        util.sleep(200)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local tree = uia.findByName("文件夹树")
if tree then
    -- 查找并展开节点
    local folder = uia.findByName("文档")
    if folder then
        folder:expand()
        util.sleep(200)
    end
end
```

:::

### 折叠节点

:::tabs

== Python

```python:line-numbers
from wingman import uia

folder = uia.find_by_name("文档")
if folder:
    folder.collapse()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local folder = uia.findByName("文档")
if folder then
    folder:collapse()
end
```

:::

### 双击切换展开/折叠

:::tabs

== Python

```python:line-numbers
from wingman import uia

folder = uia.find_by_name("文档")
if folder:
    # 双击切换展开/折叠状态
    folder.double_click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local folder = uia.findByName("文档")
if folder then
    -- 双击切换展开/折叠状态
    folder:doubleClick()
end
```

:::

---

## 遍历树节点

### 递归遍历整个树

:::tabs

== Python

```python:line-numbers
from wingman import uia

def traverse_tree(element, depth=0):
    """递归遍历树节点"""
    indent = "  " * depth
    info = element.get_info()
    print(f"{indent}{'└─' if depth > 0 else ''}{info['name']}")

    # 递归处理子节点
    children = element.get_children()
    for child in children:
        traverse_tree(child, depth + 1)

tree = uia.find_by_name("文件夹树")
if tree:
    traverse_tree(tree)
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local function traverseTree(element, depth)
    depth = depth or 0
    local indent = string.rep("  ", depth)
    local info = element:getInfo()
    local prefix = depth > 0 and "└─" or ""
    print(indent .. prefix .. info.name)

    -- 递归处理子节点
    local children = element:getChildren()
    for i, child in ipairs(children) do
        traverseTree(child, depth + 1)
    end
end

local tree = uia.findByName("文件夹树")
if tree then
    traverseTree(tree)
end
```

:::

### 获取直接子节点

:::tabs

== Python

```python:line-numbers
from wingman import uia

tree = uia.find_by_name("文件夹树")
if tree:
    # 获取直接子节点（不递归）
    children = tree.get_children()
    for child in children:
        info = child.get_info()
        print(f"子节点: {info['name']}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tree = uia.findByName("文件夹树")
if tree then
    -- 获取直接子节点（不递归）
    local children = tree:getChildren()
    for i, child in ipairs(children) do
        local info = child:getInfo()
        print("子节点: " .. info.name)
    end
end
```

:::

---

## 查找并选择节点

### 按名称查找节点

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接按名称查找树节点
node = uia.find_by_name("目标文件")
if node:
    # 可能需要先展开父节点才能找到
    node.click()
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接按名称查找树节点
local node = uia.findByName("目标文件")
if node then
    -- 可能需要先展开父节点才能找到
    node:click()
end
```

:::

---

## 可用接口

### 查找树控件

| Python 函数 | Lua 函数 | 说明 |
|------------|---------|------|
| `find_by_name(name)` | `findByName(name)` | 按名称查找树控件 |
| `find_by_id(id)` | `findById(id)` | 按 AutomationId 查找 |

### 树节点操作

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开节点 |
| `collapse()` | `:collapse()` | 折叠节点 |
| `is_expanded()` | `:isExpanded()` | 检查是否已展开 |
| `get_children()` | `:getChildren()` | 获取子节点 |
| `click()` | `:click()` | 点击节点 |
