# 配置管理指南

本指南详细介绍如何使用 Wingman 的配置管理功能。

## 📋 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [JSON 配置](#json-配置)
- [INI 配置](#ini-配置)
- [配置合并](#配置合并)
- [环境配置](#环境配置)
- [最佳实践](#最佳实践)
- [实战案例](#实战案例)

---

## 概述

Wingman 提供了灵活的配置管理方案，支持多种格式：

| 格式 | 适用场景 | 优点 |
|------|----------|------|
| **JSON** | 复杂数据结构、嵌套配置 | 通用性强、支持复杂数据类型 |
| **INI** | 简单配置、人类可编辑 | 简洁直观、易于维护 |
| **键值存储** | 运行时配置、缓存 | 高性能、简单快速 |

### 配置文件位置

配置文件通常存储在以下位置：

- **Windows**: `%APPDATA%\wingman\config\`
- **macOS**: `~/Library/Application Support/wingman/config/`
- **Linux**: `~/.config/wingman/`

---

## 快速开始

### 使用 JSON 配置

#### Lua

```lua
local json = require("wingman.json")

-- 创建配置
local config = {
    server = {
        host = "localhost",
        port = 8080
    },
    debug = true,
    maxConnections = 100
}

-- 保存配置
local config_file = io.open("config.json", "w")
config_file:write(json.encode(config, true))
config_file:close()

-- 加载配置
local config_file = io.open("config.json", "r")
local config_content = config_file:read("*all")
config_file:close()

local config = json.decode(config_content)
print(config.server.host)  -- "localhost"
```

#### Python

```python
import json

# 创建配置
config = {
    "server": {
        "host": "localhost",
        "port": 8080
    },
    "debug": True,
    "maxConnections": 100
}

# 保存配置
with open("config.json", "w") as f:
    f.write(json.encode(config, pretty=True))

# 加载配置
with open("config.json", "r") as f:
    config_content = f.read()

config = json.decode(config_content)
print(config["server"]["host"])  # "localhost"
```

### 使用 INI 配置

#### Lua

```lua
local ini = require("wingman.ini")

-- 创建配置
local config = {
    Server = {
        host = "localhost",
        port = "8080"
    },
    Database = {
        host = "db.example.com",
        port = "5432"
    }
}

-- 保存配置
local ini_content = ini.encode(config)
local config_file = io.open("config.ini", "w")
config_file:write(ini_content)
config_file:close()

-- 加载配置
local config_file = io.open("config.ini", "r")
local config_content = config_file:read("*all")
config_file:close()

local config = ini.decode(config_content)
print(config.Server.host)  -- "localhost"
```

#### Python

```python
from wingman import ini

# 创建配置
config = {
    "Server": {
        "host": "localhost",
        "port": "8080"
    },
    "Database": {
        "host": "db.example.com",
        "port": "5432"
    }
}

# 保存配置
ini_content = ini.encode(config)
with open("config.ini", "w") as f:
    f.write(ini_content)

# 加载配置
with open("config.ini", "r") as f:
    config_content = f.read()

config = ini.decode(config_content)
print(config["Server"]["host"])  # "localhost"
```

---

## JSON 配置

### 基本操作

#### 编码配置

#### Lua

```lua
local json = require("wingman.json")

-- 简单配置
local config = {
    name = "MyApp",
    version = "1.0.0",
    debug = true
}

-- 编码为 JSON 字符串
local json_string = json.encode(config)
-- {"name":"MyApp","version":"1.0.0","debug":true}

-- 美化输出（带缩进）
local pretty_json = json.encode(config, true)
print(pretty_json)
-- {
--   "name": "MyApp",
--   "version": "1.0.0",
--   "debug": true
-- }
```

#### Python

```python
from wingman import json

# 简单配置
config = {
    "name": "MyApp",
    "version": "1.0.0",
    "debug": True
}

# 编码为 JSON 字符串
json_string = json.encode(config)

# 美化输出
pretty_json = json.encode(config, pretty=True)
print(pretty_json)
```

### 复杂配置结构

#### Lua

```lua
local json = require("wingman.json")

-- 复杂配置结构
local config = {
    app = {
        name = "GameAutomation",
        version = "2.0.0",
        settings = {
            resolution = "1920x1080",
            fullscreen = true,
            vsync = true
        }
    },
    modules = {
        screen = {
            enabled = true,
            captureRate = 60
        },
        input = {
            enabled = true,
            delay = 50
        }
    },
    hotkeys = {
        start = {"F5"},
        stop = {"F6"},
        pause = {"F7"}
    }
}

-- 保存配置
local json_string = json.encode(config, true)
local file = io.open("config.json", "w")
file:write(json_string)
file:close()
```

#### Python

```python
from wingman import json

# 复杂配置结构
config = {
    "app": {
        "name": "GameAutomation",
        "version": "2.0.0",
        "settings": {
            "resolution": "1920x1080",
            "fullscreen": True,
            "vsync": True
        }
    },
    "modules": {
        "screen": {
            "enabled": True,
            "captureRate": 60
        },
        "input": {
            "enabled": True,
            "delay": 50
        }
    },
    "hotkeys": {
        "start": ["F5"],
        "stop": ["F6"],
        "pause": ["F7"]
    }
}

# 保存配置
json_string = json.encode(config, pretty=True)
with open("config.json", "w") as f:
    f.write(json_string)
```

### 读取和修改配置

#### Lua

```lua
local json = require("wingman.json")

-- 读取配置
local file = io.open("config.json", "r")
local config_content = file:read("*all")
file:close()

local config = json.decode(config_content)

-- 读取嵌套值
print(config.app.settings.resolution)

-- 修改值
config.app.settings.resolution = "2560x1440"
config.modules.input.delay = 30

-- 添加新值
config.app.newField = "newValue"

-- 保存修改后的配置
local updated_json = json.encode(config, true)
local file = io.open("config.json", "w")
file:write(updated_json)
file:close()
```

#### Python

```python
from wingman import json

# 读取配置
with open("config.json", "r") as f:
    config_content = f.read()

config = json.decode(config_content)

# 读取嵌套值
print(config["app"]["settings"]["resolution"])

# 修改值
config["app"]["settings"]["resolution"] = "2560x1440"
config["modules"]["input"]["delay"] = 30

# 添加新值
config["app"]["newField"] = "newValue"

# 保存修改后的配置
updated_json = json.encode(config, pretty=True)
with open("config.json", "w") as f:
    f.write(updated_json)
```

---

## INI 配置

### 基本操作

#### 解析 INI 文件

#### Lua

```lua
local ini = require("wingman.ini")

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

; 客户端配置
[Client]
theme = dark
language = zh
]]

local config = ini.decode(content)

-- 读取整个 section
local server = config["Server"]
print(server.host)    -- "localhost"
print(server.port)    -- "8080"

-- 读取单个值
local db_host = ini.get(config, "Database", "host")
print(db_host)  -- "db.example.com"
```

#### Python

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

config = ini.decode(content)

# 读取整个 section
server = config["Server"]
print(server["host"])  # "localhost"
print(server["port"])  # "8080"

# 读取单个值
db_host = ini.get(config, "Database", "host")
print(db_host)  # "db.example.com"
```

### 生成 INI 文件

#### Lua

```lua
local ini = require("wingman.ini")

local config = {
    Server = {
        host = "localhost",
        port = "8080",
        debug = "true"
    },
    Client = {
        theme = "dark",
        language = "zh"
    }
}

local ini_content = ini.encode(config)
print(ini_content)
-- [Server]
-- host=localhost
-- port=8080
-- debug=true
--
-- [Client]
-- theme=dark
-- language=zh
```

#### Python

```python
from wingman import ini

config = {
    "Server": {
        "host": "localhost",
        "port": "8080",
        "debug": "true"
    },
    "Client": {
        "theme": "dark",
        "language": "zh"
    }
}

ini_content = ini.encode(config)
print(ini_content)
```

### 修改 INI 配置

#### Lua

```lua
local ini = require("wingman.ini")

-- 读取配置
local content = [[
[Server]
host = localhost
port = 8080
]]

local config = ini.decode(content)

-- 修改值
config = ini.set(config, "Server", "port", "9090")

-- 添加新 key
config = ini.set(config, "Server", "timeout", "30")

-- 添加新 section
config = ini.set(config, "Database", "host", "localhost")

-- 删除 key
config = ini.delete(config, "Server", "debug")

-- 删除 section
config = ini.delete(config, "OldSection")

-- 保存修改后的配置
local updated_content = ini.encode(config)
```

#### Python

```python
from wingman import ini

# 读取配置
content = """
[Server]
host = localhost
port = 8080
"""

config = ini.decode(content)

# 修改值
config = ini.set(config, "Server", "port", "9090")

# 添加新 key
config = ini.set(config, "Server", "timeout", "30")

# 添加新 section
config = ini.set(config, "Database", "host", "localhost")

# 删除 key
config = ini.delete(config, "Server", "debug")

# 删除 section
config = ini.delete(config, "OldSection")

# 保存修改后的配置
updated_content = ini.encode(config)
```

### 检查配置项

#### Lua

```lua
local ini = require("wingman.ini")

-- 检查 section 是否存在
local has_section = ini.has_section(config, "Server")
print(has_section)  -- true

-- 检查 key 是否存在
local has_key = ini.has_key(config, "Server", "port")
print(has_key)  -- true

-- 获取所有 section
local sections = ini.sections(config)
for _, section in ipairs(sections) do
    print(section)
end

-- 获取 section 中的所有 key
local keys = ini.keys(config, "Server")
for _, key in ipairs(keys) do
    print(key)
end
```

#### Python

```python
from wingman import ini

# 检查 section 是否存在
has_section = ini.has_section(config, "Server")
print(has_section)  # True

# 检查 key 是否存在
has_key = ini.has_key(config, "Server", "port")
print(has_key)  # True

# 获取所有 section
sections = ini.sections(config)
for section in sections:
    print(section)

# 获取 section 中的所有 key
keys = ini.keys(config, "Server")
for key in keys:
    print(key)
```

---

## 配置合并

### 默认配置与用户配置

#### Lua

```lua
local ini = require("wingman.ini")

-- 系统默认配置
local default_config = ini.decode([[
[Graphics]
resolution = 1920x1080
quality = medium
vsync = true
fullscreen = false

[Audio]
master = 100
music = 80
sfx = 100
]])

-- 用户配置
local user_config = ini.decode([[
[Graphics]
resolution = 2560x1440
quality = ultra
fullscreen = true

[Audio]
master = 50
music = 40
]])

-- 合并配置（用户配置覆盖默认配置）
local final_config = ini.merge(default_config, user_config)

-- 结果：
-- Graphics.resolution = "2560x1440" (用户配置)
-- Graphics.quality = "ultra" (用户配置)
-- Graphics.vsync = "true" (默认配置)
-- Graphics.fullscreen = "true" (用户配置)
-- Audio.master = "50" (用户配置)
-- Audio.music = "40" (用户配置)
-- Audio.sfx = "100" (默认配置)
```

#### Python

```python
from wingman import ini

# 系统默认配置
default_config = ini.decode("""
[Graphics]
resolution = 1920x1080
quality = medium
vsync = true
fullscreen = false
""")

# 用户配置
user_config = ini.decode("""
[Graphics]
resolution = 2560x1440
quality = ultra
fullscreen = true
""")

# 合并配置
final_config = ini.merge(default_config, user_config)
```

### 多层配置合并

#### Lua

```lua
local ini = require("wingman.ini")

-- 系统默认配置
local system_config = ini.decode([[
[App]
name = GameAutomation
version = 1.0.0
]])

-- 用户配置
local user_config = ini.decode([[
[Graphics]
resolution = 1920x1080
]])

-- 运行时配置（最高优先级）
local runtime_config = ini.decode([[
[Graphics]
fullscreen = true
]])

-- 按优先级合并
local final_config = ini.merge(system_config, user_config)
final_config = ini.merge(final_config, runtime_config)

-- 结果：
-- App.name = "GameAutomation" (系统配置)
-- App.version = "1.0.0" (系统配置)
-- Graphics.resolution = "1920x1080" (用户配置)
-- Graphics.fullscreen = "true" (运行时配置)
```

#### Python

```python
from wingman import ini

# 系统默认配置
system_config = ini.decode("""
[App]
name = GameAutomation
version = 1.0.0
""")

# 用户配置
user_config = ini.decode("""
[Graphics]
resolution = 1920x1080
""")

# 运行时配置
runtime_config = ini.decode("""
[Graphics]
fullscreen = true
""")

# 按优先级合并
final_config = ini.merge(system_config, user_config)
final_config = ini.merge(final_config, runtime_config)
```

---

## 环境配置

### 开发环境配置

#### Lua

```lua
local json = require("wingman.json")

-- 检测环境
local env = os.getenv("APP_ENV") or "development"

-- 根据环境加载配置
local configs = {
    development = {
        debug = true,
        logLevel = "debug",
        server = {
            host = "localhost",
            port = 3000
        }
    },
    production = {
        debug = false,
        logLevel = "error",
        server = {
            host = "0.0.0.0",
            port = 80
        }
    }
}

-- 加载对应环境的配置
local config = configs[env] or configs.development

print("Running in", env, "mode")
print("Debug:", config.debug)
print("Server:", config.server.host, config.server.port)
```

#### Python

```python
import os
from wingman import json

# 检测环境
env = os.getenv("APP_ENV", "development")

# 根据环境加载配置
configs = {
    "development": {
        "debug": True,
        "logLevel": "debug",
        "server": {
            "host": "localhost",
            "port": 3000
        }
    },
    "production": {
        "debug": False,
        "logLevel": "error",
        "server": {
            "host": "0.0.0.0",
            "port": 80
        }
    }
}

# 加载对应环境的配置
config = configs.get(env, configs["development"])

print(f"Running in {env} mode")
print(f"Debug: {config['debug']}")
print(f"Server: {config['server']['host']}:{config['server']['port']}")
```

### 配置文件覆盖

#### Lua

```lua
local json = require("wingman.json")

-- 默认配置
local default_config = {
    debug = false,
    logLevel = "info"
}

-- 配置文件路径
local config_paths = {
    "/etc/wingman/config.json",
    os.getenv("HOME") .. "/.config/wingman/config.json",
    "config.json"
}

-- 加载第一个存在的配置文件
local config = default_config
for _, path in ipairs(config_paths) do
    local file = io.open(path, "r")
    if file then
        local content = file:read("*all")
        file:close()

        local file_config = json.decode(content)
        -- 合并配置
        for k, v in pairs(file_config) do
            config[k] = v
        end

        print("Loaded config from:", path)
        break
    end
end
```

#### Python

```python
import os
from wingman import json

# 默认配置
default_config = {
    "debug": False,
    "logLevel": "info"
}

# 配置文件路径
config_paths = [
    "/etc/wingman/config.json",
    os.path.expanduser("~/.config/wingman/config.json"),
    "config.json"
]

# 加载第一个存在的配置文件
config = default_config.copy()
for path in config_paths:
    try:
        with open(path, "r") as f:
            content = f.read()
            file_config = json.decode(content)
            # 合并配置
            config.update(file_config)
            print(f"Loaded config from: {path}")
            break
    except FileNotFoundError:
        continue
```

---

## 最佳实践

### 1. 配置文件命名

```
config/
├── default.json          # 默认配置（不要修改）
├── development.json      # 开发环境配置
├── production.json       # 生产环境配置
└── local.json            # 本地配置（不提交到版本控制）
```

### 2. 敏感信息处理

```lua
-- ❌ 不要在配置文件中存储敏感信息
local config = {
    database = {
        password = "mysecretpassword"  -- 错误！
    }
}

-- ✅ 使用环境变量
local config = {
    database = {
        password = os.getenv("DB_PASSWORD")  -- 正确
    }
}

-- ✅ 或使用加密配置
local encrypted = require("wingman.crypto")
local config = {
    database = {
        password = encrypted.decrypt(os.getenv("ENCRYPTED_PASSWORD"))
    }
}
```

### 3. 配置验证

```lua
-- 定义配置架构
local schema = {
    server = {
        host = "string",
        port = "number"
    },
    debug = "boolean"
}

-- 验证配置
function validateConfig(config, schema)
    for key, expected_type in pairs(schema) do
        if config[key] == nil then
            error(string.format("Missing required config: %s", key))
        end

        if type(config[key]) ~= expected_type then
            error(string.format("Invalid type for %s: expected %s, got %s",
                key, expected_type, type(config[key])))
        end
    end
end

-- 使用
local config = json.decode(config_content)
validateConfig(config, schema)
```

### 4. 配置版本控制

```lua
-- 在配置中包含版本信息
local config = {
    version = "2.0",
    settings = {...}
}

-- 加载时检查版本
local loaded = json.decode(config_content)
if loaded.version ~= "2.0" then
    -- 迁移旧版本配置
    loaded = migrateConfig(loaded)
end
```

### 5. 配置缓存

```lua
local config_cache = nil

function getConfig()
    if not config_cache then
        -- 首次加载
        local file = io.open("config.json", "r")
        local content = file:read("*all")
        file:close()

        config_cache = json.decode(content)
    end

    return config_cache
end

-- 重新加载配置
function reloadConfig()
    local file = io.open("config.json", "r")
    local content = file:read("*all")
    file:close()

    config_cache = json.decode(content)
end
```

---

## 实战案例

### 游戏配置系统

#### Lua

```lua
local json = require("wingman.json")
local ini = require("wingman.ini")

-- 游戏配置类
local GameConfig = {}
GameConfig.__index = GameConfig

function GameConfig.load(config_path)
    local self = setmetatable({}, GameConfig)

    -- 加载主配置
    local file = io.open(config_path, "r")
    local content = file:read("*all")
    file:close()

    self.config = json.decode(content)

    -- 加载键位绑定
    local binds_file = io.open("keybinds.ini", "r")
    if binds_file then
        local binds_content = binds_file:read("*all")
        binds_file:close()
        self.keybinds = ini.decode(binds_content)
    else
        self.keybinds = {}
    end

    -- 加载用户设置
    local settings_file = io.open("user_settings.json", "r")
    if settings_file then
        local settings_content = settings_file:read("*all")
        settings_file:close()

        local user_settings = json.decode(settings_content)
        -- 合并用户设置
        for section, settings in pairs(user_settings) do
            if not self.config[section] then
                self.config[section] = {}
            end
            for key, value in pairs(settings) do
                self.config[section][key] = value
            end
        end
    end

    return self
end

function GameConfig:get(section, key, default)
    if self.config[section] and self.config[section][key] ~= nil then
        return self.config[section][key]
    end
    return default
end

function GameConfig:set(section, key, value)
    if not self.config[section] then
        self.config[section] = {}
    end
    self.config[section][key] = value
end

function GameConfig:save()
    -- 保存配置
    local content = json.encode(self.config, true)
    local file = io.open("user_settings.json", "w")
    file:write(content)
    file:close()
end

function GameConfig:getKeybind(action)
    local section = self.keybinds[action]
    if section then
        return section.keys or section.key
    end
    return nil
end

-- 使用示例
local config = GameConfig.load("game_config.json")

-- 获取配置
local resolution = config:get("graphics", "resolution", "1920x1080")
local vsync = config:get("graphics", "vsync", true)

-- 修改配置
config:set("graphics", "resolution", "2560x1440")
config:save()

-- 获取键位绑定
local jump_key = config:getKeybind("jump")
```

### 多环境配置管理

#### Lua

```lua
local json = require("wingman.json")

local ConfigManager = {}
ConfigManager.__index = ConfigManager

function ConfigManager.new()
    local self = setmetatable({}, ConfigManager)
    self.env = os.getenv("APP_ENV") or "development"
    self.config = {}
    return self
end

function ConfigManager:loadDefaults()
    self.config = {
        app = {
            name = "WingmanApp",
            version = "1.0.0"
        },
        server = {
            host = "localhost",
            port = 3000,
            ssl = false
        },
        database = {
            host = "localhost",
            port = 5432,
            name = "wingman_dev"
        },
        logging = {
            level = "info",
            file = "app.log"
        }
    }
end

function ConfigManager:loadEnvironment()
    local env_config_file = string.format("config/%s.json", self.env)

    local file = io.open(env_config_file, "r")
    if file then
        local content = file:read("*all")
        file:close()

        local env_config = json.decode(content)

        -- 深度合并配置
        self:mergeConfig(self.config, env_config)
    else
        print(string.format("Warning: Environment config %s not found", env_config_file))
    end
end

function ConfigManager:mergeConfig(base, override)
    for section, values in pairs(override) do
        if not base[section] then
            base[section] = {}
        end

        if type(values) == "table" then
            self:mergeConfig(base[section], values)
        else
            base[section] = values
        end
    end
end

function ConfigManager:get(path, default)
    local parts = {}
    for part in string.gmatch(path, "[^%.]+") do
        table.insert(parts, part)
    end

    local current = self.config
    for i, part in ipairs(parts) do
        if current[part] then
            current = current[part]
        else
            return default
        end
    end

    return current
end

function ConfigManager:validate(required)
    for _, path in ipairs(required) do
        if not self:get(path) then
            error(string.format("Missing required config: %s", path))
        end
    end
end

-- 使用示例
local config_manager = ConfigManager.new()

-- 加载默认配置
config_manager:loadDefaults()

-- 加载环境特定配置
config_manager:loadEnvironment()

-- 验证必需配置
config_manager:validate({
    "server.host",
    "server.port",
    "database.host"
})

-- 使用配置
local server_host = config_manager:get("server.host", "localhost")
local server_port = config_manager:get("server.port", 3000)
local db_name = config_manager:get("database.name", "wingman")

print(string.format("Environment: %s", config_manager.env))
print(string.format("Server: %s:%d", server_host, server_port))
print(string.format("Database: %s", db_name))
```

---

## 🔗 相关文档

- [序列化 API](../api/serialization.md)
- [数据持久化 API](../api/storage.md)
- [脚本 API](../api/script.md)

---

**返回**: [文档首页](../README.md) | [使用指南](../README.md#使用指南)
