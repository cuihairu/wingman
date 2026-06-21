# API: wingman.behavior_tree

行为树引擎，提供灵活的 AI 决策系统。

## 模块概述

behavior_tree 模块提供行为树的创建和执行功能：
- **复合节点** - Sequence（序列）、Selector（选择）、Parallel（并行）
- **装饰节点** - Inverter（反转）、Repeat（重复）
- **叶子节点** - Condition（条件）、Action（动作）
- **执行管理** - 创建、执行、移除行为树

---

## 节点状态

每个节点执行后返回以下状态之一：

| 状态 | 说明 |
|------|------|
| `SUCCESS` | 成功 |
| `FAILURE` | 失败 |
| `RUNNING` | 运行中 |

---

## 创建行为树

### create(name) / create(name)

**说明**：创建一个新的行为树。

**函数签名**：

```python
create(name: str) -> None
```

```lua
create(name: string) -> nil
```

**参数**：
- `name` - 行为树名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import bt

# 创建一个名为 "combat_tree" 的行为树
bt.create("combat_tree")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建一个名为 "combat_tree" 的行为树
wingman.bt.create("combat_tree")
```

:::

---

## 创建序列节点

### sequence(name?) / sequence(name?)

**说明**：创建序列节点，按顺序执行所有子节点，全部成功才返回成功。

**函数签名**：

```python
sequence(name: str = "") -> Node
```

```lua
sequence(name: string = "") -> Node
```

**参数**：
- `name` - 可选，节点名称

**返回**：
- `Node` 对象

**执行逻辑**：
```
Sequence: 按顺序执行子节点
┌─ Sequence ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │  SUCCESS
│  ┌─────┐   │
│  │ Node│   │  SUCCESS
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │  SUCCESS
│  └─────┘   │
└────────────┘
  → SUCCESS (全部成功)
```

:::tabs

== Python

```python:line-numbers
from wingman import bt

# 创建序列节点
seq = bt.sequence("attack_sequence")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建序列节点
local seq = wingman.bt.sequence("attack_sequence")
```

:::

---

## 创建选择节点

### selector(name?) / selector(name?)

**说明**：创建选择节点，按顺序执行子节点，任一成功即返回成功。

**函数签名**：

```python
selector(name: str = "") -> Node
```

```lua
selector(name: string = "") -> Node
```

**参数**：
- `name` - 可选，节点名称

**返回**：
- `Node` 对象

**执行逻辑**：
```
Selector: 按顺序执行子节点
┌─ Selector ─┐
│   ┌─────┐  │
│   │ Node│  │  FAILURE
│   └─────┘  │
│   ┌─────┐  │
│   │ Node│  │  SUCCESS
│   └─────┘  │
└────────────┘
  → SUCCESS (任一成功)
```

:::tabs

== Python

```python:line-numbers
from wingman import bt

# 创建选择节点
sel = bt.selector("task_selector")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建选择节点
local sel = wingman.bt.selector("task_selector")
```

:::

---

## 创建条件节点

### condition(name, fn) / condition(name, fn)

**说明**：创建条件节点，执行函数返回状态。

**函数签名**：

```python
condition(name: str, fn: Callable[[], str]) -> Node
```

```lua
condition(name: string, fn: fun(): string) -> Node
```

**参数**：
- `name` - 节点名称
- `fn` - 条件函数，返回 `"SUCCESS"`/`"FAILURE"`/`"RUNNING"`

**返回**：
- `Node` 对象

:::tabs

== Python

```python:line-numbers
from wingman import bt, vision

cond = bt.condition("has_enemy", lambda: (
    "SUCCESS" if vision.find_image("enemy.png") else "FAILURE"
))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local cond = wingman.bt.condition("has_enemy", function()
    local enemy = wingman.vision.findImage("enemy.png")
    return enemy and "SUCCESS" or "FAILURE"
end)
```

:::

---

## 创建动作节点

### action(name, fn) / action(name, fn)

**说明**：创建动作节点，执行函数返回状态。

**函数签名**：

```python
action(name: str, fn: Callable[[], str]) -> Node
```

```lua
action(name: string, fn: fun(): string) -> Node
```

**参数**：
- `name` - 节点名称
- `fn` - 动作函数，返回 `"SUCCESS"`/`"FAILURE"`/`"RUNNING"`

**返回**：
- `Node` 对象

:::tabs

== Python

```python:line-numbers
from wingman import bt, input

act = bt.action("attack", lambda: (
    input.click(100, 200),
    "SUCCESS"
)[-1])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local act = wingman.bt.action("attack", function()
    wingman.input.click(100, 200)
    return "SUCCESS"
end)
```

:::

---

## 执行行为树

### tick(tree_name) / tick(treeName)

**说明**：执行行为树一次，返回执行状态。

**函数签名**：

```python
tick(tree_name: str) -> str
```

```lua
tick(treeName: string) -> string
```

**参数**：
- `tree_name` - 行为树名称

**返回**：
- 执行状态：`"SUCCESS"` / `"FAILURE"` / `"RUNNING"`

:::tabs

== Python

```python:line-numbers
from wingman import bt

status = bt.tick("combat_tree")
print(f"状态: {status}")  # SUCCESS/FAILURE/RUNNING
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local status = wingman.bt.tick("combat_tree")
print("状态:", status)  -- SUCCESS/FAILURE/RUNNING
```

:::

---

## 移除行为树

### remove(tree_name) / remove(treeName)

**说明**：移除指定的行为树，释放资源。

**函数签名**：

```python
remove(tree_name: str) -> None
```

```lua
remove(treeName: string) -> nil
```

**参数**：
- `tree_name` - 行为树名称

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import bt

# 移除行为树
bt.remove("combat_tree")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 移除行为树
wingman.bt.remove("combat_tree")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `create(name)` | `create(name)` | 创建行为树 | name: 行为树名称 |
| `sequence(name?)` | `sequence(name?)` | 创建序列节点 | name: 节点名称(可选)<br>返回: Node 对象 |
| `selector(name?)` | `selector(name?)` | 创建选择节点 | name: 节点名称(可选)<br>返回: Node 对象 |
| `condition(name, fn)` | `condition(name, fn)` | 创建条件节点 | name: 节点名称<br>fn: 条件函数<br>返回: Node 对象 |
| `action(name, fn)` | `action(name, fn)` | 创建动作节点 | name: 节点名称<br>fn: 动作函数<br>返回: Node 对象 |
| `tick(tree_name)` | `tick(treeName)` | 执行行为树 | tree_name: 行为树名称<br>返回: 执行状态 |
| `remove(tree_name)` | `remove(treeName)` | 移除行为树 | tree_name: 行为树名称 |

---

## 节点类型说明

### Sequence（序列）
按顺序执行所有子节点，全部成功才返回成功。

### Selector（选择）
按顺序执行子节点，任一成功即返回成功。

### Parallel（并行）
同时执行所有子节点。

### Inverter（反转）
反转子节点的结果（成功变失败，失败变成功）。

### Repeat（重复）
重复执行子节点 N 次。

### Wait（等待）
等待指定毫秒数。
