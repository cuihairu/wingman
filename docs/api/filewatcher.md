# API: wingman.filewatcher

文件监听模块，提供对文件/目录变更的监视与回调能力，用于在脚本中响应文件系统事件。

## 模块概述

filewatcher 模块提供以下能力：

- **watch** - 开始监听指定路径的变更
- **unwatch** - 停止监听指定路径
- **unwatchAll** - 停止所有监听
- **isWatching** - 查询某路径是否处于监听中
- **getWatchedPaths** - 列出当前所有被监听的路径

> **注意**：当前模块的脚本桥接实现为占位版本。`watch` 在参数校验通过后固定返回 `true`，`unwatch`/`unwatchAll` 仅清理解释层状态，`isWatching` 固定返回 `false`，`getWatchedPaths` 固定返回空数组。底层 `FileWatcher` 原生回调绑定尚未在脚本层接通，使用时请以代码实际行为为准。

---

## watch

### watch(path, callback) / watch(path, callback)

**说明**：开始监听指定路径的文件变更。`path` 必须为字符串、`callback` 必须为可调用对象，否则直接返回 `false`。

**函数签名**：

```python
watch(path: str, callback: Callable) -> bool
```

```lua
watch(path: string, callback: function) -> boolean
```

**参数**：
- `path` - 要监听的文件或目录路径
- `callback` - 文件变更时触发的回调函数

**返回**：
- `bool`/`boolean` - 参数校验通过返回 `true`，否则返回 `false`

:::tabs

== Python

```python:line-numbers
from wingman import filewatcher

def on_change(event):
    print("changed:", event)

filewatcher.watch("./data", on_change)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local function on_change(event)
    print("changed:", event)
end

wingman.filewatcher.watch("./data", on_change)
```

:::

---

## unwatch

### unwatch(path) / unwatch(path)

**说明**：停止监听指定路径。`path` 必须为字符串，否则返回 `false`。

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
- `bool`/`boolean` - 参数校验通过返回 `true`，否则返回 `false`

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

### unwatchAll() / unwatchAll()

**说明**：停止所有正在监听的路径。

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

### isWatching(path) / isWatching(path)

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
- `bool`/`boolean` - 是否正在监听（当前实现固定返回 `false`）

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

### getWatchedPaths() / getWatchedPaths()

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
- `list[str]`/`string[]` - 监听路径数组（当前实现固定返回空数组）

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
    print("changed:", event)

# 开始监听
filewatcher.watch("./data", on_change)

# 查询
print(filewatcher.isWatching("./data"))
print(filewatcher.getWatchedPaths())

# 停止监听
filewatcher.unwatch("./data")

# 停止所有
filewatcher.unwatchAll()
```

### Lua

```lua
local wingman = require("wingman")

local function on_change(event)
    print("changed:", event)
end

-- 开始监听
wingman.filewatcher.watch("./data", on_change)

-- 查询
print(wingman.filewatcher.isWatching("./data"))
print(wingman.filewatcher.getWatchedPaths())

-- 停止监听
wingman.filewatcher.unwatch("./data")

-- 停止所有
wingman.filewatcher.unwatchAll()
```

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `watch(path, callback)` | `watch(path, callback)` | 监听路径 | path: 路径<br>callback: 变更回调 |
| `unwatch(path)` | `unwatch(path)` | 停止监听 | path: 路径 |
| `unwatchAll()` | `unwatchAll()` | 停止所有监听 | 无 |
| `isWatching(path)` | `isWatching(path)` | 是否在监听 | path: 路径 |
| `getWatchedPaths()` | `getWatchedPaths()` | 列出监听路径 | 无 |
