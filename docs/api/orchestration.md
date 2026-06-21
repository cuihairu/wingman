# API: wingman.orchestration

工作流模块，提供工作流提交和管理功能。

::: warning 注意
此模块需要服务器组件支持，当前为存根实现。
:::

## 模块概述

orchestration 模块提供工作流管理功能：
- **提交工作流** - 提交新的工作流执行
- **取消工作流** - 取消正在执行的工作流
- **查询工作流** - 获取工作流详细信息
- **列出工作流** - 获取所有工作流列表

---

## 提交工作流

### submit_workflow(workflow) / submitWorkflow(workflow)

**说明**：提交一个新的工作流执行。

**函数签名**：

```python
submit_workflow(workflow: dict) -> str | None
```

```lua
submitWorkflow(workflow: table) -> string | nil
```

**参数**：
- `workflow` - 工作流定义对象
  - `name` / `name` - 工作流名称
  - `steps` / `steps` - 步骤数组
  - `timeout` / `timeout` - 可选，超时时间
  - `metadata` / `metadata` - 可选，元数据

**返回**：
- Python: 工作流 ID，失败返回 `None`
- Lua: 工作流 ID，失败返回 `nil`

:::tabs

== Python

```python:line-numbers
from wingman import orchestration

# 提交工作流
workflow_id = orchestration.submit_workflow({
    "name": "combat_loop",
    "steps": [
        {"type": "vision", "action": "find_enemy"},
        {"type": "input", "action": "click"},
        {"type": "wait", "duration": 1000}
    ]
})
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 提交工作流
local workflowId = wingman.orchestration.submitWorkflow({
    name = "combat_loop",
    steps = {
        { type = "vision", action = "find_enemy" },
        { type = "input", action = "click" },
        { type = "wait", duration = 1000 }
    }
})
```

:::

---

## 取消工作流

### cancel_workflow(workflow_id) / cancelWorkflow(workflowId)

**说明**：取消正在执行的工作流。

**函数签名**：

```python
cancel_workflow(workflow_id: str) -> bool
```

```lua
cancelWorkflow(workflowId: string) -> boolean
```

**参数**：
- `workflow_id` / `workflowId` - 工作流 ID

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import orchestration

# 取消工作流
orchestration.cancel_workflow(workflow_id)
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 取消工作流
wingman.orchestration.cancelWorkflow(workflowId)
```

:::

---

## 获取工作流信息

### get_workflow(workflow_id) / getWorkflow(workflowId)

**说明**：获取工作流详细信息。

**函数签名**：

```python
get_workflow(workflow_id: str) -> dict | None
```

```lua
getWorkflow(workflowId: string) -> table | nil
```

**参数**：
- `workflow_id` / `workflowId` - 工作流 ID

**返回**：
- Python: 工作流对象，不存在返回 `None`
- Lua: 工作流对象，不存在返回 `nil`

:::tabs

== Python

```python:line-numbers
from wingman import orchestration

# 获取工作流信息
workflow = orchestration.get_workflow(workflow_id)
if workflow:
    print(f"状态: {workflow['status']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取工作流信息
local workflow = wingman.orchestration.getWorkflow(workflowId)
if workflow then
    print("状态: " .. workflow.status)
end
```

:::

---

## 获取所有工作流

### get_all_workflows() / getAllWorkflows()

**说明**：获取所有工作流列表。

**函数签名**：

```python
get_all_workflows() -> list[dict]
```

```lua
getAllWorkflows() -> table
```

**返回**：
- Python: 工作流对象列表
- Lua: 工作流对象数组

:::tabs

== Python

```python:line-numbers
from wingman import orchestration

# 获取所有工作流
workflows = orchestration.get_all_workflows()
for wf in workflows:
    print(f"{wf['id']}: {wf['name']}")
```

== Lua

```lua:line-numbers
local wingman = require("wingman")

-- 获取所有工作流
local workflows = wingman.orchestration.getAllWorkflows()
for i, wf in ipairs(workflows) do
    print(wf.id .. ": " .. wf.name)
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `submit_workflow(workflow)` | `submitWorkflow(workflow)` | 提交工作流 | workflow: 工作流定义对象<br>返回: 工作流ID或None/nil |
| `cancel_workflow(workflowId)` | `cancelWorkflow(workflowId)` | 取消工作流 | workflowId: 工作流ID<br>返回: 是否成功 |
| `get_workflow(workflowId)` | `getWorkflow(workflowId)` | 获取工作流信息 | workflowId: 工作流ID<br>返回: 工作流对象或None/nil |
| `get_all_workflows()` | `getAllWorkflows()` | 获取所有工作流 | 返回: 工作流对象数组 |

---

## 工作流对象

工作流对象包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 工作流唯一 ID |
| `name` | string | 工作流名称 |
| `status` | string | 状态：`pending`, `running`, `completed`, `failed`, `canceled` |
| `steps` | array | 步骤定义 |
| `result` | any | 执行结果 |
| `error` | string | 错误信息 |
| `createdAt` | number | 创建时间戳 |
| `updatedAt` | number | 更新时间戳 |
