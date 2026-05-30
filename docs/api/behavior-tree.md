# API: wingman.behavior_tree

行为树引擎提供灵活的 AI 决策系统。

## 创建行为树

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree

behavior_tree.create("my_tree")
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")

bt.create("my_tree")
```

:::

## 创建节点

### 序列节点（Sequence）

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree

seq = behavior_tree.sequence("attack_sequence")
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")

local seq = bt.sequence("attack_sequence")
```

:::

### 选择节点（Selector）

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree

sel = behavior_tree.selector("task_selector")
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")

local sel = bt.selector("task_selector")
```

:::

### 条件节点（Condition）

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree, vision

cond = behavior_tree.condition("has_enemy", lambda: vision.find_image("enemy.png") is not None)
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")
local vision = require("wingman.vision")

local cond = bt.condition("has_enemy", function()
    local enemy = vision.findImage("enemy.png")
    return enemy ~= nil
end)
```

:::

### 动作节点（Action）

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree, input

act = behavior_tree.action("attack", lambda: (
    input.click(100, 200),
    "SUCCESS"
)[-1])
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")
local input = require("wingman.input")

local act = bt.action("attack", function()
    input.click(100, 200)
    return "SUCCESS"
end)
```

:::

## 执行行为树

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree

status = behavior_tree.tick("my_tree")
print(f"状态: {status}")  # SUCCESS/FAILURE/RUNNING
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")

local status = bt.tick("my_tree")
print("状态:", status)  -- SUCCESS/FAILURE/RUNNING
```

:::

## 移除行为树

:::tabs

== Python

```python:line-numbers
from wingman import behavior_tree

behavior_tree.remove("my_tree")
```

== Lua

```lua:line-numbers
local bt = require("wingman.behavior_tree")

bt.remove("my_tree")
```

:::

---

## 节点状态

每个节点执行后返回以下状态之一：

| 状态 | 说明 |
|------|------|
| `SUCCESS` | 成功 |
| `FAILURE` | 失败 |
| `RUNNING` | 运行中 |

---

## 节点类型

### Sequence（序列）

按顺序执行所有子节点，全部成功才返回成功。

```
┌─ Sequence ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Selector（选择）

按顺序执行子节点，任一成功即返回成功。

```
┌─ Selector ─┐
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
│   ┌─────┐  │
│   │ Node│  │
│   └─────┘  │
└────────────┘
```

### Parallel（并行）

同时执行所有子节点。

```
┌─ Parallel ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Inverter（反转）

反转子节点的结果（成功变失败，失败变成功）。

```
┌─ Inverter ─┐
│  ┌─────┐   │
│  │ Node│   │
│  └─────┘   │
└────────────┘
```

### Repeat（重复）

重复执行子节点 N 次。

```
┌─ Repeat(N) ─┐
│  ┌─────┐    │
│  │ Node│    │
│  └─────┘    │
└─────────────┘
```

### Wait（等待）

等待指定毫秒数。

```
┌─ Wait(ms) ─┐
└────────────┘
```

---

## 可用接口

### `create(name)`

创建一个新的行为树。

### `sequence(name?)`

创建序列节点。

### `selector(name?)`

创建选择节点。

### `condition(name, fn)`

创建条件节点。

### `action(name, fn)`

创建动作节点。

### `tick(tree_name)` / `tick(treeName)`

执行行为树一次。

### `remove(tree_name)`

移除行为树。
