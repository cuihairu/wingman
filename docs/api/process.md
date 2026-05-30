# API: wingman.process

进程管理模块。

## 查找进程

:::tabs

== Python

```python
from wingman import process

pid, found = process.find("notepad")
if found:
    print(f"找到记事本进程，PID: {pid}")
```

== Lua

```lua
local process = require("wingman.process")

local pid, found = process.find("notepad")
if found then
    print("找到记事本进程，PID: " .. pid)
end
```

:::

## 启动进程

:::tabs

== Python

```python
from wingman import process

# 启动记事本
pid = process.start("notepad.exe")

# 启动带参数的程序
pid = process.start("cmd.exe", args="/c dir C:\\")

# 指定工作目录
pid = process.start("cmd.exe", working_dir="C:\\Temp")
```

== Lua

```lua
local process = require("wingman.process")

-- 启动记事本
local pid = process.start("notepad.exe")

-- 启动带参数的程序
local pid = process.start("cmd.exe", "/c dir C:\\")

-- 指定工作目录
local pid = process.start("cmd.exe", "", "C:\\Temp")
```

:::

## 等待进程结束

:::tabs

== Python

```python
from wingman import process

pid = process.start("notepad.exe")

# 等待进程结束（无限等待）
if process.wait(pid):
    print("进程已结束")

# 带超时等待
if process.wait(pid, 5000):
    print("进程在5秒内结束")
else:
    print("等待超时")
```

== Lua

```lua
local process = require("wingman.process")

local pid = process.start("notepad.exe")

-- 等待进程结束（无限等待）
if process.wait(pid) then
    print("进程已结束")
end

-- 带超时等待
if process.wait(pid, 5000) then
    print("进程在5秒内结束")
else
    print("等待超时")
end
```

:::

## 终止进程

:::tabs

== Python

```python
from wingman import process

pid, found = process.find("notepad")
if found:
    if process.terminate(pid, force=False):
        print("进程已终止")
```

== Lua

```lua
local process = require("wingman.process")

local pid, found = process.find("notepad")
if found then
    if process.terminate(pid, false) then
        print("进程已终止")
    end
end
```

:::

## 检查进程是否存在

:::tabs

== Python

```python
from wingman import process

pid = 1234
if process.exists(pid):
    print(f"进程 {pid} 正在运行")
else:
    print(f"进程 {pid} 不存在")
```

== Lua

```lua
local process = require("wingman.process")

local pid = 1234
if process.exists(pid) then
    print("进程 " .. pid .. " 正在运行")
else
    print("进程 " .. pid .. " 不存在")
end
```

:::

## 等待进程出现

:::tabs

== Python

```python
from wingman import process

# 启动应用程序
process.start("notepad.exe")

# 等待进程出现
if process.wait_for("notepad", 5000):
    print("记事本进程已启动")
    pid, found = process.find("notepad")
    print(f"PID: {pid}")
else:
    print("超时：进程未启动")
```

== Lua

```lua
local process = require("wingman.process")

-- 启动应用程序
process.start("notepad.exe")

-- 等待进程出现
if process.waitFor("notepad", 5000) then
    print("记事本进程已启动")
    local pid, found = process.find("notepad")
    print("PID: " .. pid)
else
    print("超时：进程未启动")
end
```

:::

---

## 完整示例

:::tabs

== Python

```python
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

    # 等待5秒后终止
    util.sleep(5000)
    if process.terminate(pid, force=False):
        print("进程已终止")
```

== Lua

```lua
local process = require("wingman.process")
local util = require("wingman.util")

-- 检查记事本是否运行
local pid, found = process.find("notepad")
if not found then
    print("记事本未运行，正在启动...")
    pid = process.start("notepad.exe")
    util.sleep(500)
end

-- 获取进程信息
pid, found = process.find("notepad")
if found then
    print("记事本 PID: " .. pid)

    -- 检查进程是否存在
    if process.exists(pid) then
        print("进程正在运行")
    end

    -- 等待5秒后终止
    util.sleep(5000)
    if process.terminate(pid, false) then
        print("进程已终止")
    end
end
```

:::

---

## 可用接口

### `find(name)`

查找指定名称的进程。

**参数：**
- `name` (string) - 进程名称（不需要 .exe 后缀）

**返回：**
- `(number, boolean)` - 进程 ID (PID)，是否找到

### `start(path, args?, working_dir?)`

启动新进程。

**参数：**
- `path` (string) - 可执行文件路径
- `args` (string, 可选) - 命令行参数
- `working_dir` (string, 可选) - 工作目录

**返回：**
- `number` - 新进程的 PID

### `wait(pid, timeout?)`

等待进程结束。

**参数：**
- `pid` (number) - 进程 ID
- `timeout` (number, 可选) - 超时时间（毫秒），0 表示无限等待

**返回：**
- `boolean` - 进程是否已结束（超时返回 false）

### `terminate(pid, force)`

终止进程。

**参数：**
- `pid` (number) - 进程 ID
- `force` (boolean) - 是否强制终止

**返回：**
- `boolean` - 是否成功

### `exists(pid)`

检查进程是否存在。

**参数：**
- `pid` (number) - 进程 ID

**返回：**
- `boolean` - 进程是否存在

### `wait_for(name, timeout?)` / `waitFor(name, timeout?)`

等待进程出现。

**参数：**
- `name` (string) - 进程名称
- `timeout` (number, 可选) - 超时时间（毫秒），默认 5000

**返回：**
- `boolean` - 是否在超时前找到进程
