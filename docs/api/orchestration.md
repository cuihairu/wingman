# Orchestration API

`wingman.orchestration` 提供工作流提交和管理功能，支持将复杂的自动化任务定义为工作流并执行。

::: warning 注意
此模块需要服务器组件支持，当前为存根实现。
:::

## 提交工作流

:::tabs

== Python

```python
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

```lua
local orchestration = require("wingman.orchestration")

-- 提交工作流
local workflowId = orchestration.submit_workflow({
    name = "combat_loop",
    steps = {
        { type = "vision", action = "find_enemy" },
        { type = "input", action = "click" },
        { type = "wait", duration = 1000 }
    }
})
```

:::

## 取消工作流

:::tabs

== Python

```python
from wingman import orchestration

# 取消工作流
orchestration.cancel_workflow(workflow_id)
```

== Lua

```lua
local orchestration = require("wingman.orchestration")

-- 取消工作流
orchestration.cancel_workflow(workflowId)
```

:::

## 获取工作流信息

:::tabs

== Python

```python
from wingman import orchestration

# 获取工作流信息
workflow = orchestration.get_workflow(workflow_id)
if workflow:
    print(f"状态: {workflow['status']}")
```

== Lua

```lua
local orchestration = require("wingman.orchestration")

-- 获取工作流信息
local workflow = orchestration.get_workflow(workflowId)
if workflow then
    print("状态: " .. workflow.status)
end
```

:::

## 获取所有工作流

:::tabs

== Python

```python
from wingman import orchestration

# 获取所有工作流
workflows = orchestration.get_all_workflows()
for wf in workflows:
    print(f"{wf['id']}: {wf['name']}")
```

== Lua

```lua
local orchestration = require("wingman.orchestration")

-- 获取所有工作流
local workflows = orchestration.get_all_workflows()
for i, wf in ipairs(workflows) do
    print(wf.id .. ": " .. wf.name)
end
```

:::

---

## 可用接口

### `submit_workflow(workflow)` / `submit_workflow(workflow)`

提交一个新的工作流执行。

**参数：**
- `workflow` - 工作流定义对象
  - `name` - 工作流名称
  - `steps` - 步骤数组
  - `timeout` - 可选，超时时间
  - `metadata` - 可选，元数据

**返回：**
- `string?` - 工作流 ID，失败返回 `null`/`nil`

### `cancel_workflow(workflowId)` / `cancel_workflow(workflowId)`

取消正在执行的工作流。

**参数：**
- `workflowId` - 工作流 ID

**返回：**
- `boolean` - 是否成功

### `get_workflow(workflowId)` / `get_workflow(workflowId)`

获取工作流详细信息。

**参数：**
- `workflowId` - 工作流 ID

**返回：**
- `dict?` - 工作流对象，不存在返回 `null`/`nil`

### `get_all_workflows()` / `get_all_workflows()`

获取所有工作流列表。

**返回：**
- `array` - 工作流对象数组

---

## 工作流对象

工作流对象包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 工作流唯一 ID |
| name | string | 工作流名称 |
| status | string | 状态：`pending`, `running`, `completed`, `failed`, `canceled` |
| steps | array | 步骤定义 |
| result | any | 执行结果 |
| error | string | 错误信息 |
| createdAt | number | 创建时间戳 |
| updatedAt | number | 更新时间戳 |
