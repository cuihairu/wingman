> [中文](hotkey.md) | **English**

# API: wingman.hotkey

Global hotkey module. Listens to global key state by polling and triggers the callback when the main key and its modifiers are pressed together (rising edge).

## Module Overview

The hotkey module provides the following capabilities:

- **Register a global hotkey** - combo text + callback, returns a registration ID
- **Unregister a hotkey** - unregister by ID

**Implementation notes**:

- **Polling-based v1**: a background thread reads global keyboard state at a fixed interval (default 30ms) and fires the callback once on the rising edge (`Ctrl+Shift+A`-style combos, case-insensitive, modifier order irrelevant).
- **Threading constraint**: callbacks are invoked from the background polling thread, so they **must be thread-safe callables** (Python functions are fine; Lua callables are not thread-safe and are rejected with a `hotkey.error` event — on the Lua side use events/triggers instead).
- **Platform support**: Windows (`GetKeyState` — reads the key state of the calling thread's message queue; since the polling thread has no message pump, the semantics can drift from "global asynchronous key state") / Linux X11 (`XQueryKeymap`) / macOS (`CGEventSourceKeyState`, requires Accessibility permission; real-world behavior pending verification on a physical machine).
- **Known v1 limitations**: a key pressed and released within one polling interval (<30ms) may be missed.
- **Lifecycle**: the first registration automatically starts the polling thread; once unregistering empties the table, the thread self-checks and exits (it restarts automatically on the next registration).

---

## register(combo, callback)

### register(combo, callback) / register(combo, callback)

**Description**: Registers a global hotkey. Returns `0` when the combo fails to parse, the callback is empty, or the callback is not thread-safe.

**Signature**:

```python
register(combo: str, callback: Callable[[], None]) -> int
```

```lua
-- Lua callable 非线程安全，注册会被拒绝返回 0 并发出 hotkey.error 事件
register(combo: string, callback: function) -> int
```

**Parameters**:
- `combo` - Combo text: `modifier+…+main key`, e.g. `"Ctrl+Shift+A"`, `"Alt+F12"`, `"F9"` (a main key alone is also allowed). The modifiers `Ctrl`/`Shift`/`Alt` may appear in any order and are case-insensitive; the main key supports letters `A-Z`, digits `0-9`, `F1-F12`, and named keys such as `SPACE`/`ENTER`/`ESC`/`TAB`/`BACKSPACE`/`DELETE`/`INSERT`.
- `callback` - Trigger callback, takes no arguments.

**Returns**:
- `int` - registration ID (`0` = failure)

:::tabs

== Python

```python:line-numbers
from wingman import hotkey

def on_combo():
    print("hotkey pressed")

hid = hotkey.register("Ctrl+Shift+A", on_combo)
if hid == 0:
    print("注册失败（组合键非法或回调不可用）")
```

== Lua

```lua:line-numbers
-- Lua callable 非线程安全，hotkey.register 在 Lua 侧会被拒绝（返回 0）
-- 并发出 hotkey.error 事件；全局热键请用 Python 脚本注册。
```

:::

**Events**: emits `hotkey.error` with payload `{ "error": "..." }` when a registration is rejected (non-thread-safe callable).

---

## unregister(id)

### unregister(id) / unregister(id)

**Description**: Unregisters a hotkey by registration ID. Returns `false` if the ID does not exist.

**Signature**:

```python
unregister(id: int) -> bool
```

```lua
unregister(id: int) -> boolean
```

**Parameters**:
- `id` - the registration ID returned by `register`

**Returns**:
- `bool`/`boolean` - whether the operation succeeded

:::tabs

== Python

```python:line-numbers
from wingman import hotkey

hid = hotkey.register("Ctrl+Shift+A", lambda: print("hi"))
hotkey.unregister(hid)
```

== Lua

```lua:line-numbers
-- 见 register 说明：Lua 侧无法注册，故 unregister 通常无用武之地
```

:::

---

## Full Example

### Python

```python
from wingman import hotkey

def toggle_farm():
    # 切换任务状态（示例）
    print("toggle")

hid = hotkey.register("F9", toggle_farm)
if hid == 0:
    print("hotkey.error: 组合键非法或回调非线程安全")

# 脚本收尾时注销
hotkey.unregister(hid)
```

---

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `register(combo, callback)` | `register(combo, callback)` | Register a global hotkey (rejected on the Lua side, see Module Overview) | combo: combo text<br>callback: trigger callback (must be thread-safe) |
| `unregister(id)` | `unregister(id)` | Unregister by ID | id: the ID returned by register |
