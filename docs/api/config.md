> [中文](config.md) | **English**

# API: wingman.config

Configuration management module, used to read and write the application configuration.

## Module Overview

The config module provides configuration read/write functionality, supporting:
- **Custom configuration** - store user-defined key-value pairs
- **Persistence** - save the configuration to disk and load it from disk

---

## Get a Custom Config Entry

### get(key) / get(key)

**Description**: Gets a custom configuration value.

**Function signature**:

```python
get(key: str) -> Any
```

```lua
get(key: string) -> any
```

**Parameters**:
- `key` - the config entry key name. Keys are top-level strings; dot-path nesting is not supported (passing `game.character` looks up a top-level key literally named that string)

**Returns**:
- The configuration value (a string), or `None`/`nil` when it does not exist

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

## Set a Custom Config Entry

### set(key, value) / set(key, value)

**Description**: Sets a custom configuration value.

**Function signature**:

```python
set(key: str, value: str) -> None
```

```lua
set(key: string, value: string) -> nil
```

**Parameters**:
- `key` - the config entry key name. Keys are top-level strings; dot-path nesting is not supported
- `value` - the configuration value, **strings only**. Non-string values (numbers, booleans, tables, etc.) are silently stored as an empty string; for non-string JSON types, pass a JSON string (a `json::parse` is performed internally, e.g. `config.set("count", "42")` stores the number 42)

:::tabs

== Python

```python:line-numbers
from wingman import config

# 简单键值（值必须为字符串）
config.set("myKey", "myValue")
config.set("count", "42")            # 存为 JSON 数字 42
config.set("enabled", "true")        # 存为 JSON 布尔 true
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 简单键值（值必须为字符串）
wingman.config.set("myKey", "myValue")
wingman.config.set("count", "42")            -- 存为 JSON 数字 42
wingman.config.set("enabled", "true")        -- 存为 JSON 布尔 true
```

:::

---

## Delete a Custom Config Entry

### remove(key) / remove(key)

**Description**: Deletes a custom configuration value.

**Function signature**:

```python
remove(key: str) -> bool
```

```lua
remove(key: string) -> boolean
```

**Parameters**:
- `key` - the config entry key name

**Returns**:
- Whether the deletion succeeded (returns `True`/`true` if the key exists, otherwise `False`/`false`)

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

## Save the Configuration

### save() / save()

**Description**: Saves the current configuration to disk.

**Function signature**:

```python
save() -> bool
```

```lua
save() -> boolean
```

**Returns**:
- bool: whether saving succeeded (the current implementation writes JSON directly, so it is always `True`/`true`)

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

## Load the Configuration

### load() / load()

**Description**: Loads the configuration from disk.

**Function signature**:

```python
load() -> bool
```

```lua
load() -> boolean
```

**Returns**:
- bool: whether loading succeeded (the current implementation reads JSON directly, so it is always `True`/`true`)

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

## Available Interfaces

| Python function | Lua function | Description | Parameters |
|------------|---------|------|-----|
| `get(key)` | `get(key)` | Get custom configuration | key: key name (top-level string, no nested paths)<br>Return: string, None/nil if not present |
| `set(key, value)` | `set(key, value)` | Set custom configuration | key: key name, value: strings only (non-strings are silently stored as an empty string; pass JSON strings for JSON types) |
| `remove(key)` | `remove(key)` | Delete custom configuration | key: key name |
| `save()` | `save()` | Save the configuration to disk | None |
| `load()` | `load()` | Load the configuration from disk | None |
