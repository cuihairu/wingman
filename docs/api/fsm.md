> [中文](../zh/api/fsm.md) | **English**

# API: wingman.fsm

State machine module providing event-driven state machine functionality, used for scenarios such as managing game states and flow control.

## Module Overview

The fsm module provides state machine creation and management:
- **Create state machines** - Create a named state machine with a specified initial state
- **Define states** - Define states with enter/exit callbacks
- **Define transitions** - Define state transition rules with guard function support
- **Dispatch events** - Trigger state transitions
- **Query state** - Get the current state, check states

---

## Creating a State Machine

### create(name, initial)

**Description**: Creates a new state machine instance.

**Signature**:

```python
create(name: str, initial: str) -> str
```

```lua
create(name: string, initial: string) -> string
```

**Parameters**:
- `name` - State machine name
- `initial` - Initial state name

**Returns**:
- The state machine ID

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

## Defining States

### state(machine_id, state_name, on_enter?, on_exit?) / state(machineId, stateName, onEnter?, onExit?)

**Description**: Defines or updates a state and its callbacks.

**Signature**:

```python
state(machine_id: str, state_name: str, on_enter: Callable = None, on_exit: Callable = None) -> bool
```

```lua
state(machineId: string, stateName: string, onEnter: function = nil, onExit: function = nil) -> boolean
```

**Parameters**:
- `machine_id` / `machineId` - State machine ID
- `state_name` / `stateName` - State name
- `on_enter` / `onEnter` - Optional; callback invoked when entering the state, receives a context object `{state, from?, event, payload}`
- `on_exit` / `onExit` - Optional; callback invoked when leaving the state, receives a context object `{state, event, payload}`

**Returns**:
- Whether the operation succeeded

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

## Defining State Transitions

### transition(machine_id, from, to, on?, guard?, action?) / transition(machineId, from, to, on?, guard?, action?)

**Description**: Defines a state transition rule.

**Signature**:

```python
transition(machine_id: str, from: str, to: str, on: str = "", guard: Callable = None, action: Callable = None) -> bool
```

```lua
transition(machineId: string, from: string, to: string, on: string = "", guard: function = nil, action: function = nil) -> boolean
```

**Parameters**:
- `machine_id` / `machineId` - State machine ID
- `from` - Source state
- `to` - Target state
- `on` - Optional; the event name that triggers the transition
- `guard` - Optional; guard function that receives `{from, to, event, payload}`; the transition is allowed only when it returns `True`/`true`
- `action` - Optional; transition action function that receives `{from, to, event, payload}`

**Returns**:
- Whether the operation succeeded

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

## Dispatching Events

### dispatch(machine_id, event, payload?) / dispatch(machineId, event, payload?)

**Description**: Dispatches an event to the state machine, triggering a state transition if one applies.

**Signature**:

```python
dispatch(machine_id: str, event: str, payload: dict = None) -> bool
```

```lua
dispatch(machineId: string, event: string, payload: table = nil) -> boolean
```

**Parameters**:
- `machine_id` / `machineId` - State machine ID
- `event` - Event name
- `payload` - Optional; event payload

**Returns**:
- Whether a transition was successfully triggered

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

## Getting the Current State

### current(machine_id) / current(machineId)

**Description**: Gets the current state name.

**Signature**:

```python
current(machine_id: str) -> str
```

```lua
current(machineId: string) -> string
```

**Parameters**:
- `machine_id` / `machineId` - State machine ID

**Returns**:
- The current state name

---

## Resetting the State Machine

### reset(machine_id) / reset(machineId)

**Description**: Resets the state machine to its initial state.

**Signature**:

```python
reset(machine_id: str) -> bool
```

```lua
reset(machineId: string) -> boolean
```

**Parameters**:
- `machine_id` / `machineId` - State machine ID

**Returns**:
- Whether the operation succeeded

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

## Events

### fsm.changed

After every successful state machine transition, the module emits an `fsm.changed` event. Payload fields:

- `machine` - State machine name
- `from` - Source state name
- `to` - Target state name
- `event` - Name of the event that triggered the transition

Subscribe with `wingman.event.on("fsm.changed", cb)` to observe state changes in one place.

---

## Available APIs

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `create(name, initial)` | `create(name, initial)` | Create a state machine | name: state machine name<br>initial: initial state name<br>Returns: state machine ID |
| `state(machineId, stateName, onEnter?, onExit?)` | `state(machineId, stateName, onEnter?, onExit?)` | Define a state | machineId: state machine ID<br>stateName: state name<br>onEnter: enter callback (optional)<br>onExit: exit callback (optional)<br>Returns: whether it succeeded |
| `transition(machineId, from, to, on?, guard?, action?)` | `transition(machineId, from, to, on?, guard?, action?)` | Define a transition | machineId: state machine ID<br>from: source state<br>to: target state<br>on: event name (optional)<br>guard: guard function (optional)<br>action: action function (optional)<br>Returns: whether it succeeded |
| `dispatch(machineId, event, payload?)` | `dispatch(machineId, event, payload?)` | Dispatch an event | machineId: state machine ID<br>event: event name<br>payload: event payload (optional)<br>Returns: whether a transition was triggered |
| `current(machineId)` | `current(machineId)` | Get the current state | machineId: state machine ID<br>Returns: current state name |
| `reset(machineId)` | `reset(machineId)` | Reset the state machine | machineId: state machine ID<br>Returns: whether it succeeded |
