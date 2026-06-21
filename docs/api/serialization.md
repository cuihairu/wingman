# 序列化 API

Wingman 提供了多种数据序列化格式支持，用于配置文件、数据交换和持久化存储。

## 📋 目录

- [概述](#概述)
- [JSON 模块](#json-模块)
- [INI 模块](#ini-模块)
- [使用示例](#使用示例)
- [最佳实践](#最佳实践)

## 概述

### 可用的序列化格式

| 格式 | 适用场景 | 模块 | 特点 |
|------|----------|------|------|
| **JSON** | 数据交换、复杂结构 | `json` 模块 | 通用、可读性好、支持嵌套 |
| **INI** | 配置文件、简单设置 | `ini` 模块 | 简洁、人类友好、分区管理 |

### 选择合适的格式

- **使用 JSON** 当：
  - 数据结构复杂（嵌套对象、数组）
  - 需要与外部系统交换数据
  - 需要支持多种数据类型

- **使用 INI** 当：
  - 存储简单的配置项
  - 需要人工编辑配置文件
  - 数据结构扁平或简单分层

## JSON 模块

JSON (JavaScript Object Notation) 是一种轻量级的数据交换格式。

### Lua API

#### 编码（序列化）

```lua
local wingman = require("wingman")

-- 编码表
local data = {
    name = "player1",
    level = 10,
    inventory = {"sword", "shield", "potion"},
    stats = {
        hp = 100,
        mp = 50,
        strength = 15
    }
}

local json_string = wingman.json.encode(data)
print(json_string)
-- {"name":"player1","level":10,"inventory":["sword","shield","potion"],"stats":{"hp":100,"mp":50,"strength":15}}

-- 美化输出
local pretty_json = wingman.json.encode(data, true)
print(pretty_json)
-- {
--   "name": "player1",
--   "level": 10,
--   "inventory": ["sword", "shield", "potion"],
--   "stats": {
--     "hp": 100,
--     "mp": 50,
--     "strength": 15
--   }
-- }
```

#### 解码（反序列化）

```lua
local wingman = require("wingman")

local json_string = '{"name":"player1","level":10,"inventory":["sword","shield"]}'

-- 解码 JSON 字符串
local data = wingman.json.decode(json_string)

print(data.name)           -- "player1"
print(data.level)           -- 10
print(data.inventory[1])    -- "sword"

-- 遍历数组
for i, item in ipairs(data.inventory) do
    print(i, item)
end
```

#### 文件操作

```lua
local wingman = require("wingman")

-- 保存到文件
local data = {score = 1000, level = 5}
local json_string = wingman.json.encode(data, true)

local file = io.open("save_data.json", "w")
file:write(json_string)
file:close()

-- 从文件读取
local file = io.open("save_data.json", "r")
local json_string = file:read("*all")
file:close()

local data = wingman.json.decode(json_string)
print(data.score)  -- 1000
```

### Python API

#### 编码（序列化）

```python
import json

# 编码字典
data = {
    "name": "player1",
    "level": 10,
    "inventory": ["sword", "shield", "potion"],
    "stats": {
        "hp": 100,
        "mp": 50,
        "strength": 15
    }
}

json_string = json.encode(data)
print(json_string)
# {"name":"player1","level":10,"inventory":["sword","shield","potion"],"stats":{"hp":100,"mp":50,"strength":15}}

# 美化输出
pretty_json = json.encode(data, pretty=True)
print(pretty_json)
```

#### 解码（反序列化）

```python
from wingman import json

json_string = '{"name":"player1","level":10,"inventory":["sword","shield"]}'

# 解码 JSON 字符串
data = json.decode(json_string)

print(data["name"])              # "player1"
print(data["level"])              # 10
print(data["inventory"][0])       # "sword"

# 遍历数组
for item in data["inventory"]:
    print(item)
```

## INI 模块

INI 格式是一种简单的配置文件格式，常用于应用程序配置。

### Lua API

#### 解析 INI

```lua
local wingman = require("wingman")

local content = [[
; 服务器配置
[Server]
host = localhost
port = 8080
debug = true

; 数据库配置
[Database]
host = db.example.com
port = 5432
username = admin
password = secret
]]

local data = wingman.ini.decode(content)

-- 获取整个 section
local server = data["Server"]
print(server.host)    -- "localhost"
print(server.port)    -- "8080"
print(server.debug)  -- "true"

-- 或使用 get 方法
local host = wingman.ini.get(data, "Server", "host")
print(host)  -- "localhost"

-- 检查 section 是否存在
local has_server = wingman.ini.has_section(data, "Server")
print(has_server)  -- true

-- 检查 key 是否存在
local has_debug = wingman.ini.has_key(data, "Server", "debug")
print(has_debug)  -- true
```

#### 生成 INI

```lua
local wingman = require("wingman")

local data = {
    Server = {
        host = "localhost",
        port = "8080",
        debug = "true"
    },
    Database = {
        host = "db.example.com",
        port = "5432",
        username = "admin"
    }
}

local content = wingman.ini.encode(data)
print(content)
-- [Server]
-- host=localhost
-- port=8080
-- debug=true
--
-- [Database]
-- host=db.example.com
-- port=5432
-- username=admin
```

#### 修改 INI

```lua
local wingman = require("wingman")

local content = [[
[Server]
host = localhost
port = 8080
]]

local data = wingman.ini.decode(content)

-- 设置值
data = wingman.ini.set(data, "Server", "port", "9090")

-- 添加新的 section
data = wingman.ini.set(data, "Database", "host", "localhost")

-- 删除值
data = wingman.ini.delete(data, "Server", "debug")

-- 删除 section
data = wingman.ini.delete(data, "OldSection")

-- 获取所有 section
local sections = wingman.ini.sections(data)
for _, section in ipairs(sections) do
    print(section)
end

-- 获取 section 中的所有 key
local keys = wingman.ini.keys(data, "Server")
for _, key in ipairs(keys) do
    print(key)
end
```

#### 合并 INI

```lua
local wingman = require("wingman")

local default_config = [[
[Graphics]
resolution = 1920x1080
vsync = true
fullscreen = false

[Audio]
volume = 100
]]

local user_config = [[
[Graphics]
resolution = 2560x1440
fullscreen = true

[Audio]
volume = 50
]]

local default = wingman.ini.decode(default_config)
local user = wingman.ini.decode(user_config)

-- 合并配置（用户配置覆盖默认配置）
local merged = wingman.ini.merge(default, user)

print(merged.Graphics.resolution)  -- "2560x1440" (用户配置)
print(merged.Graphics.vsync)      -- "true" (默认配置)
print(merged.Audio.volume)         -- "50" (用户配置)
```

### Python API

#### 解析 INI

```python
from wingman import ini

content = """
[Server]
host = localhost
port = 8080
debug = true

[Database]
host = db.example.com
port = 5432
"""

data = ini.decode(content)

# 获取整个 section
server = data["Server"]
print(server["host"])    # "localhost"
print(server["port"])    # "8080"

# 或使用 get 方法
host = ini.get(data, "Server", "host")
print(host)  # "localhost"

# 检查 section 是否存在
has_server = ini.has_section(data, "Server")
print(has_server)  # True

# 检查 key 是否存在
has_debug = ini.has_key(data, "Server", "debug")
print(has_debug)  # True
```

#### 生成 INI

```python
from wingman import ini

data = {
    "Server": {
        "host": "localhost",
        "port": "8080",
        "debug": "true"
    },
    "Database": {
        "host": "db.example.com",
        "port": "5432"
    }
}

content = ini.encode(data)
print(content)
```

## 使用示例

### 游戏配置文件

#### Lua

```lua
local wingman = require("wingman")

-- 创建默认配置
local default_config = {
    Graphics = {
        resolution = "1920x1080",
        quality = "high",
        vsync = "true",
        fullscreen = "false"
    },
    Audio = {
        master = "100",
        music = "80",
        sfx = "100"
    },
    Gameplay = {
        difficulty = "normal",
        subtitles = "true",
        auto_save = "true"
    }
}

-- 保存配置
local config_content = wingman.ini.encode(default_config)

local file = io.open("config.ini", "w")
file:write(config_content)
file:close()

-- 加载配置
local file = io.open("config.ini", "r")
local config_content = file:read("*all")
file:close()

local config = wingman.ini.decode(config_content)

-- 使用配置
print(config.Graphics.resolution)
print(config.Audio.master)
```

### 游戏存档

#### Lua

```lua
local wingman = require("wingman")

-- 游戏状态
local game_state = {
    player = {
        name = "Hero",
        level = 10,
        experience = 5500,
        position = {x = 100.5, y = 200.3, z = 0},
        health = 85,
        mana = 60
    },
    inventory = {
        {id = 1, name = "Sword", count = 1, durability = 100},
        {id = 2, name = "Health Potion", count = 5},
        {id = 3, name = "Gold", count = 999}
    },
    quests = {
        {id = 1, name = "Dragon Slayer", status = "active", progress = 0.6},
        {id = 2, name = "Treasure Hunter", status = "completed", progress = 1.0}
    },
    world_state = {
        time = 12.5,
        weather = "sunny",
        visited_areas = {"forest", "cave", "village"}
    },
    timestamp = os.time()
}

-- 保存存档
local save_data = wingman.json.encode(game_state, true)

local file = io.open("save_001.json", "w")
file:write(save_data)
file:close()

-- 加载存档
local file = io.open("save_001.json", "r")
local save_data = file:read("*all")
file:close()

local loaded_state = wingman.json.decode(save_data)

-- 继续游戏
print("欢迎回来, " .. loaded_state.player.name)
print("等级: " .. loaded_state.player.level)
print("位置: " .. loaded_state.player.position.x .. ", " .. loaded_state.player.position.y)
```

### 配置合并策略

#### Lua

```lua
local wingman = require("wingman")

-- 系统默认配置
local system_config = wingman.ini.decode([[
[Graphics]
resolution = 1920x1080
quality = medium
vsync = true
fullscreen = false
antialiasing = fxaa

[Audio]
master = 100
music = 80
sfx = 100
voice = 100
]])

-- 用户配置
local user_config = wingman.ini.decode([[
[Graphics]
resolution = 2560x1440
quality = ultra
fullscreen = true

[Audio]
master = 50
music = 40
]])

-- 运行时配置（最高优先级）
local runtime_config = wingman.ini.decode([[
[Graphics]
vsync = false
]])

-- 按优先级合并
local final_config = wingman.ini.merge(system_config, user_config)
final_config = wingman.ini.merge(final_config, runtime_config)

-- 结果：
-- Graphics.resolution = "2560x1440" (用户配置)
-- Graphics.quality = "ultra" (用户配置)
-- Graphics.vsync = "false" (运行时配置)
-- Graphics.fullscreen = "true" (用户配置)
-- Graphics.antialiasing = "fxaa" (系统默认)
-- Audio.master = "50" (用户配置)
-- Audio.music = "40" (用户配置)
-- Audio.sfx = "100" (系统默认)
```

## 最佳实践

### 1. 配置文件命名

```lua
-- ✅ 好的做法
local config = ini.decode(file_content)

-- ❌ 避免硬编码路径
local config = ini.decode(io.open("C:\\Game\\config.ini"):read("*all"))

-- ✅ 使用相对路径或配置目录
local config_path = "config/settings.ini"
```

### 2. 错误处理

```lua
local wingman = require("wingman")

-- 读取 JSON 时处理错误
local ok, data = pcall(function()
    local file = io.open("data.json", "r")
    local content = file:read("*all")
    file:close()
    return wingman.json.decode(content)
end)

if not ok then
    print("JSON 解析失败:", data)
    -- 使用默认值
    data = {score = 0, level = 1}
end
```

### 3. 配置验证

```lua
local wingman = require("wingman")

-- 加载配置
local config = wingman.ini.decode(config_content)

-- 验证必需的配置项
local required_keys = {
    {section = "Server", key = "host"},
    {section = "Server", key = "port"},
    {section = "Database", key = "host"}
}

for _, req in ipairs(required_keys) do
    if not wingman.ini.has_key(config, req.section, req.key) then
        error(string.format("缺少必需的配置: [%s] %s", req.section, req.key))
    end
end
```

### 4. 数据验证

```lua
local wingman = require("wingman")

local data = wingman.json.decode(json_string)

-- 验证数据结构
if type(data) ~= "table" then
    error("无效的数据格式")
end

if not data.name or type(data.name) ~= "string" then
    error("缺少或无效的 name 字段")
end

if not data.level or type(data.level) ~= "number" then
    error("缺少或无效的 level 字段")
end
```

### 5. 版本控制

```lua
-- JSON 中包含版本信息
local save_data = {
    version = "1.0",
    player = {...},
    world = {...}
}

-- 加载时检查版本
local loaded = json.decode(save_string)
if loaded.version ~= "1.0" then
    -- 迁移旧版本数据
    loaded = migrate_save_data(loaded)
end
```

### 6. 性能优化

```lua
-- ✅ 只在配置更改时重新解析
local config_cache = nil

function getConfig()
    if not config_cache then
        local content = readFile("config.ini")
        config_cache = ini.decode(content)
    end
    return config_cache
end

-- ❌ 避免频繁解析
function badExample()
    local content = readFile("config.ini")
    local config = ini.decode(content)
    return config.Server.host
end
```

## 🔗 相关文档

- [配置管理指南](../guides/configuration.md)
- [数据持久化 API](storage.md)
- [核心 API](core.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
