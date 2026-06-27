# API: wingman.util

工具函数模块，提供常用的辅助功能。

## 模块概述

util 模块提供通用工具函数：
- **时间函数** - 获取时间戳、延迟执行
- **日志输出** - 输出日志

---

## 延迟执行

### sleep(milliseconds) / sleep(milliseconds)

**说明**：延迟执行指定毫秒数。

**函数签名**：

```python
sleep(milliseconds: int) -> None
```

```lua
sleep(milliseconds: number) -> nil
```

**参数**：
- `milliseconds` - 延迟的毫秒数

**返回**：
- 无

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

## 获取时间戳

### getTime() / getTime()

**说明**：获取当前时间戳（毫秒）。

**函数签名**：

```python
getTime() -> int
```

```lua
getTime() -> number
```

**返回**：
- 当前时间戳（毫秒）

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

## 输出日志

### log(message) / log(message)

**说明**：输出日志到控制台。

**函数签名**：

```python
log(message: str) -> None
```

```lua
log(message: string) -> nil
```

**参数**：
- `message` - 日志消息

**返回**：
- 无

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

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `sleep(milliseconds)` | `sleep(milliseconds)` | 延迟执行 | milliseconds: 毫秒数 |
| `getTime()` | `getTime()` | 获取时间戳 | 返回: 毫秒时间戳 |
| `log(message)` | `log(message)` | 输出日志 | message: 日志消息 |
