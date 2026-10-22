> [中文](timer.md) | **English**

# API: wingman.timer

Timer module providing one-shot/periodic timed callbacks and synchronous sleep, used to schedule delayed work in scripts.

## Module Overview

The timer module provides the following capabilities:

- **after / setTimeout** - One-shot timer: fires the callback once when due
- **every / setInterval** - Periodic timer: fires repeatedly at an interval until canceled
- **clearTimer / cancel / clearTimeout / clearInterval** - Cancel timers
- **exists / count** - Query pending timers
- **clearAll** - Clear all pending timers
- **sleep** - Synchronously block the current script thread

> **Note**: Callbacks run on a background timer thread (not the script's main thread). Python callbacks are GIL-wrapped and safe
> across threads; Lua callbacks are not thread safe — matching the existing exposure of `event.on` + background-thread emits — so in
> Lua scripts stick to simple flag/queue operations or assess thread safety yourself. When the script engine shuts down, it clears **all** pending timers in the current process (TimerService is a process-wide singleton, including timers created by other scripts/runtimes), not just this script's.
> On the Python side, multi-word functions are exposed in both camelCase (`clearTimer`) and snake_case (`clear_timer`) forms; examples below use camelCase.

---

## after

### after(ms, callback)

**Description**: Creates a one-shot timer that fires `callback` once on the timer thread after `ms` milliseconds. `ms` values below 0 are treated as 0.

**Function signature**:

```python
after(ms: int, callback: Callable[[], Any]) -> int
```

```lua
after(ms: number, callback: function) -> number
```

**Parameters**:
- `ms` - Delay in milliseconds
- `callback` - Callback fired when due (receives no arguments)

**Returns**:
- `int`/`number` - timerId (returns 0 for invalid arguments)

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

**Description**: Creates a periodic timer that fires `callback` once every `ms` milliseconds until clearTimer/clearAll is called or the script ends. If callback execution falls too far behind, the base point resets to avoid a catch-up storm.

**Function signature**:

```python
every(ms: int, callback: Callable[[], Any]) -> int
```

```lua
every(ms: number, callback: function) -> number
```

**Parameters**:
- `ms` - Firing interval in milliseconds. `ms <= 0` is not an error and is treated as a **1ms** interval
- `callback` - Callback fired periodically (receives no arguments)

**Returns**:
- `int`/`number` - timerId (returns 0 only when arguments are missing or the callback is not callable)

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

**Description**: Cancels a one-shot or periodic timer. Returns false if it already fired or does not exist.

**Function signature**:

```python
clearTimer(timerId: int) -> bool
```

```lua
clearTimer(timerId: number) -> boolean
```

**Parameters**:
- `timerId` - Timer ID returned by after/every

**Returns**:
- `bool`/`boolean` - Whether the cancellation succeeded

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

**Description**: Queries whether a given timer is still pending; queries the number of currently pending timers.

**Function signature**:

```python
exists(timerId: int) -> bool
count() -> int
```

```lua
exists(timerId: number) -> boolean
count() -> number
```

**Returns**:
- `bool`/`boolean` - Whether it exists
- `int`/`number` - Number of pending timers

---

## clearAll

### clearAll()

**Description**: Clears all pending timers.

**Function signature**:

```python
clearAll() -> int
```

```lua
clearAll() -> number
```

**Returns**:
- `int`/`number` - Number of timers cleared

---

## sleep

### sleep(ms)

**Description**: Synchronously blocks the current script thread for `ms` milliseconds (negative values are treated as 0). Note that this blocks the calling thread itself;
for a non-blocking delay, use `after` instead.

**Function signature**:

```python
sleep(ms: int) -> None
```

```lua
sleep(ms: number) -> nil
```

---

## Complete Example

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

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `after(ms, callback)` | `after(ms, callback)` | One-shot timer | ms: milliseconds<br>callback: callback |
| `setTimeout(ms, callback)` | `setTimeout(ms, callback)` | JS-style alias of after | Same as after |
| `every(ms, callback)` | `every(ms, callback)` | Periodic timer | ms: interval<br>callback: callback |
| `setInterval(ms, callback)` | `setInterval(ms, callback)` | JS-style alias of every | Same as every |
| `clearTimer(timerId)` | `clearTimer(timerId)` | Cancel a timer | timerId: timer ID |
| `cancel(timerId)` | `cancel(timerId)` | Alias of clearTimer | Same as clearTimer |
| `clearTimeout(timerId)` | `clearTimeout(timerId)` | JS-style alias of clearTimer | Same as clearTimer |
| `clearInterval(timerId)` | `clearInterval(timerId)` | JS-style alias of clearTimer | Same as clearTimer |
| `exists(timerId)` | `exists(timerId)` | Whether still pending | timerId: timer ID |
| `count()` | `count()` | Number of pending timers | None |
| `clearAll()` | `clearAll()` | Clear all timers | None |
| `sleep(ms)` | `sleep(ms)` | Synchronously block the current thread | ms: milliseconds |
