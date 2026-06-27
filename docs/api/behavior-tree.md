# API: wingman.bt

行为树引擎，提供基础的 AI 决策系统。

> ✅ 已实现 `create`/`tick`/`remove` 三基础函数 + 节点构造（sequence/selector/parallel/inverter/repeat/wait/condition/action）+ 组装（addChild/setRoot），可在脚本端完整构建并 tick 行为树（见 [节点构造](#节点构造)）。

## 模块概述

bt 模块提供行为树的基础执行管理：
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
create(name: str) -> bool
```

```lua
create(name: string) -> boolean
```

**参数**：
- `name` - 行为树名称

**返回**：
- 是否创建成功

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
| `create(name)` | `create(name)` | 创建行为树 | name: 行为树名称<br>返回: 是否成功 |
| `tick(tree_name)` | `tick(treeName)` | 执行行为树 | tree_name: 行为树名称<br>返回: 执行状态 |
| `remove(tree_name)` | `remove(treeName)` | 移除行为树 | tree_name: 行为树名称 |

---

## 节点构造

行为树由节点组成。构造函数返回节点句柄（int，≥1 有效，0=无效），用 `addChild` 组装、`setRoot` 挂到树。

### 复合节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `sequence(name?)` | `sequence(name?)` | 序列：所有子节点成功才成功 | name: 节点名(可选) → handle |
| `selector(name?)` | `selector(name?)` | 选择：任一子节点成功则成功 | name → handle |
| `parallel(name?, policy?)` | `parallel(name?, policy?)` | 并行 | policy: SUCCEED_ON_ALL/SUCCEED_ON_ONE/FAIL_ON_ALL/FAIL_ON_ONE(默认 SUCCEED_ON_ALL) → handle |

### 装饰节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `inverter(child)` | `inverter(child)` | 反转子节点结果 | childHandle → handle |
| `repeat(child, count?)` | `repeat(child, count?)` | 重复执行子节点 | childHandle, count(默认 -1=无限) → handle |

### 叶子节点

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `wait(ms)` | `wait(ms)` | 等待指定毫秒 | milliseconds → handle |
| `condition(name, callback)` | `condition(name, callback)` | 条件：回调返回 bool | name, callback(): bool → handle |
| `action(name, callback)` | `action(name, callback)` | 动作：回调返回状态 | name, callback(): "SUCCESS"/"FAILURE"/"RUNNING" → handle |

### 组装

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `add_child(parent, child)` | `addChild(parent, child)` | 把子节点加到复合节点 | parentHandle, childHandle → bool |
| `set_root(tree, node)` | `setRoot(tree, node)` | 设置行为树根节点 | treeName, nodeHandle → bool |

> condition/action 的 callback 在行为树 tick 时被回调（无参）。Lua 回调需在创建脚本的同一线程 tick（Lua state 非线程安全）；Python 回调线程安全（GIL）。

### 示例

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
