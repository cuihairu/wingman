# API: wingman.debugger

调试器模块，提供断点调试和脚本调试功能。

## 设置断点

:::tabs

== Python

```python:line-numbers
from wingman import debugger

# 在当前行设置断点
debugger.breakpoint()

# 设置条件断点
debugger.breakpoint_if(lambda: x > 10)

# 设置日志断点
debugger.log_point("变量 x 的值: {}", x)
```

== Lua

```lua:line-numbers
local debugger = require("wingman.debugger")

-- 在当前行设置断点
debugger.breakpoint()

-- 设置条件断点
debugger.breakpointIf(function() return x > 10 end)

-- 设置日志断点
debugger.logPoint("变量 x 的值: " .. tostring(x), x)
```

:::

## 启动调试服务器

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

## 连接调试器

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

## 输出调试信息

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

## 获取调用栈

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

## 单步执行

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

## 检查变量

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

## VS Code 调试配置

在 `.vscode/launch.json` 中添加以下配置：

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "wingman-lua",
      "request": "launch",
      "name": "Wingman Lua 调试",
      "script": "${workspaceFolder}/scripts/main.lua",
      "host": "localhost",
      "port": 9999,
      "stopOnEntry": false
    },
    {
      "type": "wingman-python",
      "request": "launch",
      "name": "Wingman Python 调试",
      "script": "${workspaceFolder}/scripts/main.py",
      "host": "localhost",
      "port": 9999,
      "stopOnEntry": false
    }
  ]
}
```

---

## 可用接口

### `breakpoint()` / `breakpoint()`

在当前行设置断点。

### `breakpoint_if(condition)` / `breakpointIf(condition)`

设置条件断点。

**参数：**
- `condition` / `Condition` - 条件函数，返回 True 时触发断点

### `log_point(message, ...args)` / `logPoint(message, ...)`

设置日志断点。

**参数：**
- `message` - 日志消息（支持 {} 占位符）
- `...args` - 占位符参数

### `start_server(port)` / `startServer(port)`

启动调试服务器。

**参数：**
- `port` - 监听端口

### `connect(host, port)` / `connect(host, port)`

连接到远程调试器。

**参数：**
- `host` - 主机地址
- `port` - 端口号

### `log(...)` / `log(...)`

输出调试信息。

### `inspect(data)` / `inspect(data)`

输出变量信息。

**参数：**
- `data` - 要检查的数据

### `get_stacktrace()` / `getStacktrace()`

获取当前调用栈。

### `step_into()` / `stepInto()`

单步进入。

### `step_over()` / `stepOver()`

单步跳过。

### `step_out()` / `stepOut()`

跳出当前函数。

### `resume()`

继续执行。

### `get_locals()` / `getLocals()`

检查局部变量。

### `get_globals()` / `getGlobals()`

检查全局变量。

### `eval(expression)` / `eval(expression)`

检查表达式。

**参数：**
- `expression` - 要计算的表达式

**返回：**
- 计算结果
