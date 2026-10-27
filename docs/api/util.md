> [中文](../zh/api/util.md) | **English**

# API: wingman.util

Utility function module providing common helper functionality.

## Module Overview

The util module provides general-purpose utility functions:
- **Time functions** - get timestamps, delay execution
- **Log output** - print logs

---

## Delayed Execution

### sleep(milliseconds) / sleep(milliseconds)

**Description**: Delays execution for the given number of milliseconds.

**Signature**:

```python
sleep(milliseconds: int) -> None
```

```lua
sleep(milliseconds: number) -> nil
```

**Parameters**:
- `milliseconds` - the number of milliseconds to delay

**Returns**:
- None

:::tabs

== Python

```python:line-numbers
from wingman import util

# 延迟 1000 毫秒
util.sleep(1000)

# 延迟 2 秒
util.sleep(2000)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 延迟 1000 毫秒
wingman.util.sleep(1000)

-- 延迟 2 秒
wingman.util.sleep(2000)
```

:::

---

## Getting a Timestamp

### getTime() / getTime()

**Description**: Gets the current timestamp (in milliseconds).

**Signature**:

```python
getTime() -> int
```

```lua
getTime() -> number
```

**Returns**:
- the current timestamp (in milliseconds)

:::tabs

== Python

```python:line-numbers
from wingman import util

# 获取当前时间戳（毫秒）
timestamp = util.getTime()
print(f"当前时间戳: {timestamp}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取当前时间戳（毫秒）
local timestamp = wingman.util.getTime()
print("当前时间戳: " .. timestamp)
```

:::

---

## Log Output

### log(message) / log(message)

**Description**: Prints a log message to the console.

**Signature**:

```python
log(message: str) -> None
```

```lua
log(message: string) -> nil
```

**Parameters**:
- `message` - the log message

**Returns**:
- None

:::tabs

== Python

```python:line-numbers
from wingman import util

# 输出日志
util.log("这是一条日志信息")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 输出日志
wingman.util.log("这是一条日志信息")
```

:::

---

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `sleep(milliseconds)` | `sleep(milliseconds)` | Delay execution | milliseconds: number of milliseconds |
| `getTime()` | `getTime()` | Get a timestamp | Returns: timestamp in milliseconds |
| `log(message)` | `log(message)` | Print a log message | message: log message |
