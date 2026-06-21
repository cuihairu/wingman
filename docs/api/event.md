# API: wingman.event

事件订阅和发布模块，提供进程内事件通信。

## 模块概述

event 模块提供发布-订阅模式的本地事件总线：
- **进程内事件** - 本地事件总线，用于模块间通信
- **订阅事件** - 持久订阅或一次性订阅
- **发布事件** - 触发事件并传递数据
- **取消订阅** - 取消指定订阅或清空全部
- **事件对象** - 标准化的事件消息结构

---

## 订阅事件

### on(type, callback, name?) / on(type, callback, name?)

**说明**：订阅事件，每次事件触发时都会调用回调。

**函数签名**：

```python
on(type: str, callback: Callable, name: str = "") -> str
```

```lua
on(type: string, callback: function, name: string = "") -> string
```

**参数**：
- `type` - 事件名（支持点号分隔的命名空间，如 `"combat.enemy_found"`）
- `callback` - 回调函数，接收 `EventMessage` 对象
- `name` - 可选，订阅名称，可用于按名称取消订阅

**返回**：
- 订阅 ID

:::tabs

== Python

```python:line-numbers
from wingman import event

# 定义事件处理函数
def on_enemy(e):
    print(f"Found enemy at {e['payload']}")

# 订阅事件
sub_id = event.on("combat.enemy_found", on_enemy, name="my-handler")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 定义事件处理函数
local function onEnemy(e)
    print("Found enemy at " .. e.payload.x)
end

-- 订阅事件
local id = wingman.event.on("combat.enemy_found", onEnemy, "my-handler")
```

:::

---

## 一次性订阅

### once(type, callback) / once(type, callback)

**说明**：订阅事件，仅触发一次后自动取消订阅。

**函数签名**：

```python
once(type: str, callback: Callable) -> str
```

```lua
once(type: string, callback: function) -> string
```

**参数**：
- `type` - 事件名
- `callback` - 回调函数，接收 `EventMessage` 对象

**返回**：
- 订阅 ID

:::tabs

== Python

```python:line-numbers
from wingman import event

# 一次性订阅
event.once("task.done", lambda e: print("Task done!"))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 一次性订阅
wingman.event.once("task.done", function(e)
    print("Task done!")
end)
```

:::

---

## 发布事件

### emit(type, payload?, meta?) / emit(type, payload?, meta?)

**说明**：触发事件，通知所有订阅者。

**函数签名**：

```python
emit(type: str, payload: Any = None, meta: dict = None) -> bool
```

```lua
emit(type: string, payload: any = nil, meta: table = nil) -> boolean
```

**参数**：
- `type` - 事件名
- `payload` - 可选，事件载荷（任意 JSON 兼容对象）
- `meta` - 可选，元数据
  - `source` - 事件来源
  - `correlationId` - 关联 ID（用于追踪事件链）
  - `priority` - 优先级

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 触发事件
event.emit("combat.enemy_found", {"x": 100, "y": 200}, {"source": "vision"})

# 带关联 ID
event.emit("combat.enemy_found", enemy, {"correlationId": "session-123"})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 触发事件
wingman.event.emit("combat.enemy_found", { x = 100, y = 200 }, { source = "vision" })

-- 带关联 ID
wingman.event.emit("combat.enemy_found", enemy, { correlationId = "session-123" })
```

:::

---

## 取消订阅

### off(subscription) / off(subscription)

**说明**：取消指定的订阅。

**函数签名**：

```python
off(subscription: str) -> bool
```

```lua
off(subscription: string) -> boolean
```

**参数**：
- `subscription` - 订阅 ID 或名称

**返回**：
- 是否成功

---

### clear() / clear()

**说明**：清空全部事件监听。

**函数签名**：

```python
clear() -> None
```

```lua
clear() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import event

# 取消指定订阅
event.off(sub_id)

# 清空所有监听
event.clear()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 取消指定订阅
wingman.event.off(id)

-- 清空所有监听
wingman.event.clear()
```

:::

---

## 构造事件对象

### message(type, payload?, meta?) / message(type, payload?, meta?)

**说明**：构造标准事件对象，供调试或测试使用。

**函数签名**：

```python
message(type: str, payload: Any = None, meta: dict = None) -> dict
```

```lua
message(type: string, payload: any = nil, meta: table = nil) -> table
```

**参数**：
- `type` - 事件名
- `payload` - 可选，事件载荷
- `meta` - 可选，元数据

**返回**：
- `EventMessage` 对象

---

## 事件对象结构

标准 `EventMessage` 对象包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| type | string | 事件类型名 |
| payload | any | 事件载荷 |
| source | string | 事件来源（可选） |
| correlationId | string | 关联 ID（可选） |
| timestamp | number | 时间戳（毫秒） |
| priority | number | 优先级（可选） |

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `on(type, callback, name?)` | `on(type, callback, name?)` | 订阅事件 | type: 事件名<br>callback: 回调函数<br>name: 订阅名称(可选)<br>返回: 订阅 ID |
| `once(type, callback)` | `once(type, callback)` | 一次性订阅 | type: 事件名<br>callback: 回调函数<br>返回: 订阅 ID |
| `emit(type, payload?, meta?)` | `emit(type, payload?, meta?)` | 发布事件 | type: 事件名<br>payload: 载荷(可选)<br>meta: 元数据(可选)<br>返回: 是否成功 |
| `off(subscription)` | `off(subscription)` | 取消订阅 | subscription: 订阅 ID 或名称<br>返回: 是否成功 |
| `clear()` | `clear()` | 清空全部监听 | 无返回值 |
| `message(type, payload?, meta?)` | `message(type, payload?, meta?)` | 构造事件对象 | 返回: EventMessage 对象 |
