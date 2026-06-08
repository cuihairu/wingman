# 序列化

Wingman 支持多种数据序列化格式，用于配置文件、数据交换、持久化等场景。

## 支持的格式

| 格式 | 模块 | 特点 | 适用场景 |
|------|------|------|---------|
| **JSON** | `json` | 轻量、易读、广泛支持 | API 数据交换、配置文件 |
| **YAML** | *(规划中)* | 更易读、支持注释 | 复杂配置、人工编辑的文件 |
| **INI** | `ini` | 简单、兼容性好、支持转义和合并 | 简单配置、传统应用配置 |

## 格式对比

### JSON

**优点：**
- 标准化格式，几乎所有语言都支持
- 解析速度快
- 适合机器处理和 API 交互
- 类型丰富（字符串、数字、布尔、数组、对象）

**缺点：**
- 不支持注释
- 冗余语法（引号、逗号）
- 不适合人工编辑大量配置

**示例：**

```python
from wingman import json

# 解析 JSON
config_str = '{"host": "localhost", "port": 8080, "debug": true}'
config = json.decode(config_str)

# 编码 JSON
output = json.encode(config, indent=2)
```

### YAML

**优点：**
- 更易读，适合人工编辑
- 支持注释
- 更简洁的语法
- 支持锚点和别名（复用）

**缺点：**
- 解析较慢
- 格式较复杂（缩进敏感）
- 不同实现可能有兼容性问题

**示例（规划中）：**

```python
from wingman import yaml

# 解析 YAML
config_str = '''
# 服务器配置
host: localhost
port: 8080
debug: true

# 数据库配置
database:
  host: db.example.com
  port: 5432
'''
config = yaml.decode(config_str)

# 编码 YAML
output = yaml.encode(config)
```

### INI

**优点：**
- 极其简单
- 广泛兼容（Windows 应用传统格式）
- 适合扁平配置

**缺点：**
- 不支持嵌套结构
- 类型有限（主要是字符串）
- 不适合复杂数据

**示例：**

```python
from wingman import ini

# 解析 INI
config_str = '''
[Server]
host=localhost
port=8080

[Database]
host=db.example.com
port=5432
'''
config = ini.decode(config_str)

# 编码 INI
output = ini.encode(config)

# 读取配置
host = ini.get(config, "Server", "host")
port = ini.get(config, "Server", "port")

# 修改配置
ini.set(config, "Server", "host", "192.168.1.1")

# 保存配置
with open("config.ini", "w") as f:
    f.write(ini.encode(config))
```

```lua
local ini = require("wingman.ini")

-- 解析 INI
local config_str = [[
[Server]
host=localhost
port=8080
]]

local config = ini.decode(config_str)

-- 读取配置
local host = ini.get(config, "Server", "host")
local port = ini.get(config, "Server", "port")

-- 修改配置
ini.set(config, "Server", "host", "192.168.1.1")
```

## 选择指南

### 使用 JSON

**适合场景：**
- API 请求/响应
- 与外部系统数据交换
- 需要标准格式的配置文件
- 自动化工具生成的配置

**示例：**
```json
{
  "api_endpoint": "https://api.example.com",
  "timeout": 30000,
  "retry_count": 3
}
```

### 使用 YAML（规划中）

**适合场景：**
- 需要人工编辑的复杂配置
- 配置文件需要注释说明
- 配置层级较深

**示例：**
```yaml
# 服务器配置
server:
  host: localhost
  port: 8080
  # 开发模式启用
  debug: true

# 日志配置
logging:
  level: info    # 日志级别
  file: app.log  # 日志文件
```

### 使用 INI

**适合场景：**
- 简单应用配置
- 与传统 Windows 应用兼容
- 扁平键值对配置

**示例：**
```ini
[Server]
host=localhost
port=8080

[Logging]
level=info
file=app.log
```

## 模块详情

- [json 模块文档](./json) - JSON 完整 API
- [ini 模块文档](./ini) - INI 完整 API
- yaml 模块（规划中）

## 格式转换

在不同格式间转换数据：

```python
from wingman import json

# JSON → YAML（需要 yaml 模块）
json_data = json.decode('{"key": "value"}')
# yaml_str = yaml.encode(json_data)

# YAML → JSON（需要 yaml 模块）
# yaml_data = yaml.decode(yaml_str)
# json_str = json.encode(yaml_data)
```

## 最佳实践

### 命名规范

- JSON 文件：`.json` 扩展名
- YAML 文件：`.yaml` 或 `.yml` 扩展名
- INI 文件：`.ini` 扩展名

### 编码风格

**JSON：**
- 使用 2 或 4 空格缩进
- 键名使用 snake_case 或 camelCase 保持一致

**YAML：**
- 使用 2 空格缩进（YAML 标准）
- 保持缩进一致性

**INI：**
- 使用 `[Section]` 组织配置
- 键值对使用 `=` 分隔

### 性能考虑

- JSON 解析最快，优先用于高性能场景
- YAML 适合配置加载，不适用于高频数据交换
- INI 只用于简单场景，复杂配置推荐 JSON/YAML

## 注意事项

1. **安全性**
   - 解析不可信数据时注意防范注入攻击
   - JSON 解析器默认不执行代码，较安全
   - YAML 某些特性可能存在安全风险（启用后）

2. **编码**
   - JSON/YAML 默认使用 UTF-8 编码
   - INI 文件编码可能依赖系统，建议显式指定 UTF-8

3. **兼容性**
   - JSON 是标准格式，跨平台无问题
   - YAML 不同解析器可能有细微差异
   - INI 格式不统一，建议使用简单语法

4. **大文件处理**
   - JSON/YAML 大文件建议流式解析（未来支持）
   - 避免一次性加载超大文件到内存
