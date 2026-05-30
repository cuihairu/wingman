# API: wingman.util

工具函数模块，提供常用的辅助功能。

## 延迟执行

:::tabs

== Python

```python
from wingman import util

# 延迟 1000 毫秒
util.sleep(1000)

# 延迟 2 秒
util.sleep(2000)
```

== Lua

```lua
local util = require("wingman.util")

-- 延迟 1000 毫秒
util.sleep(1000)

-- 延迟 2 秒
util.sleep(2000)
```

:::

## 获取时间戳

:::tabs

== Python

```python
from wingman import util

# 获取当前时间戳（毫秒）
timestamp = util.time()
print(f"当前时间戳: {timestamp}")
```

== Lua

```lua
local util = require("wingman.util")

-- 获取当前时间戳（毫秒）
local timestamp = util.time()
print("当前时间戳: " .. timestamp)
```

:::

## 格式化时间

:::tabs

== Python

```python
from wingman import util

# 格式化时间为字符串
formatted = util.format_time("%Y-%m-%d %H:%M:%S")
print(f"当前时间: {formatted}")
```

== Lua

```lua
local util = require("wingman.util")

-- 格式化时间为字符串
local formatted = util.formatTime("%Y-%m-%d %H:%M:%S")
print("当前时间: " .. formatted)
```

:::

## 生成随机数

:::tabs

== Python

```python
from wingman import util

# 生成 0-1 之间的随机浮点数
rand = util.random()
print(f"随机数: {rand}")

# 生成指定范围内的随机整数
rand_int = util.random_int(1, 100)
print(f"随机整数: {rand_int}")

# 从数组中随机选择一个
import random
choices = ["A", "B", "C"]
selected = random.choice(choices)
```

== Lua

```lua
local util = require("wingman.util")

-- 生成 0-1 之间的随机浮点数
local rand = util.random()
print("随机数: " .. rand)

-- 生成指定范围内的随机整数
local randInt = util.randomInt(1, 100)
print("随机整数: " .. randInt)

-- Lua 原生随机数
math.randomseed(util.time())
local choice = ({"A", "B", "C"})[math.random(3)]
print("随机选择: " .. choice)
```

:::

## 执行 Shell 命令

:::tabs

== Python

```python
from wingman import util

# 执行命令并获取输出
output = util.shell_exec("dir C:\\")
print(output)
```

== Lua

```lua
local util = require("wingman.util")

-- 执行命令并获取输出
local output = util.shellExec("dir C:\\")
print(output)
```

:::

## 获取系统信息

:::tabs

== Python

```python
from wingman import util

# 获取系统信息
info = util.get_system_info()
print(f"操作系统: {info['os']}")
print(f"架构: {info['arch']}")
print(f"Wingman 版本: {info['version']}")
```

== Lua

```lua
local util = require("wingman.util")

-- 获取系统信息
local info = util.getSystemInfo()
print("操作系统: " .. info.os)
print("架构: " .. info.arch)
print("Wingman 版本: " .. info.version)
```

:::

## 获取脚本路径

:::tabs

== Python

```python
from wingman import util

# 获取当前脚本路径
script_path = util.get_script_path()
print(f"脚本路径: {script_path}")

# 获取脚本目录
script_dir = util.get_script_dir()
print(f"脚本目录: {script_dir}")
```

== Lua

```lua
local util = require("wingman.util")

-- 获取当前脚本路径
local scriptPath = util.getScriptPath()
print("脚本路径: " .. scriptPath)

-- 获取脚本目录
local scriptDir = util.getScriptDir()
print("脚本目录: " .. scriptDir)
```

:::

## 日志输出

:::tabs

== Python

```python
from wingman import util

# 输出日志
util.log("info", "这是一条信息")
util.log("warn", "这是一条警告")
util.log("error", "这是一条错误")
```

== Lua

```lua
local util = require("wingman.util")

-- 输出日志
util.log("info", "这是一条信息")
util.log("warn", "这是一条警告")
util.log("error", "这是一条错误")
```

:::

---

## 可用接口

### `sleep(milliseconds)`

延迟执行指定毫秒数。

### `time()`

获取当前时间戳（毫秒）。

### `format_time(format)` / `formatTime(format)`

格式化时间为字符串。

**参数：**
- `format` - 时间格式字符串（如 `"%Y-%m-%d %H:%M:%S"`）

**返回：**
- `string` - 格式化后的时间字符串

### `random()`

生成 0-1 之间的随机浮点数。

**返回：**
- `number` - 随机数

### `random_int(min, max)` / `randomInt(min, max)`

生成指定范围内的随机整数。

**参数：**
- `min` - 最小值
- `max` - 最大值

**返回：**
- `number` - 随机整数

### `shell_exec(command)` / `shellExec(command)`

执行 Shell 命令并获取输出。

**参数：**
- `command` - 要执行的命令

**返回：**
- `string` - 命令输出

### `get_system_info()` / `getSystemInfo()`

获取系统信息。

**返回：**
- `dict/table` - 包含 `os`, `arch`, `version` 等字段

### `get_script_path()` / `getScriptPath()`

获取当前脚本路径。

**返回：**
- `string` - 脚本完整路径

### `get_script_dir()` / `getScriptDir()`

获取当前脚本所在目录。

**返回：**
- `string` - 目录路径

### `log(level, message)`

输出日志。

**参数：**
- `level` - 日志级别：`"info"`, `"warn"`, `"error"`
- `message` - 日志消息
