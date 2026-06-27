# API: wingman.config

配置管理模块，用于读写应用程序配置。

## 模块概述

config 模块提供配置的读写功能，支持：
- **自定义配置** - 存储用户自定义键值对
- **持久化** - 保存配置到磁盘、从磁盘加载配置

---

## 获取自定义配置

### get(key) / get(key)

**说明**：获取自定义配置值。

**函数签名**：

```python
get(key: str) -> Any
```

```lua
get(key: string) -> any
```

**参数**：
- `key` - 配置键名。键为顶层字符串，不支持点号路径嵌套（传 `game.character` 会查找字面名为该串的顶层键）

**返回**：
- 配置值，不存在时返回 `None`/`nil`

:::tabs

== Python

```python:line-numbers
from wingman import config

value = config.get("myKey")
if value:
    print(value)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local value = wingman.config.get("myKey")
if value then
    print(value)
end
```

:::

---

## 设置自定义配置

### set(key, value) / set(key, value)

**说明**：设置自定义配置值。

**函数签名**：

```python
set(key: str, value: Any) -> None
```

```lua
set(key: string, value: any) -> nil
```

**参数**：
- `key` - 配置键名。键为顶层字符串，不支持点号路径嵌套
- `value` - 配置值（支持 JSON 兼容类型）

:::tabs

== Python

```python:line-numbers
from wingman import config

# 简单键值
config.set("myKey", "myValue")
config.set("count", 42)
config.set("enabled", True)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 简单键值
wingman.config.set("myKey", "myValue")
wingman.config.set("count", 42)
wingman.config.set("enabled", true)
```

:::

---

## 删除自定义配置

### remove(key) / remove(key)

**说明**：删除自定义配置值。

**函数签名**：

```python
remove(key: str) -> bool
```

```lua
remove(key: string) -> boolean
```

**参数**：
- `key` - 配置键名

**返回**：
- 是否成功删除（键存在返回 `True`/`true`，否则返回 `False`/`false`）

:::tabs

== Python

```python:line-numbers
from wingman import config

removed = config.remove("myKey")
if removed:
    print("配置已删除")
else:
    print("配置不存在")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local removed = wingman.config.remove("myKey")
if removed then
    print("配置已删除")
else
    print("配置不存在")
end
```

:::

---

## 保存配置

### save() / save()

**说明**：将当前配置保存到磁盘。

**函数签名**：

```python
save() -> None
```

```lua
save() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import config

# 修改后保存到磁盘
config.set("myKey", "myValue")
config.save()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 修改后保存到磁盘
wingman.config.set("myKey", "myValue")
wingman.config.save()
```

:::

---

## 加载配置

### load() / load()

**说明**：从磁盘加载配置。

**函数签名**：

```python
load() -> None
```

```lua
load() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import config

# 从磁盘重新加载配置
config.load()
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 从磁盘重新加载配置
wingman.config.load()
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `get(key)` | `get(key)` | 获取自定义配置 | key: 键名（顶层字符串，不支持嵌套路径） |
| `set(key, value)` | `set(key, value)` | 设置自定义配置 | key: 键名, value: 值 |
| `remove(key)` | `remove(key)` | 删除自定义配置 | key: 键名 |
| `save()` | `save()` | 保存配置到磁盘 | 无 |
| `load()` | `load()` | 从磁盘加载配置 | 无 |
