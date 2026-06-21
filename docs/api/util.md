# API: wingman.util

工具函数模块，提供常用的辅助功能。

## 模块概述

util 模块提供通用工具函数：
- **时间函数** - 获取时间戳、格式化时间、延迟执行
- **随机数** - 生成随机浮点数和随机整数
- **系统信息** - 获取系统信息、脚本路径
- **命令执行** - 执行 Shell 命令
- **日志输出** - 输出不同级别的日志

---

## 延迟执行

### sleep(milliseconds) / sleep(milliseconds)

**说明**：延迟执行指定毫秒数。

**函数签名**：

```python
sleep(milliseconds: int) -> None
```

```lua
sleep(milliseconds: number) -> nil
```

**参数**：
- `milliseconds` - 延迟的毫秒数

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import util

# 延迟 1000 毫秒
util.sleep(1000)

# 延迟 2 秒
util.sleep(2000)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 延迟 1000 毫秒
wingman.util.sleep(1000)

-- 延迟 2 秒
wingman.util.sleep(2000)
```

:::

---

## 获取时间戳

### time() / time()

**说明**：获取当前时间戳（毫秒）。

**函数签名**：

```python
time() -> int
```

```lua
time() -> number
```

**返回**：
- 当前时间戳（毫秒）

:::tabs

== Python

```python:line-numbers
from wingman import util

# 获取当前时间戳（毫秒）
timestamp = util.time()
print(f"当前时间戳: {timestamp}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取当前时间戳（毫秒）
local timestamp = wingman.util.time()
print("当前时间戳: " .. timestamp)
```

:::

---

## 格式化时间

### format_time(format) / formatTime(format)

**说明**：格式化当前时间为字符串。

**函数签名**：

```python
format_time(format: str) -> str
```

```lua
formatTime(format: string) -> string
```

**参数**：
- `format` - 时间格式字符串（如 `"%Y-%m-%d %H:%M:%S"`）

**返回**：
- 格式化后的时间字符串

:::tabs

== Python

```python:line-numbers
from wingman import util

# 格式化时间为字符串
formatted = util.format_time("%Y-%m-%d %H:%M:%S")
print(f"当前时间: {formatted}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 格式化时间为字符串
local formatted = wingman.util.formatTime("%Y-%m-%d %H:%M:%S")
print("当前时间: " .. formatted)
```

:::

---

## 生成随机浮点数

### random() / random()

**说明**：生成 0-1 之间的随机浮点数。

**函数签名**：

```python
random() -> float
```

```lua
random() -> number
```

**返回**：
- 0-1 之间的随机浮点数

:::tabs

== Python

```python:line-numbers
from wingman import util

# 生成 0-1 之间的随机浮点数
rand = util.random()
print(f"随机数: {rand}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 生成 0-1 之间的随机浮点数
local rand = wingman.util.random()
print("随机数: " .. rand)
```

:::

---

## 生成随机整数

### random_int(min, max) / randomInt(min, max)

**说明**：生成指定范围内的随机整数。

**函数签名**：

```python
random_int(min: int, max: int) -> int
```

```lua
randomInt(min: number, max: number) -> number
```

**参数**：
- `min` - 最小值（包含）
- `max` - 最大值（包含）

**返回**：
- 指定范围内的随机整数

:::tabs

== Python

```python:line-numbers
from wingman import util

# 生成指定范围内的随机整数
rand_int = util.random_int(1, 100)
print(f"随机整数: {rand_int}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 生成指定范围内的随机整数
local randInt = wingman.util.randomInt(1, 100)
print("随机整数: " .. randInt)
```

:::

---

## 执行 Shell 命令

### shell_exec(command) / shellExec(command)

**说明**：执行 Shell 命令并获取输出。

**函数签名**：

```python
shell_exec(command: str) -> str
```

```lua
shellExec(command: string) -> string
```

**参数**：
- `command` - 要执行的命令

**返回**：
- 命令的标准输出

:::tabs

== Python

```python:line-numbers
from wingman import util

# 执行命令并获取输出
output = util.shell_exec("ls -la")
print(output)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 执行命令并获取输出
local output = wingman.util.shellExec("ls -la")
print(output)
```

:::

---

## 获取系统信息

### get_system_info() / getSystemInfo()

**说明**：获取系统信息。

**函数签名**：

```python
get_system_info() -> dict
```

```lua
getSystemInfo() -> table
```

**返回**：
- Python: 字典，包含 `os`, `arch`, `version` 等字段
- Lua: 表格，包含 `os`, `arch`, `version` 等字段

:::tabs

== Python

```python:line-numbers
from wingman import util

# 获取系统信息
info = util.get_system_info()
print(f"操作系统: {info['os']}")
print(f"架构: {info['arch']}")
print(f"Wingman 版本: {info['version']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取系统信息
local info = wingman.util.getSystemInfo()
print("操作系统: " .. info.os)
print("架构: " .. info.arch)
print("Wingman 版本: " .. info.version)
```

:::

---

## 获取脚本路径

### get_script_path() / getScriptPath()

**说明**：获取当前脚本的完整路径。

**函数签名**：

```python
get_script_path() -> str
```

```lua
getScriptPath() -> string
```

**返回**：
- 脚本完整路径

---

### get_script_dir() / getScriptDir()

**说明**：获取当前脚本所在目录。

**函数签名**：

```python
get_script_dir() -> str
```

```lua
getScriptDir() -> string
```

**返回**：
- 目录路径

:::tabs

== Python

```python:line-numbers
from wingman import util

# 获取当前脚本路径
script_path = util.get_script_path()
print(f"脚本路径: {script_path}")

# 获取脚本目录
script_dir = util.get_script_dir()
print(f"脚本目录: {script_dir}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取当前脚本路径
local scriptPath = wingman.util.getScriptPath()
print("脚本路径: " .. scriptPath)

-- 获取脚本目录
local scriptDir = wingman.util.getScriptDir()
print("脚本目录: " .. scriptDir)
```

:::

---

## 输出日志

### log(level, message) / log(level, message)

**说明**：输出日志到控制台。

**函数签名**：

```python
log(level: str, message: str) -> None
```

```lua
log(level: string, message: string) -> nil
```

**参数**：
- `level` - 日志级别：`"info"`, `"warn"`, `"error"`
- `message` - 日志消息

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import util

# 输出日志
util.log("info", "这是一条信息")
util.log("warn", "这是一条警告")
util.log("error", "这是一条错误")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 输出日志
wingman.util.log("info", "这是一条信息")
wingman.util.log("warn", "这是一条警告")
wingman.util.log("error", "这是一条错误")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `sleep(milliseconds)` | `sleep(milliseconds)` | 延迟执行 | milliseconds: 毫秒数 |
| `time()` | `time()` | 获取时间戳 | 返回: 毫秒时间戳 |
| `format_time(format)` | `formatTime(format)` | 格式化时间 | format: 格式字符串<br>返回: 格式化后的字符串 |
| `random()` | `random()` | 随机浮点数 | 返回: 0-1 之间的浮点数 |
| `random_int(min, max)` | `randomInt(min, max)` | 随机整数 | min: 最小值<br>max: 最大值<br>返回: 随机整数 |
| `shell_exec(command)` | `shellExec(command)` | 执行命令 | command: 命令字符串<br>返回: 命令输出 |
| `get_system_info()` | `getSystemInfo()` | 获取系统信息 | 返回: 信息字典/表格 |
| `get_script_path()` | `getScriptPath()` | 获取脚本路径 | 返回: 脚本完整路径 |
| `get_script_dir()` | `getScriptDir()` | 获取脚本目录 | 返回: 目录路径 |
| `log(level, message)` | `log(level, message)` | 输出日志 | level: 日志级别<br>message: 日志消息 |
