# API: wingman.json

JSON 解析和序列化。

## 解析 JSON

:::tabs

== Python

```python
from wingman import json

# 解析 JSON 字符串
data = json.decode('{"name": "Player1", "score": 100}')
print(data['name'])      # "Player1"
print(data['score'])     # 100
```

== Lua

```lua
local json = require("wingman.json")

-- 解析 JSON 字符串
local data = json.decode('{"name": "Player1", "score": 100}')
print(data.name)      -- "Player1"
print(data.score)     -- 100
```

:::

## 序列化 JSON

:::tabs

== Python

```python
from wingman import json

obj = {
    "name": "Player1",
    "score": 100,
    "items": ["sword", "shield"]
}

# 压缩格式
compressed = json.encode(obj)

# 格式化，2 空格缩进
formatted = json.encode(obj, indent=2)
print(formatted)
```

== Lua

```lua
local json = require("wingman.json")

local obj = {
    name = "Player1",
    score = 100,
    items = {"sword", "shield"}
}

-- 压缩格式
local compressed = json.encode(obj)

-- 格式化，2 空格缩进
local formatted = json.encode(obj, 2)
print(formatted)
```

:::

## JSON null 值

:::tabs

== Python

```python
from wingman import json

obj = {
    "name": "Player1",
    "nickname": json.null()  # JSON null
}
```

== Lua

```lua
local json = require("wingman.json")

local obj = {
    name = "Player1",
    nickname = json.null()  -- JSON null
}
```

:::

---

## 可用接口

### `decode(str)`

解析 JSON 字符串。

**参数：**
- `str` (string) - JSON 字符串

**返回：**
- 解析后的值（dict/list/string/number/boolean/None 或 nil）

### `encode(value, indent?)`

序列化为 JSON 字符串。

**参数：**
- `value` - 要序列化的值
- `indent` (number, 可选) - 缩进空格数，-1 表示压缩，None/nil 表示压缩

**返回：**
- `str` (string) - JSON 字符串

### `null()`

返回 JSON null 值（在 Python 中为 None，Lua 中为 nil）。

**返回：**
- `null` 值
