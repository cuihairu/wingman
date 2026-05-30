# Event API

`wingman.event` 提供跨模块事件触发和订阅接口。

## 订阅事件

:::tabs

== Python

```python
from wingman import event

# 订阅事件
def on_enemy(e: event.EventMessage) -> None:
    print(f"Found enemy at {e['payload']}")

sub_id = event.on("combat.enemy_found", on_enemy, name="my-handler")
```

== Lua

```lua
local event = require("wingman.event")

-- 订阅事件
local function onEnemy(e)
    print("Found enemy at " .. e.payload.x)
end

local id = event.on("combat.enemy_found", onEnemy, "my-handler")
```

:::

## 一次性订阅

:::tabs

== Python

```python
from wingman import event

# 一次性订阅
event.once("task.done", lambda e: print("Task done!"))
```

== Lua

```lua
local event = require("wingman.event")

-- 一次性订阅
event.once("task.done", function(e)
    print("Task done!")
end)
```

:::

## 触发事件

:::tabs

== Python

```python
from wingman import event

# 触发事件
event.emit("combat.enemy_found", {"x": 100, "y": 200}, {"source": "vision"})
```

== Lua

```lua
local event = require("wingman.event")

-- 触发事件
event.emit("combat.enemy_found", { x = 100, y = 200 }, { source = "vision" })
```

:::

## 取消订阅

:::tabs

== Python

```python
from wingman import event

# 取消订阅
event.off(sub_id)

# 清空所有监听
event.clear()
```

== Lua

```lua
local event = require("wingman.event")

-- 取消订阅
event.off(id)

-- 清空所有监听
event.clear()
```

:::

---

## 完整示例

### 游戏战斗事件系统

:::tabs

== Python

```python
from wingman import event, vision

# 定义战斗事件处理
def on_enemy_found(e):
    pos = e['payload']
    print(f"发现敌人 at ({pos['x']}, {pos['y']})")

def on_enemy_lost(e):
    print("敌人消失，停止战斗")

def on_low_hp(e):
    hp = e['payload']['hp']
    if hp < 20:
        print(f"HP 危险: {hp}，准备逃跑")

# 订阅事件
event.on("combat.enemy_found", on_enemy_found)
event.on("combat.enemy_lost", on_enemy_lost)
event.on("combat.low_hp", on_low_hp)

# 主循环
while True:
    # 检测敌人
    enemy = vision.find_enemy()
    if enemy:
        event.emit("combat.enemy_found", enemy)

    # 检测血量
    hp = vision.get_hp()
    if hp < 20:
        event.emit("combat.low_hp", {"hp": hp})
```

== Lua

```lua
local event = require("wingman.event")
local vision = require("wingman.vision")

-- 定义战斗事件处理
local function onEnemyFound(e)
    local pos = e.payload
    print(string.format("发现敌人 at (%d, %d)", pos.x, pos.y))
end

local function onEnemyLost(e)
    print("敌人消失，停止战斗")
end

local function onLowHp(e)
    local hp = e.payload.hp
    if hp < 20 then
        print(string.format("HP 危险: %d，准备逃跑", hp))
    end
end

-- 订阅事件
event.on("combat.enemy_found", onEnemyFound)
event.on("combat.enemy_lost", onEnemyLost)
event.on("combat.low_hp", onLowHp)

-- 主循环
while true do
    -- 检测敌人
    local enemy = vision.findEnemy()
    if enemy then
        event.emit("combat.enemy_found", enemy)
    end

    -- 检测血量
    local hp = vision.getHp()
    if hp < 20 then
        event.emit("combat.low_hp", { hp = hp })
    end
end
```

:::

### 任务协作事件

:::tabs

== Python

```python
from wingman import event, task

# 任务状态监听
def on_task_start(e):
    task_id = e['payload']['task_id']
    print(f"任务 {task_id} 开始")

def on_task_complete(e):
    result = e['payload']['result']
    print(f"任务完成，结果: {result}")

def on_task_fail(e):
    error = e['payload']['error']
    print(f"任务失败: {error}")

# 订阅任务事件
event.on("task.start", on_task_start)
event.on("task.complete", on_task_complete)
event.on("task.fail", on_task_fail)

# 异步任务
def run_combat():
    event.emit("task.start", {"task_id": 1})
    try:
        # 执行战斗逻辑
        result = do_combat()
        event.emit("task.complete", {"task_id": 1, "result": result})
    except Exception as e:
        event.emit("task.fail", {"task_id": 1, "error": str(e)})
```

== Lua

```lua
local event = require("wingman.event")
local task = require("wingman.task")

-- 任务状态监听
local function onTaskStart(e)
    local taskId = e.payload.task_id
    print(string.format("任务 %d 开始", taskId))
end

local function onTaskComplete(e)
    local result = e.payload.result
    print(string.format("任务完成，结果: %s", result))
end

local function onTaskFail(e)
    local error = e.payload.error
    print(string.format("任务失败: %s", error))
end

-- 订阅任务事件
event.on("task.start", onTaskStart)
event.on("task.complete", onTaskComplete)
event.on("task.fail", onTaskFail)

-- 异步任务
local function runCombat()
    event.emit("task.start", { task_id = 1 })
    local success, result = pcall(doCombat)
    if success then
        event.emit("task.complete", { task_id = 1, result = result })
    else
        event.emit("task.fail", { task_id = 1, error = result })
    end
end
```

:::

---

## 可用接口

### `on(type, callback, name?)` / `on(type, callback, name?)`

订阅事件，每次事件触发时都会调用回调。

**参数：**
- `type` - 事件名
- `callback` - 回调函数，接收 `EventMessage` 对象
- `name` - 可选，订阅名称，可用于按名称取消订阅

**返回：**
- `number/string` - 订阅 ID

### `once(type, callback)` / `once(type, callback)`

订阅事件，仅触发一次后自动取消订阅。

**参数：**
- `type` - 事件名
- `callback` - 回调函数，接收 `EventMessage` 对象

**返回：**
- `number/string` - 订阅 ID

### `emit(type, payload?, meta?)` / `emit(type, payload?, meta?)`

触发事件。

**参数：**
- `type` - 事件名
- `payload` - 可选，事件载荷（任意 JSON 兼容对象）
- `meta` - 可选，元数据 `{source?, correlationId?, priority?}`

**返回：**
- `boolean` - 是否成功

### `off(subscription)` / `off(subscription)`

取消订阅。

**参数：**
- `subscription` - 订阅 ID 或名称

**返回：**
- `boolean` - 是否成功

### `clear()` / `clear()`

清空全部事件监听。

### `message(type, payload?, meta?)` / `message(type, payload?, meta?)`

构造标准事件对象，供调试或测试使用。

**返回：**
- `dict/table` - `EventMessage` 对象

---

## 事件对象

标准事件对象字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 事件唯一 ID |
| type | string | 事件类型名 |
| payload | any | 事件载荷 |
| source | string | 事件来源（可选） |
| correlationId | string | 关联 ID（可选） |
| timestamp | number | 时间戳（毫秒） |
