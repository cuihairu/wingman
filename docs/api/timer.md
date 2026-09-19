# API: wingman.timer

定时器模块，提供一次性/周期性定时回调与同步睡眠能力，用于在脚本中安排延时任务。

## 模块概述

timer 模块提供以下能力：

- **after / setTimeout** - 一次性定时器：到点触发一次回调
- **every / setInterval** - 周期定时器：按间隔重复触发，直到取消
- **clearTimer / cancel / clearTimeout / clearInterval** - 取消定时器
- **exists / count** - 查询待触发定时器
- **clearAll** - 清理全部待触发定时器
- **sleep** - 同步阻塞当前脚本线程

> **注意**：回调在后台 timer 线程执行（非脚本主线程）。Python 回调经 GIL 包装跨线程安全；
> Lua 回调非线程安全，与 `event.on` + 后台线程 emit 的既有敞口一致，Lua 脚本中请只做
> 简单的标志位/队列操作或自行评估线程安全。脚本引擎关闭时会自动清理该脚本创建的定时器。
> Python 侧多词函数同时暴露 camelCase（`clearTimer`）与 snake_case（`clear_timer`）两种形式，下文以 camelCase 示例。

---

## after

### after(ms, callback)

**说明**：创建一次性定时器，`ms` 毫秒后在 timer 线程触发一次 `callback`。`ms` 小于 0 按 0 处理。

**函数签名**：

```python
after(ms: int, callback: Callable[[], Any]) -> int
```

```lua
after(ms: number, callback: function) -> number
```

**参数**：
- `ms` - 延迟毫秒数
- `callback` - 到点触发的回调（不接收参数）

**返回**：
- `int`/`number` - timerId（参数无效时返回 0）

:::tabs

== Python

```python:line-numbers
from wingman import timer

def on_timeout():
    print("timeout!")

tid = timer.after(3000, on_timeout)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tid = wingman.timer.after(3000, function()
    print("timeout!")
end)
```

:::

---

## every

### every(ms, callback)

**说明**：创建周期定时器，每 `ms` 毫秒触发一次 `callback`，直到调用 clearTimer/clearAll 或脚本结束。回调执行落后过多时重置基点，避免追赶风暴。

**函数签名**：

```python
every(ms: int, callback: Callable[[], Any]) -> int
```

```lua
every(ms: number, callback: function) -> number
```

**参数**：
- `ms` - 触发间隔毫秒数（必须大于 0）
- `callback` - 周期触发的回调（不接收参数）

**返回**：
- `int`/`number` - timerId（参数无效时返回 0）

:::tabs

== Python

```python:line-numbers
from wingman import timer

tid = timer.every(500, lambda: print("tick"))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tid = wingman.timer.every(500, function()
    print("tick")
end)
```

:::

---

## clearTimer

### clearTimer(timerId)

**说明**：取消一次性或周期定时器。已触发或不存在返回 false。

**函数签名**：

```python
clearTimer(timerId: int) -> bool
```

```lua
clearTimer(timerId: number) -> boolean
```

**参数**：
- `timerId` - after/every 返回的定时器 ID

**返回**：
- `bool`/`boolean` - 是否成功取消

:::tabs

== Python

```python:line-numbers
from wingman import timer

tid = timer.every(500, lambda: print("tick"))
timer.clearTimer(tid)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tid = wingman.timer.every(500, function() end)
wingman.timer.clearTimer(tid)
```

:::

---

## exists / count

### exists(timerId) / count()

**说明**：查询指定定时器是否仍在等待触发；查询当前待触发的定时器数量。

**函数签名**：

```python
exists(timerId: int) -> bool
count() -> int
```

```lua
exists(timerId: number) -> boolean
count() -> number
```

**返回**：
- `bool`/`boolean` - 是否存在
- `int`/`number` - 待触发定时器数量

---

## clearAll

### clearAll()

**说明**：清理全部待触发定时器。

**函数签名**：

```python
clearAll() -> int
```

```lua
clearAll() -> number
```

**返回**：
- `int`/`number` - 清理的定时器数量

---

## sleep

### sleep(ms)

**说明**：同步阻塞当前脚本线程 `ms` 毫秒（负值视为 0）。注意这会阻塞调用方线程本身；
如需不阻塞的延时，请改用 `after`。

**函数签名**：

```python
sleep(ms: int) -> None
```

```lua
sleep(ms: number) -> nil
```

---

## 完整示例

### Python

```python
from wingman import timer

# 一次性：3 秒后提醒
timer.after(3000, lambda: print("3s passed"))

# 周期：每秒检查一次血量，共 10 次
count = 0

def check_hp():
    global count
    count += 1
    print("check", count)
    if count >= 10:
        timer.clearTimer(tid)

tid = timer.every(1000, check_hp)

# 查询
print(timer.count())
```

### Lua

```lua
local wingman = require("wingman")
local timer = wingman.timer

-- 一次性：3 秒后提醒
timer.after(3000, function()
    print("3s passed")
end)

-- 周期：每秒检查一次，共 10 次
local count = 0
local tid

tid = timer.every(1000, function()
    count = count + 1
    print("check", count)
    if count >= 10 then
        timer.clearTimer(tid)
    end
end)

-- 查询
print(timer.count())
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `after(ms, callback)` | `after(ms, callback)` | 一次性定时器 | ms: 毫秒<br>callback: 回调 |
| `setTimeout(ms, callback)` | `setTimeout(ms, callback)` | after 的 JS 风格别名 | 同 after |
| `every(ms, callback)` | `every(ms, callback)` | 周期定时器 | ms: 间隔<br>callback: 回调 |
| `setInterval(ms, callback)` | `setInterval(ms, callback)` | every 的 JS 风格别名 | 同 every |
| `clearTimer(timerId)` | `clearTimer(timerId)` | 取消定时器 | timerId: 定时器 ID |
| `cancel(timerId)` | `cancel(timerId)` | clearTimer 的别名 | 同 clearTimer |
| `clearTimeout(timerId)` | `clearTimeout(timerId)` | clearTimer 的 JS 风格别名 | 同 clearTimer |
| `clearInterval(timerId)` | `clearInterval(timerId)` | clearTimer 的 JS 风格别名 | 同 clearTimer |
| `exists(timerId)` | `exists(timerId)` | 是否仍在等待触发 | timerId: 定时器 ID |
| `count()` | `count()` | 待触发定时器数量 | 无 |
| `clearAll()` | `clearAll()` | 清理全部定时器 | 无 |
| `sleep(ms)` | `sleep(ms)` | 同步阻塞当前线程 | ms: 毫秒 |
