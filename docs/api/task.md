# Task API

`wingman.task` 提供异步任务提交与生命周期管理功能，支持重试、超时等特性。

## Python

```python
from wingman import task

# 提交任务
def my_work(ctx):
    # 执行一些耗时操作
    return {"result": 42}

task_id = task.submit(my_work, {
    "timeoutMs": 5000,
    "retry": {"max": 3, "backoffMs": 500}
})

# 检查状态
status = task.status(task_id)  # "running", "succeeded", "failed", etc.

# 等待完成
if task.wait(task_id, 10000):
    result = task.result(task_id)
    print(f"任务完成: {result}")
else:
    error = task.error(task_id)
    print(f"任务失败: {error}")

# 取消任务
task.cancel(task_id)
```

## Lua

```lua
local task = require("wingman.task")

local function myWork(ctx)
    -- 执行操作
    return { result = 42 }
end

local taskId = task.submit(myWork, {
    timeoutMs = 5000,
    maxRetries = 3,
    backoffMs = 500
})

local status = task.status(taskId)

if task.wait(taskId, 10000) then
    local result = task.result(taskId)
    print("任务完成: " .. result.result)
else
    local err = task.error(taskId)
    print("任务失败: " .. err)
end

task.cancel(taskId)
```

## 可用接口

### `submit(work, options?)`

提交一个异步任务。

- `work`: 工作函数，将在后台线程执行
- `options`: 可选配置
  - `timeoutMs`: 超时时间（毫秒），默认 30000
  - `maxRetries`: 最大重试次数，默认 0
  - `backoffMs`: 重试退避基数（毫秒），默认 500
  - `backoffFactor`: 退避因子，默认 2.0
  - `metadata`: 元数据（任意 JSON 对象）
- **返回**: 任务 ID

### `cancel(taskId)`

取消任务。

- `taskId`: 任务 ID
- **返回**: 是否成功

### `status(taskId)`

获取任务状态。

- `taskId`: 任务 ID
- **返回**: 状态字符串：`"pending"`, `"running"`, `"succeeded"`, `"failed"`, `"canceled"`, `"timeout"`

### `wait(taskId, timeoutMs?)`

等待任务完成。

- `taskId`: 任务 ID
- `timeoutMs`: 可选，等待超时（毫秒），默认 30000
- **返回**: 是否在超时前完成

### `result(taskId)`

获取任务结果。

- `taskId`: 任务 ID
- **返回**: 任务返回值，如果任务失败或未完成返回 `null`

### `error(taskId)`

获取任务错误信息。

- `taskId`: 任务 ID
- **返回**: 错误信息字符串，无错误返回空字符串

### `retry(taskId, options?)`

重试失败的任务。

- `taskId`: 任务 ID
- `options`: 可选重试配置
  - `maxRetries`: 最大重试次数
  - `backoffMs`: 重试退避基数
- **返回**: 是否成功发起重试

## 生命周期事件

任务生命周期会触发以下事件：

```python
event.on("task.submitted", lambda e: print(f"任务提交: {e['payload']['taskId']}"))
event.on("task.started", lambda e: print(f"任务开始"))
event.on("task.succeeded", lambda e: print(f"任务成功"))
event.on("task.failed", lambda e: print(f"任务失败: {e['payload']['error']}"))
event.on("task.canceled", lambda e: print(f"任务取消"))
event.on("task.timeout", lambda e: print(f"任务超时"))
```

事件载荷包含：
- `taskId`: 任务 ID
- `status`: 当前状态
- `metadata`: 元数据
