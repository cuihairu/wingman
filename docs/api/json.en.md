> [中文](json.md) | **English**

# API: wingman.json

JSON parsing and serialization module.

## Module Overview

The json module provides parsing and serialization for the JSON format:
- **Parse JSON** - convert a JSON string into a native object
- **Serialize JSON** - convert a native object into a JSON string
- **Formatted output** - pretty output with indentation control

Python's `None` and Lua's `nil` serialize to JSON `null` through `encode`.

---

## Parse a JSON String

### decode(str) / decode(str)

**Description**: Parses a JSON string and converts it into a native object.

**Signature**:

```python
decode(str: str) -> Any
```

```lua
decode(str: string) -> any
```

**Parameters**:
- `str` - a string in JSON format

**Returns**:
- Python: the parsed value (`dict`/`list`/`str`/`int`/`float`/`bool`/`None`)
- Lua: the parsed value (`table`/`string`/`number`/`boolean`/`nil`)

**On parse failure**:
- Returns `None` (Python) / `nil` (Lua); no exception is thrown
- Note: because JSON `null` itself also parses to `None`/`nil`, the return value alone cannot distinguish "the parsed result is null" from "parsing failed"

:::tabs

== Python

```python:line-numbers
from wingman import json

# 解析 JSON 字符串
data = json.decode('{"name": "Player1", "score": 100}')
print(data['name'])      # "Player1"
print(data['score'])     # 100

# 解析数组
items = json.decode('["sword", "shield", "potion"]')
for item in items:
    print(item)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 解析 JSON 字符串
local data = wingman.json.decode('{"name": "Player1", "score": 100}')
print(data.name)      -- "Player1"
print(data.score)     -- 100

-- 解析数组
local items = wingman.json.decode('["sword", "shield", "potion"]')
for i, item in ipairs(items) do
    print(item)
end
```

:::

---

## Serialize to a JSON String

### encode(value, indent?) / encode(value, indent?)

**Description**: Serializes a native object into a JSON string.

**Signature**:

```python
encode(value: Any, indent: int = -1) -> str
```

```lua
encode(value: any, indent: number = -1) -> string
```

**Parameters**:
- `value` - the value to serialize (dict, list, or JSON-compatible types)
- `indent` - optional, number of indent spaces
  - `-1` or omitted: compact format (no spaces)
  - `0` or any other value: indent with the given number of spaces

**Returns**:
- a string in JSON format

:::tabs

== Python

```python:line-numbers
from wingman import json

obj = {
    "name": "Player1",
    "score": 100,
    "items": ["sword", "shield"]
}

# 压缩格式
compressed = json.encode(obj)
# {"name":"Player1","score":100,"items":["sword","shield"]}

# 格式化，2 空格缩进（模块函数只接受位置参数）
formatted = json.encode(obj, 2)
print(formatted)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local obj = {
    name = "Player1",
    score = 100,
    items = {"sword", "shield"}
}

-- 压缩格式
local compressed = wingman.json.encode(obj)
-- {"name":"Player1","score":100,"items":["sword","shield"]}

-- 格式化，2 空格缩进
local formatted = wingman.json.encode(obj, 2)
print(formatted)
```

:::

---

## Available Interfaces

| Python Function | Lua Function | Description | Parameters |
|------------|---------|------|-----|
| `decode(str)` | `decode(str)` | Parse a JSON string | str: JSON string<br>Returns: native object; None/nil on parse failure |
| `encode(value, indent?)` | `encode(value, indent?)` | Serialize to JSON | value: native object<br>indent: number of indent spaces (default -1 = compact)<br>Returns: JSON string |
