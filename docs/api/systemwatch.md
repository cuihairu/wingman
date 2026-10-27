> [中文](../zh/api/systemwatch.md) | **English**

# API: wingman.systemwatch

Unified system event source (process/window watch legs). Listens to process and window snapshot changes by polling and triggers callbacks when a registered target is hit.

## Module Overview

The systemwatch module provides the following capabilities:

- **processWatch** - watch process start/exit (exact name match, or all processes)
- **windowWatch** - watch window open/close/title changes (title substring match, or all windows)
- **unwatch** - unregister a watch by registration ID
- **clearAll** - clear all watches
- **watchCount** - query the current number of watches (optionally by category)

**Implementation notes**:

- **Polling-based v1**: a background thread snapshots `Process::enumerate()` and `Window::enumerate()` at a fixed interval (default 500ms) and diffs against the previous tick → the first tick only establishes a baseline and triggers nothing.
- **Threading constraint**: callbacks are invoked from the background polling thread, so they **must be thread-safe callables** (Python functions are fine; Lua callables are not thread-safe and are rejected with a `systemwatch.error` event — on the Lua side use events/triggers instead).
- **Event surface**: hit events are also fed into `wingman.event` as `systemwatch.process` / `systemwatch.window` (source `"systemwatch"`, payload carrying action/pid/name or action/handle/title) — likewise dispatched synchronously from the background thread, so script subscriptions via `event.on` are subject to the same thread-safety constraint.
- **Known v1 limitations**: a process/window that appears and disappears within one polling interval (< polling interval) may be missed (same pattern as hotkey); window events are grouped by handle, and a title change triggers `changed`.
- **Lifecycle**: the first registration automatically starts the polling thread; once unregistering empties the table, the thread self-checks and exits (it restarts automatically on the next registration).
- **File changes**: provided by `wingman.filewatcher`, not part of this module.

---

## processWatch(name, callback)

### processWatch(name, callback) / process_watch(name, callback)

**Description**: Watches processes. An empty `name` watches all processes; a non-empty name matches process names exactly (consistent with `process.find`). Returns `0` when the callback is empty or not thread-safe.

**Signature**:

```python
processWatch(name: str, callback: Callable[[dict], None]) -> int
```

**Parameters**:
- `name` - process name (e.g. `"sleep"`); empty string = all processes
- `callback` - change callback, receives the payload object `{ "action": "started"|"exited", "pid": int, "name": str }`

**Returns**:
- `int` - registration ID (`0` = failure)

:::tabs

== Python

```python:line-numbers
from wingman import systemwatch

def on_proc(ev):
    print(ev["action"], ev["pid"], ev["name"])

wid = systemwatch.processWatch("mygame", on_proc)
if wid == 0:
    print("注册失败（回调非线程安全）")
```

== Lua

```lua:line-numbers
-- Lua callable 非线程安全，systemwatch.processWatch 在 Lua 侧会被拒绝
-- （返回 0）并发出 systemwatch.error 事件；系统观察请用 Python 脚本。
```

:::

---

## windowWatch(title, callback)

### windowWatch(title, callback) / window_watch(title, callback)

**Description**: Watches windows. An empty `title` watches all windows; a non-empty title matches by title substring (consistent with `window.find`).

**Signature**:

```python
windowWatch(title: str, callback: Callable[[dict], None]) -> int
```

**Parameters**:
- `title` - window title substring; empty string = all windows
- `callback` - change callback, receives `{ "action": "opened"|"closed"|"changed", "handle": int, "title": str }` (`changed` = title changed on the same handle)

**Returns**:
- `int` - registration ID (`0` = failure)

---

## unwatch(id)

### unwatch(id) / unwatch(id)

**Description**: Unregisters a watch by registration ID (works for both the process and window categories). Returns `false` if the ID does not exist.

**Signature**:

```python
unwatch(id: int) -> bool
```

---

## clearAll()

### clearAll() / clear_all()

**Description**: Clears all watches (idempotent; the thread self-checks and exits once the table is empty).

**Signature**:

```python
clearAll() -> None
```

---

## watchCount(kind?)

### watchCount(kind?) / watch_count(kind?)

**Description**: Queries the current number of watches. `kind` accepts `"process"` / `"window"`; omitted, it returns the total; an unknown category returns `0`.

**Signature**:

```python
watchCount(kind: str | None = None) -> int
```

---

## Full Example

```python
from wingman import systemwatch

def on_window(ev):
    if ev["action"] == "closed":
        print("目标窗口已关闭")

wid = systemwatch.windowWatch("记事本", on_window)

# 脚本收尾时清理
systemwatch.clearAll()
```

---

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `processWatch(name, callback)` | same name | Watch process start/exit (rejected on the Lua side, see Module Overview) | name: process name (empty = all)<br>callback: callback (must be thread-safe) |
| `windowWatch(title, callback)` | same name | Watch window open/close/title changes | title: title substring (empty = all)<br>callback: callback (must be thread-safe) |
| `unwatch(id)` | `unwatch(id)` | Unregister by ID | id: registration ID |
| `clearAll()` | `clearAll()` | Clear all watches | - |
| `watchCount(kind?)` | `watchCount(kind?)` | Query the number of watches | kind: optional "process"/"window" |

**Events**:

| Event | Payload | Description |
|-----|------|-----|
| `systemwatch.process` | `{action, pid, name}` | Process started/exited |
| `systemwatch.window` | `{action, handle, title}` | Window opened/closed/changed |
| `systemwatch.error` | `{error}` | Registration rejected (non-thread-safe callable) |
