> [中文](behavior-tree.md) | **English**

# API: wingman.bt

Behavior tree engine, providing a basic AI decision system.

> [The three basic functions `create`/`tick`/`remove` are implemented, plus node construction (sequence/selector/parallel/inverter/repeat/wait/condition action) and assembly (addChild/setRoot), so a behavior tree can be fully built and ticked on the script side (see [Node Construction](#node-construction)).]

## Module Overview

The bt module provides basic execution management for behavior trees:
- **Execution management** - create, execute, and remove behavior trees

---

## Node States

Each node returns one of the following states after execution:

| State | Description |
|------|------|
| `SUCCESS` | Success |
| `FAILURE` | Failure |
| `RUNNING` | Running |

---

## Create Behavior Tree

### create(name) / create(name)

**Description**: Creates a new behavior tree.

**Function signature**:

```python
create(name: str) -> bool
```

```lua
create(name: string) -> boolean
```

**Parameters**:
- `name` - behavior tree name

**Returns**:
- Whether creation succeeded

:::tabs

== Python

```python:line-numbers
from wingman import bt

# 创建一个名为 "combat_tree" 的行为树
ok = bt.create("combat_tree")
if not ok:
    print("创建失败")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建一个名为 "combat_tree" 的行为树
local ok = wingman.bt.create("combat_tree")
if not ok then
    print("创建失败")
end
```

:::

---

## Execute Behavior Tree

### tick(tree_name) / tick(treeName)

**Description**: Executes the behavior tree once and returns the execution status.

**Function signature**:

```python
tick(tree_name: str) -> str
```

```lua
tick(treeName: string) -> string
```

**Parameters**:
- `tree_name` - behavior tree name

**Returns**:
- Execution status: `"SUCCESS"` / `"FAILURE"` / `"RUNNING"`

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

## Remove Behavior Tree

### remove(tree_name) / remove(treeName)

**Description**: Removes the specified behavior tree and releases its resources.

**Function signature**:

```python
remove(tree_name: str) -> None
```

```lua
remove(treeName: string) -> nil
```

**Parameters**:
- `tree_name` - behavior tree name

**Returns**:
- None

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

## Available Interfaces

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `create(name)` | `create(name)` | Create behavior tree | name: behavior tree name<br>Return: whether successful |
| `tick(tree_name)` | `tick(treeName)` | Execute behavior tree | tree_name: behavior tree name<br>Return: execution status |
| `remove(tree_name)` | `remove(treeName)` | Remove behavior tree | tree_name: behavior tree name |

---

## Node Construction

Behavior trees are composed of nodes. Construction functions return a node handle (int, ≥1 means valid, 0 means invalid), which are assembled with `addChild` and attached to a tree with `setRoot`.

### Composite Nodes

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `sequence(name?)` | `sequence(name?)` | Sequence: succeeds only when all child nodes succeed | name: node name (optional) → handle |
| `selector(name?)` | `selector(name?)` | Selector: succeeds when any child node succeeds | name → handle |
| `parallel(name?, policy?)` | `parallel(name?, policy?)` | Parallel | policy: SUCCEED_ON_ALL/SUCCEED_ON_ONE/FAIL_ON_ALL/FAIL_ON_ONE (default SUCCEED_ON_ALL) → handle |

### Decorator Nodes

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `inverter(child)` | `inverter(child)` | Inverts the child node's result | childHandle → handle |
| `repeat(child, count?)` | `repeat(child, count?)` | Repeatedly executes the child node | childHandle, count (default -1=infinite) → handle |

### Leaf Nodes

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `wait(ms)` | `wait(ms)` | Wait for the given number of milliseconds | milliseconds → handle |
| `condition(name, callback)` | `condition(name, callback)` | Condition: callback returns bool | name, callback(): bool → handle |
| `action(name, callback)` | `action(name, callback)` | Action: callback returns a status | name, callback(): "SUCCESS"/"FAILURE"/"RUNNING" → handle |

### Assembly

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `add_child(parent, child)` | `addChild(parent, child)` | Adds a child node to a composite node | parentHandle, childHandle → bool |
| `set_root(tree, node)` | `setRoot(tree, node)` | Sets the root node of a behavior tree | treeName, nodeHandle → bool |

> The condition/action callbacks are invoked when the behavior tree is ticked (no arguments). Lua callbacks must tick on the same thread that created the script (the Lua state is not thread-safe); Python callbacks are thread-safe (GIL).

### Example

```python
from wingman import bt

bt.create("combat")
cond = bt.condition("enemy_near", lambda: True)
act = bt.action("attack", lambda: "SUCCESS")
seq = bt.sequence()
bt.add_child(seq, cond)
bt.add_child(seq, act)
bt.set_root("combat", seq)
print(bt.tick("combat"))  # SUCCESS
```
