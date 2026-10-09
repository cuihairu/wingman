# API: wingman.event

事件订阅和发布模块，提供进程内事件通信。

## 模块概述

event 模块提供发布-订阅模式的本地事件总线：
- **进程内事件** - 本地事件总线，用于模块间通信
- **订阅事件** - 持久订阅或一次性订阅
- **发布事件** - 触发事件并传递数据
- **取消订阅** - 取消指定订阅或清空全部
- **查询监听器** - 按订阅 ID / 名称查询，或列出事件的全部订阅
- **事件对象** - 标准化的事件消息结构

---

## 订阅事件

### on(type, callback, name?) / on(type, callback, name?)

**说明**：订阅事件，每次事件触发时都会调用回调。

**函数签名**：

```python
on(type: str, callback: Callable, name: str = "") -> int
```

```lua
on(type: string, callback: function, name: string = "") -> int
```

**参数**：
- `type` - 事件名（支持点号分隔的命名空间，如 `"combat.enemy_found"`）
- `callback` - 回调函数，接收 `EventMessage` 对象
- `name` - 可选，订阅名称，可用于按名称取消订阅

**返回**：
- 订阅 ID（整数）

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
once(type: str, callback: Callable) -> int
```

```lua
once(type: string, callback: function) -> int
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
off(subscription: str | int) -> bool
```

```lua
off(subscription: string | number) -> boolean
```

**参数**：
- `subscription` - 订阅 ID（int）或名称（str）

**返回**：
- 恒为 `True`/`true`——仅表示调用被受理，**不反映订阅是否存在**（取消不存在的订阅也返回 true）

---

### clear(type?) / clear(type?)

**说明**：清理事件监听。无参（或 `nil`）清空全部事件监听；传入事件名时只清理该事件的全部订阅。

**函数签名**：

```python
clear(type: str | None = None) -> None
```

```lua
clear(type: string | nil = nil) -> nil
```

**参数**：
- `type` - 可选，事件名；省略时清空全部监听

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import event

# 取消指定订阅
event.off(sub_id)

# 只清理某事件的全部订阅
event.clear("combat.enemy_found")

# 清空所有监听
event.clear()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 取消指定订阅
wingman.event.off(id)

-- 只清理某事件的全部订阅
wingman.event.clear("combat.enemy_found")

-- 清空所有监听
wingman.event.clear()
```

:::

---

## 查询监听器

### listener(subscription) / listener(subscription)

**说明**：查询单个已注册监听器，按订阅 ID 或监听器名（`on` 的第三个参数）。同名监听器注册了多个时返回最早注册的那个；不存在时返回 `nil`。

**函数签名**：

```python
listener(subscription: int | str) -> ListenerInfo | None
```

```lua
listener(subscription: number | string) -> table | nil
```

**参数**：
- `subscription` - 订阅 ID（int）或监听器名（str）

**返回**：
- `ListenerInfo` 对象（字段见下表），未找到时为 `nil`/`None`

### listeners(type) / listeners(type)

**说明**：列出某事件的全部已注册监听器（按订阅 ID 升序，即注册顺序）。

**函数签名**：

```python
listeners(type: str) -> list[ListenerInfo]
```

```lua
listeners(type: string) -> table[]
```

**参数**：
- `type` - 事件名

**返回**：
- `ListenerInfo` 数组；事件无订阅时为空数组

**ListenerInfo 字段**：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | int | 订阅 ID（可传给 `off` 取消订阅） |
| type | string | 监听的事件名 |
| name | string | 监听器名（匿名订阅为空串） |
| once | bool | 是否为一次性订阅 |

:::tabs

== Python

```python:line-numbers
from wingman import event

sub_id = event.on("combat.enemy_found", on_enemy, name="my-handler")

# 按订阅 ID 查询
info = event.listener(sub_id)
print(info["type"], info["name"])   # combat.enemy_found my-handler

# 按监听器名查询
info = event.listener("my-handler")

# 列出某事件的全部监听器
for listener in event.listeners("combat.enemy_found"):
    print(listener["id"], listener["once"])
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local id = wingman.event.on("combat.enemy_found", onEnemy, "my-handler")

-- 按订阅 ID 查询
local info = wingman.event.listener(id)
print(info.type, info.name)         -- combat.enemy_found my-handler

-- 按监听器名查询
info = wingman.event.listener("my-handler")

-- 列出某事件的全部监听器
for _, listener in ipairs(wingman.event.listeners("combat.enemy_found")) do
    print(listener.id, listener.once)
end
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

## 内置事件源

除脚本自行 `emit` 的事件外，以下模块在运行期向总线**同步**分发事件（`event.on` 可直接订阅）。**注意**：这些事件由模块的后台线程（轮询/平台后端）触发，订阅回调也在该线程执行——Lua callable 非线程安全，订阅这类事件请使用 Python 回调；Lua 侧请改用触发器。

| 事件 | source | 载荷 | 说明 |
|-----|--------|------|-----|
| `systemwatch.process` | `"systemwatch"` | `{action, pid, name}` | 进程 started/exited（见 [systemwatch](./systemwatch.md)） |
| `systemwatch.window` | `"systemwatch"` | `{action, handle, title}` | 窗口 opened/closed/changed |
| `systemwatch.error` | `"systemwatch"` | `{error}` | 观察注册被拒（非线程安全 callable） |
| `filewatcher.changed` | `"filewatcher"` | `{type, path, oldPath, timestamp}` | 文件变更（见 [filewatcher](./filewatcher.md)） |
| `filewatcher.error` | `"filewatcher"` | `{error}` | 监听注册被拒（非线程安全 callable） |
| `trigger.fired` | `"trigger"` | `{id, name, type, triggered, lastTriggerTime}` | 触发器命中（`type` 为 TriggerType 整型值） |
| `trigger.action` | `"trigger"` | `{id, name, actionCount}` | 触发器动作执行完成（一次命中一条） |
| `hotkey.error` | `"hotkey"` | `{error}` | 热键注册被拒（非线程安全 callable；热键命中不走事件面，直接回调） |

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `on(type, callback, name?)` | `on(type, callback, name?)` | 订阅事件 | type: 事件名<br>callback: 回调函数<br>name: 订阅名称(可选)<br>返回: 订阅 ID |
| `once(type, callback)` | `once(type, callback)` | 一次性订阅 | type: 事件名<br>callback: 回调函数<br>返回: 订阅 ID |
| `emit(type, payload?, meta?)` | `emit(type, payload?, meta?)` | 发布事件 | type: 事件名<br>payload: 载荷(可选)<br>meta: 元数据(可选)<br>返回: 是否成功 |
| `off(subscription)` | `off(subscription)` | 取消订阅 | subscription: 订阅 ID 或名称<br>返回: 恒 true（不反映订阅是否存在） |
| `listener(subscription)` | `listener(subscription)` | 查询单个监听器 | subscription: 订阅 ID 或监听器名<br>返回: ListenerInfo 或 nil |
| `listeners(type)` | `listeners(type)` | 列出事件的全部监听器 | type: 事件名<br>返回: ListenerInfo 数组 |
| `clear(type?)` | `clear(type?)` | 清理全部或指定事件的监听 | type: 事件名(可选)<br>无返回值 |
| `message(type, payload?, meta?)` | `message(type, payload?, meta?)` | 构造事件对象 | 返回: EventMessage 对象 |
