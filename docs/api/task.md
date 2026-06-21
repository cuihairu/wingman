# API: wingman.task

异步任务模块，提供异步任务提交与生命周期管理功能，支持重试、超时等特性。

## 模块概述

task 模块提供异步任务管理功能：
- **提交任务** - 提交异步任务到后台执行
- **状态查询** - 获取任务状态
- **等待完成** - 等待任务执行完成
- **获取结果** - 获取任务执行结果
- **取消任务** - 取消正在执行的任务
- **重试任务** - 重试失败的任务

---

## 提交任务

### submit(work, options?) / submit(work, options?)

**说明**：提交一个异步任务。

**函数签名**：

```python
submit(work: Callable, options: dict = None) -> str
```

```lua
submit(work: function, options: table = nil) -> string
```

**参数**：
- `work` - 工作函数，将在后台线程执行，接收上下文对象
- `options` - 可选配置
  - `timeoutMs` / `timeoutMs` - 超时时间（毫秒），默认 30000
  - `maxRetries` / `maxRetries` - 最大重试次数，默认 0
  - `backoffMs` / `backoffMs` - 重试退避基数（毫秒），默认 500
  - `backoffFactor` / `backoffFactor` - 退避因子，默认 2.0
  - `metadata` / `metadata` - 元数据（任意对象）

**返回**：
- 任务 ID

:::tabs

== Python

```python:line-numbers
from wingman import task

# 提交任务
def my_work(ctx):
    # 执行一些耗时操作
    return {"result": 42}

task_id = task.submit(my_work, {
    "timeoutMs": 5000,
    "retry": {"max": 3, "backoffMs": 500}
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 提交任务
local function myWork(ctx)
    -- 执行操作
    return { result = 42 }
end

local taskId = wingman.task.submit(myWork, {
    timeoutMs = 5000,
    maxRetries = 3,
    backoffMs = 500
})
```

:::

---

## 获取任务状态

### status(task_id) / status(taskId)

**说明**：获取任务状态。

**函数签名**：

```python
status(task_id: str) -> str
```

```lua
status(taskId: string) -> string
```

**参数**：
- `task_id` / `taskId` - 任务 ID

**返回**：
- 状态字符串：`"pending"`, `"running"`, `"succeeded"`, `"failed"`, `"canceled"`, `"timeout"`

:::tabs

== Python

```python:line-numbers
from wingman import task

# 检查状态
status = task.status(task_id)
print(f"任务状态: {status}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 检查状态
local status = wingman.task.status(taskId)
print("任务状态: " .. status)
```

:::

---

## 等待任务完成

### wait(task_id, timeout_ms?) / wait(taskId, timeoutMs?)

**说明**：等待任务完成。

**函数签名**：

```python
wait(task_id: str, timeout_ms: int = 30000) -> bool
```

```lua
wait(taskId: string, timeoutMs: number = 30000) -> boolean
```

**参数**：
- `task_id` / `taskId` - 任务 ID
- `timeout_ms` / `timeoutMs` - 可选，等待超时（毫秒），默认 30000

**返回**：
- 是否在超时前完成

:::tabs

== Python

```python:line-numbers
from wingman import task

# 等待完成
if task.wait(task_id, 10000):
    result = task.result(task_id)
    print(f"任务完成: {result}")
else:
    error = task.error(task_id)
    print(f"任务失败: {error}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 等待完成
if wingman.task.wait(taskId, 10000) then
    local result = wingman.task.result(taskId)
    print("任务完成: " .. result.result)
else
    local err = wingman.task.error(taskId)
    print("任务失败: " .. err)
end
```

:::

---

## 获取任务结果

### result(task_id) / result(taskId)

**说明**：获取任务结果。

**函数签名**：

```python
result(task_id: str) -> Any
```

```lua
result(taskId: string) -> any
```

**参数**：
- `task_id` / `taskId` - 任务 ID

**返回**：
- Python: 任务返回值，如果任务失败或未完成返回 `None`
- Lua: 任务返回值，如果任务失败或未完成返回 `nil`

---

## 获取错误信息

### error(task_id) / error(taskId)

**说明**：获取任务错误信息。

**函数签名**：

```python
error(task_id: str) -> str
```

```lua
error(taskId: string) -> string
```

**参数**：
- `task_id` / `taskId` - 任务 ID

**返回**：
- 错误信息字符串，无错误返回空字符串

---

## 取消任务

### cancel(task_id) / cancel(taskId)

**说明**：取消任务。

**函数签名**：

```python
cancel(task_id: str) -> bool
```

```lua
cancel(taskId: string) -> boolean
```

**参数**：
- `task_id` / `taskId` - 任务 ID

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import task

# 取消任务
task.cancel(task_id)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 取消任务
wingman.task.cancel(taskId)
```

:::

---

## 重试任务

### retry(task_id, options?) / retry(taskId, options?)

**说明**：重试失败的任务。

**函数签名**：

```python
retry(task_id: str, options: dict = None) -> bool
```

```lua
retry(taskId: string, options: table = nil) -> boolean
```

**参数**：
- `task_id` / `taskId` - 任务 ID
- `options` - 可选重试配置
  - `maxRetries` / `maxRetries` - 最大重试次数
  - `backoffMs` / `backoffMs` - 重试退避基数

**返回**：
- 是否成功发起重试

:::tabs

== Python

```python:line-numbers
from wingman import task

# 重试失败的任务
task.retry(task_id, {"max": 5, "backoffMs": 1000})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 重试失败的任务
wingman.task.retry(taskId, { maxRetries = 5, backoffMs = 1000 })
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `submit(work, options?)` | `submit(work, options?)` | 提交任务 | work: 工作函数<br>options: 配置(可选)<br>返回: 任务ID |
| `cancel(taskId)` | `cancel(taskId)` | 取消任务 | taskId: 任务ID<br>返回: 是否成功 |
| `status(taskId)` | `status(taskId)` | 获取状态 | taskId: 任务ID<br>返回: 状态字符串 |
| `wait(taskId, timeoutMs?)` | `wait(taskId, timeoutMs?)` | 等待完成 | taskId: 任务ID<br>timeoutMs: 等待超时(默认30000)<br>返回: 是否完成 |
| `result(taskId)` | `result(taskId)` | 获取结果 | taskId: 任务ID<br>返回: 任务结果 |
| `error(taskId)` | `error(taskId)` | 获取错误 | taskId: 任务ID<br>返回: 错误信息 |
| `retry(taskId, options?)` | `retry(taskId, options?)` | 重试任务 | taskId: 任务ID<br>options: 重试配置(可选)<br>返回: 是否成功 |

---

## 生命周期事件

任务生命周期会触发以下事件：

| 事件名 | 说明 | 载荷字段 |
|--------|------|---------|
| `task.submitted` | 任务已提交 | taskId, metadata |
| `task.started` | 任务开始执行 | taskId, metadata |
| `task.succeeded` | 任务成功 | taskId, metadata |
| `task.failed` | 任务失败 | taskId, metadata, error |
| `task.canceled` | 任务已取消 | taskId, metadata |
| `task.timeout` | 任务超时 | taskId, metadata, error |

:::tabs

== Python

```python:line-numbers
from wingman import event

event.on("task.submitted", lambda e: print(f"任务提交: {e['payload']['taskId']}"))
event.on("task.started", lambda e: print("任务开始"))
event.on("task.succeeded", lambda e: print("任务成功"))
event.on("task.failed", lambda e: print(f"任务失败: {e['payload']['error']}"))
event.on("task.canceled", lambda e: print("任务取消"))
event.on("task.timeout", lambda e: print("任务超时"))
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

wingman.event.on("task.submitted", function(e)
    print("任务提交: " .. e.payload.taskId)
end)
wingman.event.on("task.started", function(e)
    print("任务开始")
end)
wingman.event.on("task.succeeded", function(e)
    print("任务成功")
end)
wingman.event.on("task.failed", function(e)
    print("任务失败: " .. e.payload.error)
end)
wingman.event.on("task.canceled", function(e)
    print("任务取消")
end)
wingman.event.on("task.timeout", function(e)
    print("任务超时")
end)
```

:::
