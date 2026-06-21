# API: wingman.process

进程管理模块，提供进程查找、启动、终止等功能。

> **💡 提示**：查看 [数据类型参考](./types.md) 了解进程 ID（PID）的详细说明。

## 模块概述

process 模块用于管理与操作系统进程相关的操作。主要功能包括：

- **进程查找**：根据进程名称查找正在运行的进程
- **进程启动**：启动新的应用程序或命令
- **进程终止**：终止正在运行的进程
- **进程监控**：等待进程出现或结束，检查进程是否存在

### 进程 ID（PID）

每个正在运行的进程都有一个唯一的进程标识符（PID），在 Wingman 中用数字表示。

---

## 查找进程

### find(name)

**说明**：根据进程名称查找正在运行的进程。

**函数签名**：

```python
find(name: str) -> tuple[int, bool]
```

```lua
find(name: string) -> number, boolean
```

**参数**：
- `name` - 进程名称（不需要 .exe 后缀，支持部分匹配）

**返回**：
- Python: `(pid, found)` 元组，pid 为进程 ID，found 表示是否找到
- Lua: `pid, found` 两个返回值

:::tabs

== Python

```python:line-numbers
from wingman import process

# 查找记事本进程
pid, found = process.find("notepad")
if found:
    print(f"找到记事本进程，PID: {pid}")
else:
    print("记事本未运行")

# 查找 Chrome 进程（匹配包含 "chrome" 的进程名）
pid, found = process.find("chrome")
if found:
    print(f"找到 Chrome 进程，PID: {pid}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找记事本进程
local pid, found = wingman.process.find("notepad")
if found then
    print("找到记事本进程，PID:", pid)
else
    print("记事本未运行")
end

-- 查找 Chrome 进程
local pid, found = wingman.process.find("chrome")
if found then
    print("找到 Chrome 进程，PID:", pid)
end
```

:::

---

## 启动进程

### start(path, args?, working_dir?) / start(path, args?, workingDir?)

**说明**：启动一个新的进程。

**函数签名**：

```python
start(path: str, args: str = None, working_dir: str = None) -> int
```

```lua
start(path: string, args: string | nil, workingDir: string | nil) -> number
```

**参数**：
- `path` - 可执行文件路径
- `args` / `args` - 可选，命令行参数
- `working_dir` / `workingDir` - 可选，工作目录

**返回**：新进程的 PID

:::tabs

== Python

```python:line-numbers
from wingman import process

# 启动记事本
pid = process.start("notepad.exe")
print(f"记事本已启动，PID: {pid}")

# 启动带参数的程序
pid = process.start("cmd.exe", args="/c dir C:\\")
print(f"命令已执行，PID: {pid}")

# 指定工作目录
pid = process.start("cmd.exe", working_dir="C:\\Temp")

# 同时指定参数和工作目录
pid = process.start(
    "python.exe",
    args="script.py --verbose",
    working_dir="C:\\Scripts"
)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 启动记事本
local pid = wingman.process.start("notepad.exe")
print("记事本已启动，PID:", pid)

-- 启动带参数的程序
local pid = wingman.process.start("cmd.exe", "/c dir C:\\")
print("命令已执行，PID:", pid)

-- 指定工作目录
local pid = wingman.process.start("cmd.exe", nil, "C:\\Temp")

-- 同时指定参数和工作目录
local pid = wingman.process.start(
    "python.exe",
    "script.py --verbose",
    "C:\\Scripts"
)
```

:::

---

## 等待进程结束

### wait(pid, timeout?)

**说明**：等待指定进程结束。

**函数签名**：

```python
wait(pid: int, timeout: int = 0) -> bool
```

```lua
wait(pid: number, timeout: number = 0) -> boolean
```

**参数**：
- `pid` - 进程 ID
- `timeout` - 可选，超时时间（毫秒），0 表示无限等待

**返回**：进程是否已结束（超时返回 false）

:::tabs

== Python

```python:line-numbers
from wingman import process

pid = process.start("notepad.exe")

# 无限等待进程结束
if process.wait(pid):
    print("进程已结束")

# 等待最多 5 秒
if process.wait(pid, 5000):
    print("进程在 5 秒内结束")
else:
    print("等待超时，进程仍在运行")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local pid = wingman.process.start("notepad.exe")

-- 无限等待进程结束
if wingman.process.wait(pid) then
    print("进程已结束")
end

-- 等待最多 5 秒
if wingman.process.wait(pid, 5000) then
    print("进程在 5 秒内结束")
else
    print("等待超时，进程仍在运行")
end
```

:::

---

## 终止进程

### terminate(pid, force)

**说明**：终止正在运行的进程。

**函数签名**：

```python
terminate(pid: int, force: bool) -> bool
```

```lua
terminate(pid: number, force: boolean) -> boolean
```

**参数**：
- `pid` - 进程 ID
- `force` - 是否强制终止

**返回**：是否成功终止

**force 参数说明**：
- `force=False/False` - 尝试正常关闭（发送关闭消息）
- `force=True/True` - 强制终止（立即结束进程）

:::tabs

== Python

```python:line-numbers
from wingman import process

# 查找记事本进程
pid, found = process.find("notepad")
if found:
    # 尝试正常关闭
    if process.terminate(pid, force=False):
        print("进程已正常终止")

# 强制终止进程
pid, found = process.find("notepad")
if found:
    if process.terminate(pid, force=True):
        print("进程已被强制终止")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 查找记事本进程
local pid, found = wingman.process.find("notepad")
if found then
    -- 尝试正常关闭
    if wingman.process.terminate(pid, false) then
        print("进程已正常终止")
    end
end

-- 强制终止进程
local pid, found = wingman.process.find("notepad")
if found then
    if wingman.process.terminate(pid, true) then
        print("进程已被强制终止")
    end
end
```

:::

---

## 检查进程是否存在

### exists(pid)

**说明**：检查指定进程是否正在运行。

**函数签名**：

```python
exists(pid: int) -> bool
```

```lua
exists(pid: number) -> boolean
```

**参数**：
- `pid` - 进程 ID

**返回**：进程是否存在

:::tabs

== Python

```python:line-numbers
from wingman import process

pid = 1234
if process.exists(pid):
    print(f"进程 {pid} 正在运行")
else:
    print(f"进程 {pid} 不存在")

# 检查刚启动的进程
pid = process.start("notepad.exe")
if process.exists(pid):
    print("记事本正在运行")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

local pid = 1234
if wingman.process.exists(pid) then
    print("进程 " .. pid .. " 正在运行")
else
    print("进程 " .. pid .. " 不存在")
end

-- 检查刚启动的进程
local pid = wingman.process.start("notepad.exe")
if wingman.process.exists(pid) then
    print("记事本正在运行")
end
```

:::

---

## 等待进程出现

### wait_for(name, timeout?) / waitFor(name, timeout?)

**说明**：等待指定名称的进程出现。

**函数签名**：

```python
wait_for(name: str, timeout: int = 5000) -> bool
```

```lua
waitFor(name: string, timeout: number = 5000) -> boolean
```

**参数**：
- `name` - 进程名称
- `timeout` - 可选，超时时间（毫秒），默认 5000

**返回**：是否在超时前找到进程

:::tabs

== Python

```python:line-numbers
from wingman import process

# 启动应用程序
process.start("notepad.exe")

# 等待进程出现
if process.wait_for("notepad", 5000):
    print("记事本进程已启动")
    pid, found = process.find("notepad")
    if found:
        print(f"PID: {pid}")
else:
    print("超时：进程未启动")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 启动应用程序
wingman.process.start("notepad.exe")

-- 等待进程出现
if wingman.process.waitFor("notepad", 5000) then
    print("记事本进程已启动")
    local pid, found = wingman.process.find("notepad")
    if found then
        print("PID:", pid)
    end
else
    print("超时：进程未启动")
end
```

:::

---

## 完整示例

### 进程管理脚本

这个示例展示了如何检查、启动、监控和终止进程：

:::tabs

== Python

```python:line-numbers
from wingman import process, util

# 检查记事本是否运行
pid, found = process.find("notepad")
if not found:
    print("记事本未运行，正在启动...")
    pid = process.start("notepad.exe")
    util.sleep(500)

# 获取进程信息
pid, found = process.find("notepad")
if found:
    print(f"记事本 PID: {pid}")

    # 检查进程是否存在
    if process.exists(pid):
        print("进程正在运行")

    # 等待 5 秒后终止
    print("等待 5 秒...")
    util.sleep(5000)

    # 尝试正常关闭
    if process.terminate(pid, force=False):
        print("进程已终止")

    # 检查是否已结束
    util.sleep(500)
    if not process.exists(pid):
        print("进程已成功结束")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 检查记事本是否运行
local pid, found = wingman.process.find("notepad")
if not found then
    print("记事本未运行，正在启动...")
    pid = wingman.process.start("notepad.exe")
    wingman.util.sleep(500)
end

-- 获取进程信息
pid, found = wingman.process.find("notepad")
if found then
    print("记事本 PID:", pid)

    -- 检查进程是否存在
    if wingman.process.exists(pid) then
        print("进程正在运行")
    end

    -- 等待 5 秒后终止
    print("等待 5 秒...")
    wingman.util.sleep(5000)

    -- 尝试正常关闭
    if wingman.process.terminate(pid, false) then
        print("进程已终止")
    end

    -- 检查是否已结束
    wingman.util.sleep(500)
    if not wingman.process.exists(pid) then
        print("进程已成功结束")
    end
end
```

:::

---

## 可用接口

### 进程查找与启动

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `find(name)` | `find(name)` | 查找进程 | name: 进程名<br>返回: (pid, found) |
| `start(path, args?, working_dir?)` | `start(path, args?, workingDir?)` | 启动进程 | path: 可执行路径<br>args: 命令行参数<br>working_dir: 工作目录<br>返回: PID |
| `wait_for(name, timeout?)` | `waitFor(name, timeout?)` | 等待进程 | name: 进程名<br>timeout: 超时(ms) |

### 进程控制

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `wait(pid, timeout?)` | `wait(pid, timeout?)` | 等待结束 | pid: 进程ID<br>timeout: 超时(ms, 0=无限)<br>返回: 是否结束 |
| `terminate(pid, force)` | `terminate(pid, force)` | 终止进程 | pid: 进程ID<br>force: 是否强制<br>返回: 是否成功 |
| `exists(pid)` | `exists(pid)` | 检查存在 | pid: 进程ID<br>返回: 是否存在 |
