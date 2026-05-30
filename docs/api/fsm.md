# FSM API

`wingman.fsm` 提供事件驱动状态机功能，用于管理游戏状态、流程控制等场景。

## Python

```python
from wingman import fsm

# 创建状态机
machine_id = fsm.create("combat", "idle")

# 定义状态及回调
fsm.state(machine_id, "idle", on_enter=lambda ctx: print("进入空闲状态"))
fsm.state(machine_id, "fight", on_enter=lambda ctx: print("进入战斗状态"))

# 定义状态转移
fsm.transition(machine_id, "idle", "fight", on="enemy_found")
fsm.transition(machine_id, "fight", "idle", on="enemy_lost")

# 派发事件
fsm.dispatch(machine_id, "enemy_found", {"target": "boss"})

# 获取当前状态
current = fsm.current(machine_id)  # "fight"

# 重置状态机
fsm.reset(machine_id)
```

## Lua

```lua
local fsm = require("wingman.fsm")

local machineId = fsm.create("combat", "idle")

fsm.state(machineId, "idle", function(ctx)
    print("进入空闲状态")
end, function(ctx)
    print("离开空闲状态")
end)

fsm.state(machineId, "fight")

fsm.transition(machineId, "idle", "fight", "enemy_found")
fsm.transition(machineId, "fight", "idle", "enemy_lost")

fsm.dispatch(machineId, "enemy_found", { target = "boss" })

local current = fsm.current(machineId)  -- "fight"

fsm.reset(machineId)
```

## 可用接口

### `create(name, initial)`

创建一个新的状态机实例。

- `name`: 状态机名称
- `initial`: 初始状态名
- **返回**: 状态机 ID（用于后续操作）

### `state(machineId, stateName, onEnter?, onExit?)`

定义或更新一个状态的回调。

- `machineId`: 状态机 ID
- `stateName`: 状态名
- `onEnter`: 可选，进入状态时的回调，参数 `{state, from?, event, payload}`
- `onExit`: 可选，离开状态时的回调，参数 `{state, event, payload}`
- **返回**: 是否成功

### `transition(machineId, from, to, on?, guard?, action?)`

定义状态转移规则。

- `machineId`: 状态机 ID
- `from`: 源状态
- `to`: 目标状态
- `on`: 触发事件名（可选）
- `guard`: 可选，守卫函数，参数 `{from, to, event, payload}`，返回 `bool` 决定是否允许转移
- `action`: 可选，转移动作函数，参数 `{from, to, event, payload}`
- **返回**: 是否成功

### `dispatch(machineId, event, payload?)`

派发事件到状态机，触发可能的状态转移。

- `machineId`: 状态机 ID
- `event`: 事件名
- `payload`: 可选，事件载荷
- **返回**: 是否成功触发转移

### `current(machineId)`

获取当前状态。

- `machineId`: 状态机 ID
- **返回**: 当前状态名

### `reset(machineId)`

重置状态机到初始状态。

- `machineId`: 状态机 ID
- **返回**: 是否成功

## 自动事件

状态变化会自动触发 `fsm.changed` 事件：

```python
event.on("fsm.changed", lambda e: print(f"状态变化: {e['payload']['from']} -> {e['payload']['to']}"))
```

事件载荷包含：
- `machine`: 状态机名称
- `from`: 源状态
- `to`: 目标状态
- `event`: 触发的事件
