# FSM API

`wingman.fsm` 提供事件驱动状态机功能，用于管理游戏状态、流程控制等场景。

## 创建状态机

::: code-group

```python [Python]
from wingman import fsm

# 创建状态机
machine_id = fsm.create("combat", "idle")
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 创建状态机
local machineId = fsm.create("combat", "idle")
```

:::

## 定义状态及回调

::: code-group

```python [Python]
from wingman import fsm

machine_id = fsm.create("combat", "idle")

# 定义状态
fsm.state(machine_id, "idle", on_enter=lambda ctx: print("进入空闲状态"))
fsm.state(machine_id, "fight", on_enter=lambda ctx: print("进入战斗状态"))
```

```lua [Lua]
local fsm = require("wingman.fsm")

local machineId = fsm.create("combat", "idle")

-- 定义状态
fsm.state(machineId, "idle", function(ctx)
    print("进入空闲状态")
end, function(ctx)
    print("离开空闲状态")
end)

fsm.state(machineId, "fight")
```

:::

## 定义状态转移

::: code-group

```python [Python]
from wingman import fsm

# 定义状态转移
fsm.transition(machine_id, "idle", "fight", on="enemy_found")
fsm.transition(machine_id, "fight", "idle", on="enemy_lost")
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 定义状态转移
fsm.transition(machineId, "idle", "fight", "enemy_found")
fsm.transition(machineId, "fight", "idle", "enemy_lost")
```

:::

## 派发事件

::: code-group

```python [Python]
from wingman import fsm

# 派发事件
fsm.dispatch(machine_id, "enemy_found", {"target": "boss"})
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 派发事件
fsm.dispatch(machineId, "enemy_found", { target = "boss" })
```

:::

## 获取当前状态

::: code-group

```python [Python]
from wingman import fsm

# 获取当前状态
current = fsm.current(machine_id)
print(f"当前状态: {current}")
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 获取当前状态
local current = fsm.current(machineId)
print("当前状态: " .. current)
```

:::

## 重置状态机

::: code-group

```python [Python]
from wingman import fsm

# 重置状态机
fsm.reset(machine_id)
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 重置状态机
fsm.reset(machineId)
```

:::

---

## 完整示例

### 游戏战斗状态机

::: code-group

```python [Python]
from wingman import fsm, vision, input

# 创建战斗状态机
combat_id = fsm.create("combat", "idle")

# 定义状态
fsm.state(combat_id, "idle",
          on_enter=lambda ctx: print("进入空闲状态，开始巡逻"),
          on_exit=lambda ctx: print("离开空闲状态"))

fsm.state(combat_id, "fight",
          on_enter=lambda ctx: print("进入战斗状态"),
          on_exit=lambda ctx: print("离开战斗状态"))

fsm.state(combat_id, "flee",
          on_enter=lambda ctx: print("开始逃跑"),
          on_exit=lambda ctx: print("停止逃跑"))

# 定义转移
fsm.transition(combat_id, "idle", "fight", on="enemy_found")
fsm.transition(combat_id, "fight", "idle", on="enemy_lost")
fsm.transition(combat_id, "fight", "flee", on="low_hp")
fsm.transition(combat_id, "flee", "idle", on="hp_recovered")

# 主循环
while True:
    # 检测敌人
    enemy = vision.find_enemy()
    if enemy and fsm.current(combat_id) == "idle":
        fsm.dispatch(combat_id, "enemy_found", {"target": enemy})

    # 检测血量
    hp = vision.get_hp()
    if hp < 20 and fsm.current(combat_id) == "fight":
        fsm.dispatch(combat_id, "low_hp", {"hp": hp})
    elif hp > 50 and fsm.current(combat_id) == "flee":
        fsm.dispatch(combat_id, "hp_recovered", {"hp": hp})
```

```lua [Lua]
local fsm = require("wingman.fsm")
local vision = require("wingman.vision")
local input = require("wingman.input")

-- 创建战斗状态机
local combatId = fsm.create("combat", "idle")

-- 定义状态
fsm.state(combatId, "idle",
    function(ctx) print("进入空闲状态，开始巡逻") end,
    function(ctx) print("离开空闲状态") end
)

fsm.state(combatId, "fight",
    function(ctx) print("进入战斗状态") end,
    function(ctx) print("离开战斗状态") end
)

fsm.state(combatId, "flee",
    function(ctx) print("开始逃跑") end,
    function(ctx) print("停止逃跑") end
)

-- 定义转移
fsm.transition(combatId, "idle", "fight", "enemy_found")
fsm.transition(combatId, "fight", "idle", "enemy_lost")
fsm.transition(combatId, "fight", "flee", "low_hp")
fsm.transition(combatId, "flee", "idle", "hp_recovered")

-- 主循环
while true do
    -- 检测敌人
    local enemy = vision.findEnemy()
    if enemy and fsm.current(combatId) == "idle" then
        fsm.dispatch(combatId, "enemy_found", { target = enemy })
    end

    -- 检测血量
    local hp = vision.getHp()
    if hp < 20 and fsm.current(combatId) == "fight" then
        fsm.dispatch(combatId, "low_hp", { hp = hp })
    elseif hp > 50 and fsm.current(combatId) == "flee" then
        fsm.dispatch(combatId, "hp_recovered", { hp = hp })
    end
end
```

:::

### 带守卫的状态转移

::: code-group

```python [Python]
from wingman import fsm

# 创建状态机
task_id = fsm.create("task", "pending")

# 定义带守卫的转移
def can_start(ctx):
    # 只有前置条件满足才能开始
    return ctx['payload'].get('ready', False)

def can_complete(ctx):
    # 只有所有步骤完成才能结束
    return ctx['payload'].get('steps_done', 0) >= 5

fsm.state(task_id, "pending")
fsm.state(task_id, "running")
fsm.state(task_id, "completed")

# 使用守卫函数
fsm.transition(task_id, "pending", "running",
                on="start", guard=can_start)
fsm.transition(task_id, "running", "completed",
                on="finish", guard=can_complete)

# 测试
fsm.dispatch(task_id, "start", {"ready": False})  # 不会转移
print(fsm.current(task_id))  # "pending"

fsm.dispatch(task_id, "start", {"ready": True})  # 会转移
print(fsm.current(task_id))  # "running"
```

```lua [Lua]
local fsm = require("wingman.fsm")

-- 创建状态机
local taskId = fsm.create("task", "pending")

-- 定义带守卫的转移
local function canStart(ctx)
    -- 只有前置条件满足才能开始
    return ctx.payload.ready == true
end

local function canComplete(ctx)
    -- 只有所有步骤完成才能结束
    return ctx.payload.steps_done >= 5
end

fsm.state(taskId, "pending")
fsm.state(taskId, "running")
fsm.state(taskId, "completed")

-- 使用守卫函数
fsm.transition(taskId, "pending", "running", "start", canStart)
fsm.transition(taskId, "running", "completed", "finish", canComplete)

-- 测试
fsm.dispatch(taskId, "start", { ready = false })  -- 不会转移
print(fsm.current(taskId))  -- "pending"

fsm.dispatch(taskId, "start", { ready = true })  -- 会转移
print(fsm.current(taskId))  -- "running"
```

:::

---

## 可用接口

### `create(name, initial)` / `create(name, initial)`

创建一个新的状态机实例。

**参数：**
- `name` - 状态机名称
- `initial` - 初始状态名

**返回：**
- `number/string` - 状态机 ID

### `state(machineId, stateName, onEnter?, onExit?)` / `state(machineId, stateName, onEnter?, onExit?)`

定义或更新一个状态的回调。

**参数：**
- `machineId` - 状态机 ID
- `stateName` - 状态名
- `onEnter` - 可选，进入状态时的回调，参数 `{state, from?, event, payload}`
- `onExit` - 可选，离开状态时的回调，参数 `{state, event, payload}`

**返回：**
- `boolean` - 是否成功

### `transition(machineId, from, to, on?, guard?, action?)` / `transition(machineId, from, to, on?, guard?, action?)`

定义状态转移规则。

**参数：**
- `machineId` - 状态机 ID
- `from` - 源状态
- `to` - 目标状态
- `on` - 可选，触发事件名
- `guard` - 可选，守卫函数，参数 `{from, to, event, payload}`，返回 `bool` 决定是否允许转移
- `action` - 可选，转移动作函数，参数 `{from, to, event, payload}`

**返回：**
- `boolean` - 是否成功

### `dispatch(machineId, event, payload?)` / `dispatch(machineId, event, payload?)`

派发事件到状态机，触发可能的状态转移。

**参数：**
- `machineId` - 状态机 ID
- `event` - 事件名
- `payload` - 可选，事件载荷

**返回：**
- `boolean` - 是否成功触发转移

### `current(machineId)` / `current(machineId)`

获取当前状态。

**参数：**
- `machineId` - 状态机 ID

**返回：**
- `string` - 当前状态名

### `reset(machineId)` / `reset(machineId)`

重置状态机到初始状态。

**参数：**
- `machineId` - 状态机 ID

**返回：**
- `boolean` - 是否成功

### `is(machineId, state)` / `is(machineId, state)`

检查状态机是否处于指定状态。

**参数：**
- `machineId` - 状态机 ID
- `state` - 要检查的状态名

**返回：**
- `boolean` - 是否处于该状态

### `can(machineId, event)` / `can(machineId, event)`

检查是否可以触发指定事件的转移。

**参数：**
- `machineId` - 状态机 ID
- `event` - 事件名

**返回：**
- `boolean` - 是否可以转移
