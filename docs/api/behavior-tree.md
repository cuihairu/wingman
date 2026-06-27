# API: wingman.bt

行为树引擎，提供基础的 AI 决策系统。

> ⚠️ 当前仅实现 `create`/`tick`/`remove` 三个基础函数；sequence/selector/condition/action 等节点构造 API 规划中（见 [节点构造待实现](#节点构造待实现)）。

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

## 节点构造待实现

以下节点构造函数属于规划中的 API，当前**尚未实现**，调用将失败：

- **复合节点** - Sequence（序列）、Selector（选择）、Parallel（并行）
- **装饰节点** - Inverter（反转）、Repeat（重复）、Wait（等待）
- **叶子节点** - Condition（条件）、Action（动作）

> 当前可用的工作流：通过 `create` 创建空行为树后，使用 `tick` 执行并通过返回状态驱动逻辑；节点组装能力将在后续版本补齐。
