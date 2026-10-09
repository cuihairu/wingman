# API: wingman.filewatcher

文件监听模块，提供对文件/目录变更的监视与回调能力，用于在脚本中响应文件系统事件。

## 模块概述

filewatcher 模块提供以下能力：

- **watch** - 开始监听指定路径的变更，返回注册 ID
- **unwatch** - 停止监听指定路径
- **unwatchAll** - 停止所有监听
- **isWatching** - 查询某路径是否处于监听中
- **getWatchedPaths** - 列出当前所有被监听的路径

**实现要点**：

- **原生后端**：Linux 走 inotify、macOS 走 FSEvents、Windows 走 `ReadDirectoryChangesW`，变更由平台后端线程异步回调。
- **线程约束**：回调从平台后端线程触发，**必须是线程安全 callable**（Python 函数可以；Lua callable 非线程安全，会被拒绝并发出 `filewatcher.error` 事件——Lua 侧请改用 Python 注册监听）。
- **事件面**：每次变更同时以 `filewatcher.changed` 进 `wingman.event`（source `"filewatcher"`）——同样从后端线程同步分发，`event.on` 脚本订阅同受线程安全约束。
- **路径簿记**：模块按注册路径记录监听（`isWatching`/`getWatchedPaths` 据此查询）；同一路径重复 `watch` 会先注销旧观察再注册新的（替换语义）。
- **统一事件源**：进程/窗口变化由 `wingman.systemwatch` 提供，文件变化由本模块提供。

---

## watch

### watch(path, callback, recursive?) / watch(path, callback, recursive?)

**说明**：开始监听指定路径的文件变更。`path` 必须为字符串、`callback` 必须为可调用对象；回调为非线程安全 callable 或原生后端注册失败时返回 `0`。`recursive` 缺省为 `true`（递归子目录）；传非布尔值按缺省处理。

**函数签名**：

```python
watch(path: str, callback: Callable[[dict], None], recursive: bool = True) -> int
```

```lua
watch(path: string, callback: function, recursive?: boolean) -> integer
```

**参数**：
- `path` - 要监听的文件或目录路径
- `callback` - 文件变更时触发的回调函数，参数为载荷对象 `{ "type": str, "path": str, "oldPath": str, "timestamp": int }`
  - `type` ∈ `"added"` / `"removed"` / `"modified"` / `"renamed_old"` / `"renamed_new"`
- `recursive` - 是否递归监听子目录（可选，默认 `true`）

**返回**：
- `int`/`integer` - 注册 ID（`0` = 失败）

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

**说明**：停止监听指定路径。路径未被监听时返回 `false`，重复注销第二次返回 `false`。

**函数签名**：

```python
unwatch(path: str) -> bool
```

```lua
unwatch(path: string) -> boolean
```

**参数**：
- `path` - 要停止监听的文件或目录路径

**返回**：
- `bool`/`boolean` - 成功注销返回 `true`，否则返回 `false`

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

**说明**：停止所有正在监听的路径并清空簿记（幂等）。

**函数签名**：

```python
unwatchAll() -> None
```

```lua
unwatchAll() -> nil
```

**参数**：
- 无

**返回**：
- 无

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

**说明**：查询指定路径是否正在被监听。`path` 必须为字符串，否则返回 `false`。

**函数签名**：

```python
isWatching(path: str) -> bool
```

```lua
isWatching(path: string) -> boolean
```

**参数**：
- `path` - 要查询的文件或目录路径

**返回**：
- `bool`/`boolean` - 是否正在监听

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

**说明**：返回当前所有正在被监听的路径。

**函数签名**：

```python
getWatchedPaths() -> list[str]
```

```lua
getWatchedPaths() -> string[]
```

**参数**：
- 无

**返回**：
- `list[str]`/`string[]` - 监听路径数组

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

## 完整示例

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

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `watch(path, callback, recursive?)` | 同名（Lua 侧拒绝，见模块概述） | 监听路径，返回注册 ID | path: 路径<br>callback: 变更回调（须线程安全）<br>recursive: 是否递归（默认 true） |
| `unwatch(path)` | `unwatch(path)` | 停止监听 | path: 路径 |
| `unwatchAll()` | `unwatchAll()` | 停止所有监听 | 无 |
| `isWatching(path)` | `isWatching(path)` | 是否在监听 | path: 路径 |
| `getWatchedPaths()` | `getWatchedPaths()` | 列出监听路径 | 无 |

**事件**：

| 事件 | 载荷 | 说明 |
|-----|------|-----|
| `filewatcher.changed` | `{type, path, oldPath, timestamp}` | 文件变更（added/removed/modified/renamed_old/renamed_new） |
| `filewatcher.error` | `{error}` | 注册被拒（非线程安全 callable） |
