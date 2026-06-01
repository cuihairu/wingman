# API: wingman.json

JSON 解析和序列化模块。

## 模块概述

json 模块提供 JSON 格式的解析和序列化功能：
- **解析 JSON** - 将 JSON 字符串转换为原生对象
- **序列化 JSON** - 将原生对象转换为 JSON 字符串
- **格式化输出** - 支持缩进控制的美化输出
- **null 值处理** - 提供统一的 null 值表示

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
- 抛出异常，包含错误信息

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
local json = require("wingman.json")

-- 解析 JSON 字符串
local data = json.decode('{"name": "Player1", "score": 100}')
print(data.name)      -- "Player1"
print(data.score)     -- 100

-- 解析数组
local items = json.decode('["sword", "shield", "potion"]')
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

# 格式化，2 空格缩进
formatted = json.encode(obj, indent=2)
print(formatted)
```

== Lua

```lua:line-numbers
local json = require("wingman.json")

local obj = {
    name = "Player1",
    score = 100,
    items = {"sword", "shield"}
}

-- 压缩格式
local compressed = json.encode(obj)
-- {"name":"Player1","score":100,"items":["sword","shield"]}

-- 格式化，2 空格缩进
local formatted = json.encode(obj, 2)
print(formatted)
```

:::

---

## 获取 null 值

### null() / null()

**说明**：获取 JSON null 值的表示。

**函数签名**：

```python
null() -> None
```

```lua
null() -> nil
```

**返回**：
- Python: `None`
- Lua: `nil`

**使用场景**：
- 在对象中表示显式的 null 值
- 区分"字段不存在"和"字段值为 null"

:::tabs

== Python

```python:line-numbers
from wingman import json

obj = {
    "name": "Player1",
    "nickname": json.null(),  # 显式设置为 null
    "score": 100
}

json_str = json.encode(obj)
# {"name":"Player1","nickname":null,"score":100}
```

== Lua

```lua:line-numbers
local json = require("wingman.json")

local obj = {
    name = "Player1",
    nickname = json.null(),  -- 显式设置为 null
    score = 100
}

local jsonStr = json.encode(obj)
-- {"name":"Player1","nickname":null,"score":100}
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `decode(str)` | `decode(str)` | 解析 JSON 字符串 | str: JSON 字符串<br>返回: 原生对象 |
| `encode(value, indent?)` | `encode(value, indent?)` | 序列化为 JSON | value: 原生对象<br>indent: 缩进空格数(默认-1压缩)<br>返回: JSON 字符串 |
| `null()` | `null()` | 获取 null 值 | 返回: None/nil |
