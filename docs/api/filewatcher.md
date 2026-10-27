> [中文](../zh/api/filewatcher.md) | **English**

# API: wingman.filewatcher

File watcher module that monitors file/directory changes and invokes callbacks, letting scripts respond to file system events.

## Module Overview

The filewatcher module provides the following capabilities:

- **watch** - Start watching a specified path for changes, returns a registration ID
- **unwatch** - Stop watching a specified path
- **unwatchAll** - Stop all watches
- **isWatching** - Query whether a path is currently being watched
- **getWatchedPaths** - List all currently watched paths

**Implementation notes**:

- **Native backends**: Linux uses inotify, macOS uses FSEvents, Windows uses `ReadDirectoryChangesW`; changes are reported asynchronously from the platform backend thread.
- **Threading constraints**: Callbacks fire from the platform backend thread and **must be thread-safe callables** (Python functions qualify; Lua callables are not thread safe and are rejected with a `filewatcher.error` event — on the Lua side, register watches from Python instead).
- **Event surface**: Every change is also delivered as `filewatcher.changed` through `wingman.event` (source `"filewatcher"`) — likewise dispatched synchronously from the backend thread, so `event.on` script subscriptions are subject to the same thread-safety constraint.
- **Path bookkeeping**: The module records watches by registered path (`isWatching`/`getWatchedPaths` query this bookkeeping); calling `watch` again on the same path unregisters the old watch before registering the new one (replacement semantics).
- **Unified event sources**: Process/window changes come from `wingman.systemwatch`; file changes come from this module.

---

## watch

### watch(path, callback, recursive?) / watch(path, callback, recursive?)

**Description**: Starts watching a specified path for file changes. `path` must be a string and `callback` must be callable; returns `0` when the callback is a non-thread-safe callable or native backend registration fails. `recursive` defaults to `true` (recurse into subdirectories); non-boolean values are treated as the default.

**Function signature**:

```python
watch(path: str, callback: Callable[[dict], None], recursive: bool = True) -> int
```

```lua
watch(path: string, callback: function, recursive?: boolean) -> integer
```

**Parameters**:
- `path` - File or directory path to watch
- `callback` - Callback invoked when files change; receives a payload object `{ "type": str, "path": str, "oldPath": str, "timestamp": int }`
  - `type` ∈ `"added"` / `"removed"` / `"modified"` / `"renamed_old"` / `"renamed_new"`
- `recursive` - Whether to watch subdirectories recursively (optional, default `true`)

**Returns**:
- `int`/`integer` - Registration ID (`0` = failure)

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

def on_change(event):
    print(event["type"], event["path"])

wid = filewatcher.watch("./data", on_change)
if wid == 0:
    print("注册失败（回调非线程安全或路径不可监听）")
```

== Lua

```lua:line-numbers
-- Lua callable 非线程安全，filewatcher.watch 在 Lua 侧会被拒绝
-- （返回 0）并发出 filewatcher.error 事件；文件监听请用 Python 脚本。
```

:::

---

## unwatch

### unwatch(path) / unwatch(path)

**Description**: Stops watching the specified path. Returns `false` if the path is not being watched; a repeated unwatch returns `false` the second time.

**Function signature**:

```python
unwatch(path: str) -> bool
```

```lua
unwatch(path: string) -> boolean
```

**Parameters**:
- `path` - File or directory path to stop watching

**Returns**:
- `bool`/`boolean` - `true` on successful unregistration, `false` otherwise

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

filewatcher.unwatch("./data")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.filewatcher.unwatch("./data")
```

:::

---

## unwatchAll

### unwatchAll() / unwatch_all()

**Description**: Stops all active watches and clears the bookkeeping (idempotent).

**Function signature**:

```python
unwatchAll() -> None
```

```lua
unwatchAll() -> nil
```

**Parameters**:
- None

**Returns**:
- None

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

filewatcher.unwatchAll()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.filewatcher.unwatchAll()
```

:::

---

## isWatching

### isWatching(path) / is_watching(path)

**Description**: Queries whether the specified path is currently being watched. `path` must be a string; otherwise returns `false`.

**Function signature**:

```python
isWatching(path: str) -> bool
```

```lua
isWatching(path: string) -> boolean
```

**Parameters**:
- `path` - File or directory path to query

**Returns**:
- `bool`/`boolean` - Whether the path is being watched

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

if filewatcher.isWatching("./data"):
    print("watching")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

if wingman.filewatcher.isWatching("./data") then
    print("watching")
end
```

:::

---

## getWatchedPaths

### getWatchedPaths() / get_watched_paths()

**Description**: Returns all paths currently being watched.

**Function signature**:

```python
getWatchedPaths() -> list[str]
```

```lua
getWatchedPaths() -> string[]
```

**Parameters**:
- None

**Returns**:
- `list[str]`/`string[]` - Array of watched paths

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

paths = filewatcher.getWatchedPaths()
for p in paths:
    print(p)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local paths = wingman.filewatcher.getWatchedPaths()
for _, p in ipairs(paths) do
    print(p)
end
```

:::

---

## Complete Example

### Python

```python
from wingman import filewatcher

def on_change(event):
    print("changed:", event["type"], event["path"])

# 开始监听（递归）
wid = filewatcher.watch("./data", on_change)

# 查询
print(filewatcher.isWatching("./data"))
print(filewatcher.getWatchedPaths())

# 停止监听
filewatcher.unwatch("./data")

# 停止所有
filewatcher.unwatchAll()
```

---

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `watch(path, callback, recursive?)` | Same name (rejected on the Lua side, see Module Overview) | Watch a path, returns a registration ID | path: path to watch<br>callback: change callback (must be thread safe)<br>recursive: whether to recurse (default true) |
| `unwatch(path)` | `unwatch(path)` | Stop watching | path: path to stop watching |
| `unwatchAll()` | `unwatchAll()` | Stop all watches | None |
| `isWatching(path)` | `isWatching(path)` | Whether it is being watched | path: path to query |
| `getWatchedPaths()` | `getWatchedPaths()` | List watched paths | None |

**Events**:

| Event | Payload | Description |
|-----|------|-----|
| `filewatcher.changed` | `{type, path, oldPath, timestamp}` | File change (added/removed/modified/renamed_old/renamed_new) |
| `filewatcher.error` | `{error}` | Registration rejected (non-thread-safe callable) |
