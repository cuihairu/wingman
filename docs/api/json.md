# JSON 模块

JSON 解析和序列化。

## Lua API

### json.decode(str)

解析 JSON 字符串。

**参数：**
- `str` (string) - JSON 字符串

**返回：**
- `value` - 解析后的 Lua 值（table、string、number、boolean、nil）

**示例：**
```lua
local data = json.decode('{"name": "Player1", "score": 100}')
print(data.name)      -- "Player1"
print(data.score)     -- 100
```

### json.encode(value, indent)

序列化为 JSON 字符串。

**参数：**
- `value` - Lua 值
- `indent` (number, 可选) - 缩进空格数，-1 表示压缩

**返回：**
- `str` (string) - JSON 字符串

**示例：**
```lua
local obj = {
    name = "Player1",
    score = 100,
    items = {"sword", "shield"}
}

local compressed = json.encode(obj)           -- 压缩格式
local formatted = json.encode(obj, 2)         -- 格式化，2 空格缩进
```

### json.null()

返回 JSON null 值（在 Lua 中表示为 nil）。

**示例：**
```lua
local obj = {
    name = "Player1",
    nickname = json.null()  -- JSON null
}
```
