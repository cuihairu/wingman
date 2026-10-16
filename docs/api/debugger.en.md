> [中文](debugger.md) | **English**

# API: wingman.debugger

> [The debugger is not yet implemented; the current version is a stub. `start` always returns false. A complete debugging API (breakpoint management/stepping/evaluation/stack inspection) is planned.]

## Module Overview

The debugger module currently has only a placeholder (stub) implementation. The source comments explicitly mark it as "debugger not yet implemented".

It currently provides only the following 4 stub functions, with no actual debugging capability:

- `start()` — stub, always returns `false`
- `stop()` — no-op placeholder
- `breakpoint(file, line)` — returns a string identifier of the form `"file:line"`; actual breakpoints are set by the IDE
- `breakHere()` — returns the fixed string `"DEBUG_BREAK_HERE"`

:::warning
Do not rely on this module for actual debugging work. Future versions will provide full breakpoint management, stepping, expression evaluation, call stack inspection, and other capabilities.
:::

---

## Function List

| Python function | Lua function | Signature | Description |
|------------|---------|------|------|
| `start()` | `start()` | `() -> bool` | stub, always returns `false` |
| `stop()` | `stop()` | `() -> nil` | stub, no-op |
| `breakpoint(file, line)` | `breakpoint(file, line)` | `file:str, line:int -> str` | returns a `"file:line"` identifier string |
| `breakHere()` | `breakHere()` | `() -> str` | returns the fixed string `"DEBUG_BREAK_HERE"` |

---

## start()

**Description**: Starts the debugger (stub, not implemented, always returns `false`).

**Function signature**:

```python
start() -> bool
```

```lua
start() -> boolean
```

**Returns**:
- Always `false` (Python) / `false` (Lua)

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

**Description**: Stops the debugger (stub, no-op).

**Function signature**:

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

**Description**: Registers a breakpoint location and returns a string identifier of the form `"<file>:<line>"`. Actual breakpoints are set by the connected IDE.

**Function signature**:

```python
breakpoint(file: str, line: int) -> str
```

```lua
breakpoint(file: string, line: number) -> string
```

**Parameters**:
- `file` - script file path
- `line` - line number

**Returns**:
- The string `"<file>:<line>"`

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

**Description**: Marks a breakpoint at the current location (stub, only returns the fixed string `"DEBUG_BREAK_HERE"`).

**Function signature**:

```python
breakHere() -> str
```

```lua
breakHere() -> string
```

**Returns**:
- The fixed string `"DEBUG_BREAK_HERE"`

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
