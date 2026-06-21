# API: wingman.debugger

调试器模块，提供断点调试和脚本调试功能。

## 模块概述

debugger 模块提供脚本调试功能：
- **断点控制** - 设置断点、条件断点、日志断点
- **调试服务器** - 启动/连接调试服务器
- **执行控制** - 单步执行、继续执行
- **变量检查** - 查看局部变量、全局变量、表达式求值

---

## 设置断点

### breakpoint() / breakpoint()

**说明**：在当前行设置断点。

**函数签名**：

```python
breakpoint() -> None
```

```lua
breakpoint() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 在当前行设置断点
debugger.breakpoint()
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 在当前行设置断点
debugger.breakpoint()
```

:::

---

## 设置条件断点

### breakpoint_if(condition) / breakpointIf(condition)

**说明**：设置条件断点，条件为真时触发断点。

**函数签名**：

```python
breakpoint_if(condition: Callable[[], bool]) -> None
```

```lua
breakpointIf(condition: fun(): boolean) -> nil
```

**参数**：
- `condition` - 条件函数，返回 `True`/`true` 时触发断点

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 设置条件断点
debugger.breakpoint_if(lambda: x > 10)
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 设置条件断点
debugger.breakpointIf(function() return x > 10 end)
```

:::

---

## 设置日志断点

### log_point(message, ...args) / logPoint(message, ...args)

**说明**：设置日志断点，输出日志消息但不中断执行。

**函数签名**：

```python
log_point(message: str, *args) -> None
```

```lua
logPoint(message: string, ...any) -> nil
```

**参数**：
- `message` - 日志消息（支持 `{}` 占位符）
- `...args` - 占位符参数

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 设置日志断点
debugger.log_point("变量 x 的值: {}", x)
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 设置日志断点
debugger.logPoint("变量 x 的值: " .. tostring(x), x)
```

:::

---

## 启动调试服务器

### start_server(port) / startServer(port)

**说明**：启动调试服务器，等待 IDE 连接。

**函数签名**：

```python
start_server(port: int = 9999) -> None
```

```lua
startServer(port: number = 9999) -> nil
```

**参数**：
- `port` - 可选，监听端口，默认 9999

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 启动调试服务器
debugger.start_server(9999)
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 启动调试服务器
debugger.startServer(9999)
```

:::

---

## 连接调试器

### connect(host, port) / connect(host, port)

**说明**：连接到远程调试器。

**函数签名**：

```python
connect(host: str = "localhost", port: int = 9999) -> bool
```

```lua
connect(host: string = "localhost", port: number = 9999) -> boolean
```

**参数**：
- `host` - 可选，主机地址，默认 localhost
- `port` - 可选，端口号，默认 9999

**返回**：
- 是否连接成功

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 连接到远程调试器
debugger.connect("localhost", 9999)
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 连接到远程调试器
debugger.connect("localhost", 9999)
```

:::

---

## 输出调试信息

### log(..., ...) / log(..., ...)

**说明**：输出调试信息到调试器控制台。

**函数签名**：

```python
log(*args) -> None
```

```lua
log(...) -> nil
```

**参数**：
- 可变参数，要输出的内容

**返回**：
- 无

---

### inspect(data) / inspect(data)

**说明**：输出变量详细信息。

**函数签名**：

```python
inspect(data: Any) -> None
```

```lua
inspect(data: any) -> nil
```

**参数**：
- `data` - 要检查的数据

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 输出调试信息
debugger.log("变量 x 的值:", x)
debugger.log(f"当前状态: {status}")

# 输出变量信息
debugger.inspect({"x": x, "y": y, "status": status})
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 输出调试信息
debugger.log("变量 x 的值:", x)
debugger.log("当前状态: " .. tostring(status))

-- 输出变量信息
debugger.inspect({x = x, y = y, status = status})
```

:::

---

## 获取调用栈

### get_stacktrace() / getStacktrace()

**说明**：获取当前调用栈信息。

**函数签名**：

```python
get_stacktrace() -> list[dict]
```

```lua
getStacktrace() -> table
```

**返回**：
- Python: 调用栈帧列表，每帧包含 `{file, line, function}`
- Lua: 调用栈帧数组

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 获取当前调用栈
stack = debugger.get_stacktrace()
for frame in stack:
    print(f"  {frame['file']}:{frame['line']} in {frame['function']}")
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 获取当前调用栈
local stack = debugger.getStacktrace()
for i, frame in ipairs(stack) do
    print("  " .. frame.file .. ":" .. frame.line .. " in " .. frame.function)
end
```

:::

---

## 单步执行

### step_into() / stepInto()

**说明**：单步进入，进入函数内部。

**函数签名**：

```python
step_into() -> None
```

```lua
stepInto() -> nil
```

---

### step_over() / stepOver()

**说明**：单步跳过，不进入函数内部。

**函数签名**：

```python
step_over() -> None
```

```lua
stepOver() -> nil
```

---

### step_out() / stepOut()

**说明**：跳出当前函数。

**函数签名**：

```python
step_out() -> None
```

```lua
stepOut() -> nil
```

---

### resume() / resume()

**说明**：继续执行，直到下一个断点。

**函数签名**：

```python
resume() -> None
```

```lua
resume() -> nil
```

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 单步进入
debugger.step_into()

# 单步跳过
debugger.step_over()

# 跳出当前函数
debugger.step_out()

# 继续执行
debugger.resume()
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 单步进入
debugger.stepInto()

-- 单步跳过
debugger.stepOver()

-- 跳出当前函数
debugger.stepOut()

-- 继续执行
debugger.resume()
```

:::

---

## 变量检查

### get_locals() / getLocals()

**说明**：获取当前作用域的局部变量。

**函数签名**：

```python
get_locals() -> dict
```

```lua
getLocals() -> table
```

**返回**：
- Python: 局部变量字典
- Lua: 局部变量表格

---

### get_globals() / getGlobals()

**说明**：获取全局变量。

**函数签名**：

```python
get_globals() -> dict
```

```lua
getGlobals() -> table
```

**返回**：
- Python: 全局变量字典
- Lua: 全局变量表格

---

### eval(expression) / eval(expression)

**说明**：计算表达式并返回结果。

**函数签名**：

```python
eval(expression: str) -> Any
```

```lua
eval(expression: string) -> any
```

**参数**：
- `expression` - 要计算的表达式

**返回**：
- 计算结果

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 检查局部变量
locals_dict = debugger.get_locals()
print("局部变量:", locals_dict)

# 检查全局变量
globals_dict = debugger.get_globals()
print("全局变量:", globals_dict)

# 检查表达式
result = debugger.eval("x + y")
print(f"x + y = {result}")
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 检查局部变量
local locals = debugger.getLocals()
print("局部变量:", locals)

-- 检查全局变量
local globals = debugger.getGlobals()
print("全局变量:", globals)

-- 检查表达式
local result = debugger.eval("x + y")
print("x + y = " .. tostring(result))
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `breakpoint()` | `breakpoint()` | 设置断点 | 无返回值 |
| `breakpoint_if(condition)` | `breakpointIf(condition)` | 条件断点 | condition: 条件函数 |
| `log_point(message, ...args)` | `logPoint(message, ...)` | 日志断点 | message: 消息<br>args: 参数 |
| `start_server(port?)` | `startServer(port?)` | 启动调试服务器 | port: 端口(默认9999) |
| `connect(host?, port?)` | `connect(host?, port?)` | 连接调试器 | host: 主机(默认localhost)<br>port: 端口(默认9999)<br>返回: 是否成功 |
| `log(...)` | `log(...)` | 输出调试信息 | args: 可变参数 |
| `inspect(data)` | `inspect(data)` | 检查变量 | data: 要检查的数据 |
| `get_stacktrace()` | `getStacktrace()` | 获取调用栈 | 返回: 调用栈帧列表 |
| `step_into()` | `stepInto()` | 单步进入 | 无返回值 |
| `step_over()` | `stepOver()` | 单步跳过 | 无返回值 |
| `step_out()` | `stepOut()` | 跳出函数 | 无返回值 |
| `resume()` | `resume()` | 继续执行 | 无返回值 |
| `get_locals()` | `getLocals()` | 获取局部变量 | 返回: 变量字典/表格 |
| `get_globals()` | `getGlobals()` | 获取全局变量 | 返回: 变量字典/表格 |
| `eval(expression)` | `eval(expression)` | 计算表达式 | expression: 表达式<br>返回: 计算结果 |

---

## VS Code 调试配置

在 `.vscode/launch.json` 中添加以下配置：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "emmylua_new",
      "request": "launch",
      "name": "Attach to Wingman Lua",
      "host": "localhost",
      "port": 9966,
      "ideConnectDebugger": true,
      "cwd": "${workspaceFolder}",
      "ext": [".lua", ".lua.txt", ".lua.bytes"]
    }
  ]
}
```
