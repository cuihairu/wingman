# Event API

`wingman.event` 提供跨模块事件触发和订阅接口。

## Python

```python
from wingman import event

# 订阅事件
def on_enemy(e: event.EventMessage) -> None:
    print(f"Found enemy at {e['payload']}")

sub_id = event.on("combat.enemy_found", on_enemy, name="my-handler")

# 一次性订阅
event.once("task.done", lambda e: print("Task done!"))

# 触发事件
event.emit("combat.enemy_found", {"x": 100, "y": 200}, {"source": "vision"})

# 取消订阅
event.off(sub_id)

# 清空所有监听
event.clear()
```

## Lua

```lua
local event = require("wingman.event")

local function onEnemy(e)
    print("Found enemy at " .. e.payload.x)
end

local id = event.on("combat.enemy_found", onEnemy, "my-handler")

event.once("task.done", function(e)
    print("Task done!")
end)

event.emit("combat.enemy_found", { x = 100, y = 200 }, { source = "vision" })

event.off(id)

event.clear()
```

## 可用接口

### `on(type, callback, name?)`

订阅事件，每次事件触发时都会调用回调。

- `type`: 事件名
- `callback`: 回调函数，接收 `EventMessage` 对象
- `name`: 可选，订阅名称，可用于按名称取消订阅
- **返回**: 订阅 ID

### `once(type, callback)`

订阅事件，仅触发一次后自动取消订阅。

- `type`: 事件名
- `callback`: 回调函数，接收 `EventMessage` 对象
- **返回**: 订阅 ID

### `emit(type, payload?, meta?)`

触发事件。

- `type`: 事件名
- `payload`: 可选，事件载荷（任意 JSON 兼容对象）
- `meta`: 可选，元数据 `{source?, correlationId?, priority?}`
- **返回**: 是否成功

### `off(subscription)`

取消订阅。

- `subscription`: 订阅 ID 或名称
- **返回**: 是否成功

### `clear()`

清空全部事件监听。

### `message(type, payload?, meta?)`

构造标准事件对象，供调试或测试使用。

- **返回**: `EventMessage` 对象

## 事件对象

标准事件对象字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 事件名 |
| `source` | string | 来源模块 |
| `correlationId` | string | 关联 ID |
| `timestamp` | integer | Unix ms |
| `priority` | integer | 优先级 |
| `payload` | object | 业务载荷 |
