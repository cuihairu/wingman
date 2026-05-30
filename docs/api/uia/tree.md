# API: UIA Tree

树形控件（Tree）用于显示层次结构数据，常见于：
- 文件夹树
- 组织架构图
- 分类目录
- 注册表编辑器

## 查找树控件

**说明**：树控件通常有名称，可以通过名称查找。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 查找名为"文件夹树"的树控件
tree = uia.find_by_name("文件夹树")
if tree:
    print("找到树控件")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 查找名为"文件夹树"的树控件
local tree = uia.findByName("文件夹树")
if tree then
    print("找到树控件")
end
```

:::

---

## 展开/折叠节点

### 展开节点

**说明**：展开树节点以显示其子节点。

:::tabs

== Python

```python:line-numbers
from wingman import uia, util

tree = uia.find_by_name("文件夹树")
if tree:
    # 查找并展开"文档"节点
    folder = uia.find_by_name("文档")
    if folder:
        folder.expand()
        print("已展开文档节点")
        util.sleep(200)  # 等待子节点加载
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")
local util = require("wingman.util")

local tree = uia.findByName("文件夹树")
if tree then
    -- 查找并展开"文档"节点
    local folder = uia.findByName("文档")
    if folder then
        folder:expand()
        print("已展开文档节点")
        util.sleep(200)  -- 等待子节点加载
    end
end
```

:::

### 折叠节点

**说明**：折叠树节点以隐藏其子节点。

:::tabs

== Python

```python:line-numbers
from wingman import uia

folder = uia.find_by_name("文档")
if folder:
    folder.collapse()
    print("已折叠文档节点")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local folder = uia.findByName("文档")
if folder then
    folder:collapse()
    print("已折叠文档节点")
end
```

:::

### 双击切换

**说明**：双击树节点可以切换展开/折叠状态。

:::tabs

== Python

```python:line-numbers
from wingman import uia

folder = uia.find_by_name("文档")
if folder:
    # 双击切换展开/折叠
    folder.double_click()
    print("已双击切换状态")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local folder = uia.findByName("文档")
if folder then
    -- 双击切换展开/折叠
    folder:doubleClick()
    print("已双击切换状态")
end
```

:::

---

## 遍历树节点

### 递归遍历整个树

**说明**：递归遍历树的所有节点，打印完整的树结构。

:::tabs

== Python

```python:line-numbers
from wingman import uia

def traverse_tree(element, depth=0):
    """递归遍历树节点"""
    indent = "  " * depth
    info = element.get_info()
    name = info.get('name', '(无名称)')
    prefix = "└─" if depth > 0 else ""

    print(f"{indent}{prefix}{name}")

    # 递归处理子节点
    children = element.get_children()
    for child in children:
        traverse_tree(child, depth + 1)

# 使用
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
    local name = info.name or "(无名称)"
    local prefix = depth > 0 and "└─" or ""

    print(indent .. prefix .. name)

    -- 递归处理子节点
    local children = element:getChildren()
    for i, child in ipairs(children) do
        traverseTree(child, depth + 1)
    end
end

-- 使用
local tree = uia.findByName("文件夹树")
if tree then
    traverseTree(tree)
end
```

:::

### 获取直接子节点

**说明**：只获取树的第一层子节点，不递归。

:::tabs

== Python

```python:line-numbers
from wingman import uia

tree = uia.find_by_name("文件夹树")
if tree:
    # 获取直接子节点
    children = tree.get_children()

    print(f"根节点共有 {len(children)} 个子节点：")
    for child in children:
        info = child.get_info()
        print(f"  - {info.get('name', '(无名称)')}")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

local tree = uia.findByName("文件夹树")
if tree then
    -- 获取直接子节点
    local children = tree:getChildren()

    print("根节点共有 " .. #children .. " 个子节点：")
    for i, child in ipairs(children) do
        local info = child:getInfo()
        print("  - " .. (info.name or "(无名称)"))
    end
end
```

:::

---

## 查找并选择节点

**说明**：按名称查找树节点，然后点击选中。

:::tabs

== Python

```python:line-numbers
from wingman import uia

# 直接按名称查找树节点
node = uia.find_by_name("目标文件.txt")
if node:
    # 可能需要先展开父节点才能找到
    node.click()
    print("已选中：目标文件.txt")
```

== Lua

```lua:line-numbers
local uia = require("wingman.uia")

-- 直接按名称查找树节点
local node = uia.findByName("目标文件.txt")
if node then
    -- 可能需要先展开父节点才能找到
    node:click()
    print("已选中：目标文件.txt")
end
```

:::

---

## 可用接口

| Python 方法 | Lua 方法 | 说明 |
|------------|---------|------|
| `expand()` | `:expand()` | 展开节点 |
| `collapse()` | `:collapse()` | 折叠节点 |
| `is_expanded()` | `:isExpanded()` | 检查是否已展开 |
| `get_children()` | `:getChildren()` | 获取子节点 |
| `click()` | `:click()` | 点击节点 |
| `double_click()` | `:doubleClick()` | 双击切换展开/折叠 |
