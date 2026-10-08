# API: wingman.json

JSON 解析和序列化模块。

## 模块概述

json 模块提供 JSON 格式的解析和序列化功能：
- **解析 JSON** - 将 JSON 字符串转换为原生对象
- **序列化 JSON** - 将原生对象转换为 JSON 字符串
- **格式化输出** - 支持缩进控制的美化输出

Python 侧的 `None`、Lua 侧的 `nil` 经 `encode` 序列化即为 JSON `null`。

---

## 解析 JSON 字符串

### decode(str) / decode(str)

**说明**：解析 JSON 字符串，转换为原生对象。

**函数签名**：

```python
decode(str: str) -> Any
```

```lua
decode(str: string) -> any
```

**参数**：
- `str` - JSON 格式的字符串

**返回**：
- Python: 解析后的值（`dict`/`list`/`str`/`int`/`float`/`bool`/`None`）
- Lua: 解析后的值（`table`/`string`/`number`/`boolean`/`nil`）

**解析失败**：
- 返回 `None`（Python）/ `nil`（Lua），不抛出异常
- 注意：由于 `null` 本身也解析为 `None`/`nil`，仅凭返回值无法区分「解析结果就是 null」与「解析失败」

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

## 序列化为 JSON 字符串

### encode(value, indent?) / encode(value, indent?)

**说明**：将原生对象序列化为 JSON 字符串。

**函数签名**：

```python
encode(value: Any, indent: int = -1) -> str
```

```lua
encode(value: any, indent: number = -1) -> string
```

**参数**：
- `value` - 要序列化的值（字典、列表、或 JSON 兼容类型）
- `indent` - 可选，缩进空格数
  - `-1` 或省略：压缩格式（无空格）
  - `0` 或其他：使用指定空格数缩进

**返回**：
- JSON 格式的字符串

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

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `decode(str)` | `decode(str)` | 解析 JSON 字符串 | str: JSON 字符串<br>返回: 原生对象；解析失败返回 None/nil |
| `encode(value, indent?)` | `encode(value, indent?)` | 序列化为 JSON | value: 原生对象<br>indent: 缩进空格数(默认-1压缩)<br>返回: JSON 字符串 |
