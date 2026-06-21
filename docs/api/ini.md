# ini 模块

ini 模块提供 INI 配置文件格式的解析和编码功能。

INI 格式是一种简单的配置文件格式，广泛用于 Windows 应用程序和各种配置场景。

## 格式规范

INI 文件由 section 和 key-value 对组成：

```ini
; 注释以 ; 或 # 开头

[section_name]
key1 = value1
key2 = value2

; 另一个 section
[section2]
key3 = value3
```

**语法规则：**
- 注释以 `;` 或 `#` 开头
- Section 名称用 `[` 和 `]` 包围
- Key-value 对使用 `=` 分隔
- 空行被忽略
- Key 和 value 前后的空格会被自动去除
- 值中的特殊字符支持转义（`\n`, `\r`, `\t`, `\\`）

## API 函数

### ini.decode(content)

解析 INI 格式字符串。

**参数：**
- `content` (string): INI 格式的内容

**返回：**
- object: 解析后的数据 `{section: {key: value}}`

**示例：**

```python
from wingman import ini

content = """
; Server configuration
[Server]
host = localhost
port = 8080
debug = true

[Database]
host = db.example.com
port = 5432
"""

data = ini.decode(content)
# data = {
#     "Server": {"host": "localhost", "port": "8080", "debug": "true"},
#     "Database": {"host": "db.example.com", "port": "5432"}
# }
```

```lua
local wingman = require("wingman")

local content = [[
[Server]
host = localhost
port = 8080
debug = true
]]

local data = wingman.ini.decode(content)
```

### ini.encode(data)

将数据编码为 INI 格式字符串。

**参数：**
- `data` (object): INI 数据 `{section: {key: value}}`

**返回：**
- string: INI 格式字符串

**示例：**

```python
from wingman import ini

data = {
    "Server": {
        "host": "localhost",
        "port": "8080"
    },
    "Database": {
        "host": "db.example.com",
        "port": "5432"
    }
}

content = ini.encode(data)
print(content)
```

```lua
local wingman = require("wingman")

local data = {
    Server = {
        host = "localhost",
        port = "8080"
    }
}

local content = wingman.ini.encode(data)
print(content)
```

### ini.get(data, section, key?)

获取配置值。

**参数：**
- `data` (object): INI 数据
- `section` (string): section 名称
- `key` (string?, optional): key 名称

**返回：**
- 如果提供 `key`：返回对应值（字符串）或 `null`
- 如果不提供 `key`：返回整个 section（对象）或 `null`

**示例：**

```python
# 获取整个 section
server = ini.get(data, "Server")

# 获取特定值
host = ini.get(data, "Server", "host")
port = ini.get(data, "Server", "port")
```

```lua
-- 获取整个 section
local server = ini.get(data, "Server")

-- 获取特定值
local host = ini.get(data, "Server", "host")
local port = ini.get(data, "Server", "port")
```

### ini.set(data, section, key, value)

设置配置值。

**参数：**
- `data` (object): INI 数据（会被修改）
- `section` (string): section 名称
- `key` (string): key 名称
- `value` (string): 新值

**返回：**
- string: 设置的值

**示例：**

```python
ini.set(data, "Server", "host", "192.168.1.1")
ini.set(data, "Server", "port", "9000")
```

```lua
ini.set(data, "Server", "host", "192.168.1.1")
ini.set(data, "Server", "port", "9000")
```

### ini.delete(data, section, key?)

删除配置。

**参数：**
- `data` (object): INI 数据（会被修改）
- `section` (string): section 名称
- `key` (string?, optional): key 名称

**返回：**
- boolean: 是否成功删除

**示例：**

```python
# 删除整个 section
ini.delete(data, "OldSection")

# 删除特定 key
ini.delete(data, "Server", "debug")
```

```lua
-- 删除整个 section
ini.delete(data, "OldSection")

-- 删除特定 key
ini.delete(data, "Server", "debug")
```

### ini.has_section(data, section)

检查 section 是否存在。

**参数：**
- `data` (object): INI 数据
- `section` (string): section 名称

**返回：**
- boolean: 是否存在

**示例：**

```python
if ini.has_section(data, "Server"):
    print("Server configuration exists")
```

```lua
if ini.has_section(data, "Server") then
    print("Server configuration exists")
end
```

### ini.has_key(data, section, key)

检查 key 是否存在。

**参数：**
- `data` (object): INI 数据
- `section` (string): section 名称
- `key` (string): key 名称

**返回：**
- boolean: 是否存在

**示例：**

```python
if ini.has_key(data, "Server", "host"):
    print("Server host is configured")
```

```lua
if ini.has_key(data, "Server", "host") then
    print("Server host is configured")
end
```

### ini.sections(data)

获取所有 section 名称。

**参数：**
- `data` (object): INI 数据

**返回：**
- array: section 名称列表

**示例：**

```python
for section in ini.sections(data):
    print(f"Section: {section}")
```

```lua
for _, section in ipairs(ini.sections(data)) do
    print("Section: " .. section)
end
```

### ini.keys(data, section)

获取指定 section 的所有 key 名称。

**参数：**
- `data` (object): INI 数据
- `section` (string): section 名称

**返回：**
- array: key 名称列表

**示例：**

```python
for key in ini.keys(data, "Server"):
    value = ini.get(data, "Server", key)
    print(f"{key} = {value}")
```

```lua
for _, key in ipairs(ini.keys(data, "Server")) do
    local value = ini.get(data, "Server", key)
    print(key .. " = " .. value)
end
```

### ini.merge(base, override)

合并两个 INI 数据。

**参数：**
- `base` (object): 基础数据
- `override` (object): 覆盖数据

**返回：**
- object: 合并后的数据

**示例：**

```python
# 默认配置
default_config = ini.decode("""
[Server]
host = localhost
port = 8080
debug = false
""")

# 用户配置
user_config = ini.decode("""
[Server]
port = 9000
debug = true
""")

# 合并（用户配置覆盖默认配置）
config = ini.merge(default_config, user_config)
# 结果：port=9000, debug=true, host=localhost（保留）
```

```lua
local default_config = ini.decode([[
[Server]
host = localhost
port = 8080
]])

local user_config = ini.decode([[
[Server]
port = 9000
]])

local config = ini.merge(default_config, user_config)
```

## 完整示例

### Python

```python
from wingman import ini

# 读取配置文件
with open("config.ini", "r", encoding="utf-8") as f:
    content = f.read()

# 解析配置
config = ini.decode(content)

# 读取配置
server_host = ini.get(config, "Server", "host")
server_port = ini.get(config, "Server", "port")

# 修改配置
ini.set(config, "Server", "host", "192.168.1.100")
ini.set(config, "Server", "port", "9000")

# 删除配置
ini.delete(config, "Server", "debug")

# 保存配置
with open("config.ini", "w", encoding="utf-8") as f:
    f.write(ini.encode(config))

# 遍历所有配置
for section in ini.sections(config):
    print(f"[{section}]")
    for key in ini.keys(config, section):
        value = ini.get(config, section, key)
        print(f"{key} = {value}")
```

### Lua

```lua
local wingman = require("wingman")

-- 读取配置文件
local file = io.open("config.ini", "r")
local content = file:read("*a")
file:close()

-- 解析配置
local config = wingman.ini.decode(content)

-- 读取配置
local server_host = wingman.ini.get(config, "Server", "host")
local server_port = wingman.ini.get(config, "Server", "port")

-- 修改配置
wingman.ini.set(config, "Server", "host", "192.168.1.100")
wingman.ini.set(config, "Server", "port", "9000")

-- 删除配置
wingman.ini.delete(config, "Server", "debug")

-- 保存配置
local file = io.open("config.ini", "w")
file:write(wingman.ini.encode(config))
file:close()

-- 遍历所有配置
for _, section in ipairs(wingman.ini.sections(config)) do
    print("[" .. section .. "]")
    for _, key in ipairs(wingman.ini.keys(config, section)) do
        local value = wingman.ini.get(config, section, key)
        print(key .. " = " .. value)
    end
end
```

## 转义序列

值中的特殊字符支持转义：

| 转义 | 实际字符 |
|------|---------|
| `\n` | 换行 |
| `\r` | 回车 |
| `\t` | 制表符 |
| `\\` | 反斜杠 |

**示例：**

```ini
[Message]
text = Hello\nWorld
path = C:\\Users\\Admin
```

解析后：
- `text` = "Hello\nWorld"（包含换行符）
- `path` = "C:\Users\Admin"

## 最佳实践

### 1. 配置文件命名

- 应用配置：`config.ini` 或 `settings.ini`
- 用户配置：`user.ini` 或 `preferences.ini`
- 临时配置：`temp.ini`

### 2. Section 命名

- 使用大写或 PascalCase：`Server`, `Database`
- 使用清晰的命名：`Logging` 而不是 `Log`

### 3. Key 命名

- 使用小写加下划线：`host`, `port`, `max_connections`
- 或使用小驼峰：`hostName`, `portNumber`

### 4. 注释使用

```ini
; 数据库配置
[Database]
host = localhost     # 数据库主机
port = 5432         # 数据库端口
username = admin    # 用户名
```

### 5. 配置合并

推荐使用 `ini.merge` 实现配置分层：

```python
# 默认配置
default_config = ini.decode(default_config_content)

# 环境配置
env_config = ini.decode(env_config_content)

# 用户配置
user_config = ini.decode(user_config_content)

# 逐层合并
config = ini.merge(default_config, env_config)
config = ini.merge(config, user_config)
```

## 注意事项

1. **编码**
   - 推荐使用 UTF-8 编码
   - 读写文件时显式指定编码

2. **大小写敏感**
   - Section 和 Key 名称区分大小写

3. **类型转换**
   - INI 中的所有值都是字符串
   - 需要手动转换为数字或布尔值

4. **重复 Key**
   - 如果 INI 文件中有重复的 key，后出现的值会覆盖前面的值

5. **特殊字符**
   - Key 名称不应包含 `[`, `]`, `=`, `;`, `#`
   - Section 名称不应包含 `]`
