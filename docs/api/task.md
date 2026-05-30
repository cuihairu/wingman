# Task API

`wingman.task` 提供异步任务提交与生命周期管理功能，支持重试、超时等特性。

## 提交任务

::: code-group

```python [Python]
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

```lua [Lua]
local task = require("wingman.task")

-- 提交任务
local function myWork(ctx)
    -- 执行操作
    return { result = 42 }
end

local taskId = task.submit(myWork, {
    timeoutMs = 5000,
    maxRetries = 3,
    backoffMs = 500
})
```

:::

## 检查任务状态

::: code-group

```python [Python]
from wingman import task

# 检查状态
status = task.status(task_id)
print(f"任务状态: {status}")  # "running", "succeeded", "failed", etc.
```

```lua [Lua]
local task = require("wingman.task")

-- 检查状态
local status = task.status(taskId)
print("任务状态: " .. status)
```

:::

## 等待任务完成

::: code-group

```python [Python]
from wingman import task

# 等待完成
if task.wait(task_id, 10000):
    result = task.result(task_id)
    print(f"任务完成: {result}")
else:
    error = task.error(task_id)
    print(f"任务失败: {error}")
```

```lua [Lua]
local task = require("wingman.task")

-- 等待完成
if task.wait(taskId, 10000) then
    local result = task.result(taskId)
    print("任务完成: " .. result.result)
else
    local err = task.error(taskId)
    print("任务失败: " .. err)
end
```

:::

## 取消任务

::: code-group

```python [Python]
from wingman import task

# 取消任务
task.cancel(task_id)
```

```lua [Lua]
local task = require("wingman.task")

-- 取消任务
task.cancel(taskId)
```

:::

## 重试任务

::: code-group

```python [Python]
from wingman import task

# 重试失败的任务
task.retry(task_id, {"max": 5, "backoffMs": 1000})
```

```lua [Lua]
local task = require("wingman.task")

-- 重试失败的任务
task.retry(taskId, { maxRetries = 5, backoffMs = 1000 })
```

:::

---

## 完整示例

### 异步图像处理任务

::: code-group

```python [Python]
from wingman import task, screen, vision

def process_image(ctx):
    """在后台线程处理图像"""
    # 截图
    img = screen.capture(0, 0, 1920, 1080)

    # 耗时操作：查找所有目标
    targets = vision.find_all_colors(img, (255, 0, 0))

    return {"count": len(targets), "targets": targets}

# 提交任务
task_id = task.submit(process_image, {"timeoutMs": 10000})

# 在等待期间可以做其他事情
print("任务已提交，继续执行其他操作...")

# 等待结果
if task.wait(task_id, 15000):
    result = task.result(task_id)
    print(f"找到 {result['count']} 个目标")
else:
    print("图像处理超时")
```

```lua [Lua]
local task = require("wingman.task")
local screen = require("wingman.screen")
local vision = require("wingman.vision")

local function processImage(ctx)
    -- 在后台线程处理图像
    local img = screen.capture(0, 0, 1920, 1080)
    local targets = vision.findAllColors(img, {255, 0, 0})
    return { count = #targets, targets = targets }
end

-- 提交任务
local taskId = task.submit(processImage, { timeoutMs = 10000 })

-- 在等待期间可以做其他事情
print("任务已提交，继续执行其他操作...")

-- 等待结果
if task.wait(taskId, 15000) then
    local result = task.result(taskId)
    print("找到 " .. result.count .. " 个目标")
else
    print("图像处理超时")
end
```

:::

### 带重试的网络请求

::: code-group

```python [Python]
from wingman import task, http

def fetch_api(ctx):
    """可能失败的网络请求"""
    response = http.get("https://api.example.com/data")

    if response['status'] != 200:
        raise Exception(f"HTTP {response['status']}")

    return response['json']

# 提交带重试的任务
task_id = task.submit(fetch_api, {
    "timeoutMs": 5000,
    "retry": {"max": 3, "backoffMs": 1000}
})

if task.wait(task_id, 20000):
    result = task.result(task_id)
    print(f"获取数据成功: {result}")
else:
    error = task.error(task_id)
    print(f"请求失败: {error}")
```

```lua [Lua]
local task = require("wingman.task")
local http = require("wingman.http")

local function fetchApi(ctx)
    -- 可能失败的网络请求
    local response = http.get("https://api.example.com/data")

    if response.status ~= 200 then
        error("HTTP " .. response.status)
    end

    return response.json
end

-- 提交带重试的任务
local taskId = task.submit(fetchApi, {
    timeoutMs = 5000,
    maxRetries = 3,
    backoffMs = 1000
})

if task.wait(taskId, 20000) then
    local result = task.result(taskId)
    print("获取数据成功: " .. result.value)
else
    local err = task.error(taskId)
    print("请求失败: " .. err)
end
```

:::

### 批量任务处理

::: code-group

```python [Python]
from wingman import task, vision, input

def click_target(ctx):
    """点击单个目标"""
    x, y = ctx['target']
    input.click(x, y)
    return {"clicked": (x, y)}

# 批量提交任务
targets = vision.find_all_colors((255, 0, 0))
task_ids = []

for target in targets:
    task_id = task.submit(click_target, {
        "metadata": {"target": target}
    })
    task_ids.append(task_id)

# 等待所有任务完成
results = []
for task_id in task_ids:
    if task.wait(task_id, 5000):
        results.append(task.result(task_id))

print(f"已点击 {len(results)} 个目标")
```

```lua [Lua]
local task = require("wingman.task")
local vision = require("wingman.vision")
local input = require("wingman.input")

local function clickTarget(ctx)
    -- 点击单个目标
    local t = ctx.target
    input.click(t.x, t.y)
    return { clicked = {x = t.x, y = t.y} }
end

-- 批量提交任务
local targets = vision.findAllColors({255, 0, 0})
local taskIds = {}

for i, target in ipairs(targets) do
    local taskId = task.submit(clickTarget, {
        metadata = { target = target }
    })
    table.insert(taskIds, taskId)
end

-- 等待所有任务完成
local results = {}
for i, taskId in ipairs(taskIds) do
    if task.wait(taskId, 5000) then
        table.insert(results, task.result(taskId))
    end
end

print("已点击 " .. #results .. " 个目标")
```

:::

---

## 生命周期事件

任务生命周期会触发以下事件：

::: code-group

```python [Python]
from wingman import event

event.on("task.submitted", lambda e: print(f"任务提交: {e['payload']['taskId']}"))
event.on("task.started", lambda e: print(f"任务开始"))
event.on("task.succeeded", lambda e: print(f"任务成功"))
event.on("task.failed", lambda e: print(f"任务失败: {e['payload']['error']}"))
event.on("task.canceled", lambda e: print(f"任务取消"))
event.on("task.timeout", lambda e: print(f"任务超时"))
```

```lua [Lua]
local event = require("wingman.event")

event.on("task.submitted", function(e)
    print("任务提交: " .. e.payload.taskId)
end)
event.on("task.started", function(e)
    print("任务开始")
end)
event.on("task.succeeded", function(e)
    print("任务成功")
end)
event.on("task.failed", function(e)
    print("任务失败: " .. e.payload.error)
end)
event.on("task.canceled", function(e)
    print("任务取消")
end)
event.on("task.timeout", function(e)
    print("任务超时")
end)
```

:::

事件载荷包含：
- `taskId`: 任务 ID
- `status`: 当前状态
- `metadata`: 元数据
- `error`: 错误信息（仅失败/超时时）

---

## 可用接口

### `submit(work, options?)` / `submit(work, options?)`

提交一个异步任务。

**参数：**
- `work` - 工作函数，将在后台线程执行
- `options` - 可选配置
  - `timeoutMs` - 超时时间（毫秒），默认 30000
  - `maxRetries` / `retry.max` - 最大重试次数，默认 0
  - `backoffMs` / `retry.backoffMs` - 重试退避基数（毫秒），默认 500
  - `backoffFactor` / `retry.backoffFactor` - 退避因子，默认 2.0
  - `metadata` - 元数据（任意 JSON 对象）

**返回：**
- `number/string` - 任务 ID

### `cancel(taskId)` / `cancel(taskId)`

取消任务。

**参数：**
- `taskId` - 任务 ID

**返回：**
- `boolean` - 是否成功

### `status(taskId)` / `status(taskId)`

获取任务状态。

**参数：**
- `taskId` - 任务 ID

**返回：**
- `string` - 状态字符串：`"pending"`, `"running"`, `"succeeded"`, `"failed"`, `"canceled"`, `"timeout"`

### `wait(taskId, timeoutMs?)` / `wait(taskId, timeoutMs?)`

等待任务完成。

**参数：**
- `taskId` - 任务 ID
- `timeoutMs` - 可选，等待超时（毫秒），默认 30000

**返回：**
- `boolean` - 是否在超时前完成

### `result(taskId)` / `result(taskId)`

获取任务结果。

**参数：**
- `taskId` - 任务 ID

**返回：**
- `any` - 任务返回值，如果任务失败或未完成返回 `null`/`nil`

### `error(taskId)` / `error(taskId)`

获取任务错误信息。

**参数：**
- `taskId` - 任务 ID

**返回：**
- `string` - 错误信息字符串，无错误返回空字符串

### `retry(taskId, options?)` / `retry(taskId, options?)`

重试失败的任务。

**参数：**
- `taskId` - 任务 ID
- `options` - 可选重试配置
  - `maxRetries` / `max` - 最大重试次数
  - `backoffMs` - 重试退避基数

**返回：**
- `boolean` - 是否成功发起重试
