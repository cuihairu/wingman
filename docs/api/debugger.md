# API: wingman.debugger

> ⚠️ 调试器尚未实现，当前为 stub。`start` 恒返回 false。完整调试 API（断点管理/单步/求值/堆栈）规划中。

## 模块概述

debugger 模块目前仅有占位（stub）实现。源码注释明确标注 "debugger not yet implemented"。

当前仅提供以下 4 个 stub 函数，且不具备实际调试能力：

- `start()` — stub，恒返回 `false`
- `stop()` — 无操作占位
- `breakpoint(file, line)` — 返回形如 `"file:line"` 的字符串标识，实际断点由 IDE 设置
- `breakHere()` — 返回固定字符串 `"DEBUG_BREAK_HERE"`

:::warning
不要依赖本模块完成实际调试工作。未来版本将提供完整断点管理、单步执行、表达式求值、调用栈查询等能力。
:::

---

## 函数清单

| Python 函数 | Lua 函数 | 签名 | 说明 |
|------------|---------|------|------|
| `start()` | `start()` | `() -> bool` | stub，恒返回 `false` |
| `stop()` | `stop()` | `() -> nil` | stub，无操作 |
| `breakpoint(file, line)` | `breakpoint(file, line)` | `file:str, line:int -> str` | 返回 `"file:line"` 标识字符串 |
| `breakHere()` | `breakHere()` | `() -> str` | 返回固定字符串 `"DEBUG_BREAK_HERE"` |

---

## start()

**说明**：启动调试器（stub，未实现，恒返回 `false`）。

**函数签名**：

```python
start() -> bool
```

```lua
start() -> boolean
```

**返回**：
- 恒为 `false`（Python）/ `false`（Lua）

:::tabs

== Python

```python:line-numbers
from wingman import debugger

ok = debugger.start()
print(ok)  # False（未实现）
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local ok = wingman.debugger.start()
print(ok)  -- false（未实现）
```

:::

---

## stop()

**说明**：停止调试器（stub，无操作）。

**函数签名**：

```python
stop() -> None
```

```lua
stop() -> nil
```

:::tabs

== Python

```python:line-numbers
from wingman import debugger

debugger.stop()  # 无操作
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.debugger.stop()  -- 无操作
```

:::

---

## breakpoint(file, line)

**说明**：登记断点位置，返回形如 `"<file>:<line>"` 的字符串标识。实际断点由连接的 IDE 设置。

**函数签名**：

```python
breakpoint(file: str, line: int) -> str
```

```lua
breakpoint(file: string, line: number) -> string
```

**参数**：
- `file` - 脚本文件路径
- `line` - 行号

**返回**：
- 字符串 `"<file>:<line>"`

:::tabs

== Python

```python:line-numbers
from wingman import debugger

bp = debugger.breakpoint("main.py", 42)
print(bp)  # "main.py:42"
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local bp = wingman.debugger.breakpoint("main.lua", 42)
print(bp)  -- "main.lua:42"
```

:::

---

## breakHere()

**说明**：在当前位置标记断点（stub，仅返回固定字符串 `"DEBUG_BREAK_HERE"`）。

**函数签名**：

```python
breakHere() -> str
```

```lua
breakHere() -> string
```

**返回**：
- 固定字符串 `"DEBUG_BREAK_HERE"`

:::tabs

== Python

```python:line-numbers
from wingman import debugger

tag = debugger.breakHere()
print(tag)  # "DEBUG_BREAK_HERE"
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local tag = wingman.debugger.breakHere()
print(tag)  -- "DEBUG_BREAK_HERE"
```

:::
