# API: wingman.fsm

状态机模块，提供事件驱动的状态机功能，用于管理游戏状态、流程控制等场景。

## 模块概述

fsm 模块提供状态机的创建和管理功能：
- **创建状态机** - 创建命名状态机并指定初始状态
- **定义状态** - 定义状态及进入/离开回调
- **定义转移** - 定义状态转移规则，支持守卫函数
- **派发事件** - 触发状态转移
- **查询状态** - 获取当前状态、检查状态

---

## 创建状态机

### create(name, initial) / create(name, initial)

**说明**：创建一个新的状态机实例。

**函数签名**：

```python
create(name: str, initial: str) -> str
```

```lua
create(name: string, initial: string) -> string
```

**参数**：
- `name` - 状态机名称
- `initial` - 初始状态名

**返回**：
- 状态机 ID

:::tabs

== Python

```python:line-numbers
from wingman import fsm

# 创建状态机
machine_id = fsm.create("combat", "idle")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 创建状态机
local machineId = wingman.fsm.create("combat", "idle")
```

:::

---

## 定义状态

### state(machine_id, state_name, on_enter?, on_exit?) / state(machineId, stateName, onEnter?, onExit?)

**说明**：定义或更新一个状态及其回调。

**函数签名**：

```python
state(machine_id: str, state_name: str, on_enter: Callable = None, on_exit: Callable = None) -> bool
```

```lua
state(machineId: string, stateName: string, onEnter: function = nil, onExit: function = nil) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID
- `state_name` / `stateName` - 状态名
- `on_enter` / `onEnter` - 可选，进入状态时的回调，接收上下文对象 `{state, from?, event, payload}`
- `on_exit` / `onExit` - 可选，离开状态时的回调，接收上下文对象 `{state, event, payload}`

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import fsm

machine_id = fsm.create("combat", "idle")

# 定义状态
fsm.state(machine_id, "idle", on_enter=lambda ctx: print("进入空闲状态"))
fsm.state(machine_id, "fight", on_enter=lambda ctx: print("进入战斗状态"))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local machineId = wingman.fsm.create("combat", "idle")

-- 定义状态
wingman.fsm.state(machineId, "idle", function(ctx)
    print("进入空闲状态")
end, function(ctx)
    print("离开空闲状态")
end)

wingman.fsm.state(machineId, "fight")
```

:::

---

## 定义状态转移

### transition(machine_id, from, to, on?, guard?, action?) / transition(machineId, from, to, on?, guard?, action?)

**说明**：定义状态转移规则。

**函数签名**：

```python
transition(machine_id: str, from: str, to: str, on: str = "", guard: Callable = None, action: Callable = None) -> bool
```

```lua
transition(machineId: string, from: string, to: string, on: string = "", guard: function = nil, action: function = nil) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID
- `from` - 源状态
- `to` - 目标状态
- `on` - 可选，触发事件名
- `guard` - 可选，守卫函数，接收 `{from, to, event, payload}`，返回 `True`/`true` 才允许转移
- `action` - 可选，转移动作函数，接收 `{from, to, event, payload}`

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import fsm

# 定义状态转移
fsm.transition(machine_id, "idle", "fight", on="enemy_found")
fsm.transition(machine_id, "fight", "idle", on="enemy_lost")

# 带守卫函数
fsm.transition(machine_id, "pending", "running",
                on="start", guard=lambda ctx: ctx['payload'].get('ready', False))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 定义状态转移
wingman.fsm.transition(machineId, "idle", "fight", "enemy_found")
wingman.fsm.transition(machineId, "fight", "idle", "enemy_lost")

-- 带守卫函数
wingman.fsm.transition(machineId, "pending", "running", "start", function(ctx)
    return ctx.payload.ready == true
end)
```

:::

---

## 派发事件

### dispatch(machine_id, event, payload?) / dispatch(machineId, event, payload?)

**说明**：派发事件到状态机，触发可能的状态转移。

**函数签名**：

```python
dispatch(machine_id: str, event: str, payload: dict = None) -> bool
```

```lua
dispatch(machineId: string, event: string, payload: table = nil) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID
- `event` - 事件名
- `payload` - 可选，事件载荷

**返回**：
- 是否成功触发转移

:::tabs

== Python

```python:line-numbers
from wingman import fsm

# 派发事件
fsm.dispatch(machine_id, "enemy_found", {"target": "boss"})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 派发事件
wingman.fsm.dispatch(machineId, "enemy_found", { target = "boss" })
```

:::

---

## 获取当前状态

### current(machine_id) / current(machineId)

**说明**：获取当前状态名。

**函数签名**：

```python
current(machine_id: str) -> str
```

```lua
current(machineId: string) -> string
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID

**返回**：
- 当前状态名

---

## 检查状态

### is(machine_id, state) / is(machineId, state)

**说明**：检查状态机是否处于指定状态。

**函数签名**：

```python
is(machine_id: str, state: str) -> bool
```

```lua
is(machineId: string, state: string) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID
- `state` - 要检查的状态名

**返回**：
- 是否处于该状态

---

## 检查可否转移

### can(machine_id, event) / can(machineId, event)

**说明**：检查是否可以触发指定事件的转移。

**函数签名**：

```python
can(machine_id: str, event: str) -> bool
```

```lua
can(machineId: string, event: string) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID
- `event` - 事件名

**返回**：
- 是否可以转移

---

## 重置状态机

### reset(machine_id) / reset(machineId)

**说明**：重置状态机到初始状态。

**函数签名**：

```python
reset(machine_id: str) -> bool
```

```lua
reset(machineId: string) -> boolean
```

**参数**：
- `machine_id` / `machineId` - 状态机 ID

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import fsm

# 重置状态机
fsm.reset(machine_id)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 重置状态机
wingman.fsm.reset(machineId)
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `create(name, initial)` | `create(name, initial)` | 创建状态机 | name: 状态机名称<br>initial: 初始状态名<br>返回: 状态机ID |
| `state(machineId, stateName, onEnter?, onExit?)` | `state(machineId, stateName, onEnter?, onExit?)` | 定义状态 | machineId: 状态机ID<br>stateName: 状态名<br>onEnter: 进入回调(可选)<br>onExit: 离开回调(可选)<br>返回: 是否成功 |
| `transition(machineId, from, to, on?, guard?, action?)` | `transition(machineId, from, to, on?, guard?, action?)` | 定义转移 | machineId: 状态机ID<br>from: 源状态<br>to: 目标状态<br>on: 事件名(可选)<br>guard: 守卫函数(可选)<br>action: 动作函数(可选)<br>返回: 是否成功 |
| `dispatch(machineId, event, payload?)` | `dispatch(machineId, event, payload?)` | 派发事件 | machineId: 状态机ID<br>event: 事件名<br>payload: 事件载荷(可选)<br>返回: 是否成功转移 |
| `current(machineId)` | `current(machineId)` | 获取当前状态 | machineId: 状态机ID<br>返回: 当前状态名 |
| `is(machineId, state)` | `is(machineId, state)` | 检查状态 | machineId: 状态机ID<br>state: 状态名<br>返回: 是否处于该状态 |
| `can(machineId, event)` | `can(machineId, event)` | 检查可否转移 | machineId: 状态机ID<br>event: 事件名<br>返回: 是否可以转移 |
| `reset(machineId)` | `reset(machineId)` | 重置状态机 | machineId: 状态机ID<br>返回: 是否成功 |
