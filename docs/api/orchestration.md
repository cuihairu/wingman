# API: wingman.orchestration

工作流模块，提供工作流提交、依赖调度、取消与查询功能。

## 模块概述

orchestration 模块在 runtime 内**本地执行**工作流：

- **提交工作流** - 校验依赖图（环检测）后立即返回工作流 ID，任务在独立调度线程上执行
- **依赖调度** - 前置任务全部成功才调度后置任务；前置失败/取消/超时沿依赖图传播跳过
- **取消工作流** - 取消执行中的工作流（协作式取消，未开工任务直接取消）
- **查询工作流** - 获取工作流与任务级状态快照（含依赖阻塞态）

::: warning 注意
任务通过 `run` 可调用体在**调度线程**上执行，必须是线程安全可调用体——
Python 函数满足（GIL），**Lua 函数提交即拒绝**（同 `task.submit` 的 `async` 门控语义）。
:::

---

## 提交工作流

### submit_workflow(workflow)

**说明**：提交一个新的工作流执行。立即返回工作流 ID，任务由调度线程按依赖序串行执行（v1 不并发，并发控制为后续批次项）。两端函数名一致（均为 snake_case）。

**函数签名**：

```python
submit_workflow(workflow: dict) -> str | None
```

```lua
submit_workflow(workflow: table) -> string | nil
```

**参数**：

- `workflow` - 工作流定义对象：
  - `name` (string，可选) - 工作流名称
  - `tasks` (array，必填，非空) - 任务定义数组，每个任务：
    - `id` (string，必填) - 任务唯一标识（重复即拒绝）
    - `run` (callable，必填) - 任务执行体（须线程安全可调用体）
    - `dependsOn` (array of string，可选) - 前置任务 id 集合；前置全部 `succeeded` 才调度本任务
    - `timeoutMs` (int，可选) - 超时毫秒数，默认 30000
    - `maxRetries` (int，可选) - 失败重试次数，默认 0
    - `backoffMs` (int，可选) - 重试退避基准毫秒数，默认 500
    - `backoffFactor` (float，可选) - 退避因子，默认 2.0

**返回**：

- Python: 工作流 ID，失败返回 `None`
- Lua: 工作流 ID，失败返回 `nil`

**拒绝规则**（返回 `None` 并发出 `orchestration.error` 事件，负载 `{"error": 原因}`）：

- `workflow` 非对象、`tasks` 缺失/空数组、任务项缺 `id`/`run` 或类型不符
- 重复任务 id
- 未知前置引用、自依赖、依赖环（DFS 环检测，提交即拒绝）
- `run` 非线程安全可调用体（Lua 函数）

:::tabs

== Python

```python:line-numbers
from wingman import orchestration

workflow_id = orchestration.submit_workflow({
    "name": "daily_report",
    "tasks": [
        # 无 dependsOn 的任务即入口任务
        {"id": "collect", "run": collect_data},
        {"id": "analyze", "run": analyze, "dependsOn": ["collect"]},
        # 多前置：b、c 都成功后才执行 d
        {"id": "render",  "run": render, "dependsOn": ["analyze"]},
        {"id": "upload",  "run": upload, "dependsOn": ["analyze", "render"]},
    ]
})
if workflow_id is None:
    print("提交被拒绝，原因见 orchestration.error 事件")
```

== Lua

```lua:line-numbers
-- Lua 可调用体非线程安全，run 直接提交会被拒绝（orchestration.error 事件）。
-- Lua 侧如需编排，可由 run 调 wingman.task.submit（async=false 同步任务）封装。
```

:::

---

## 取消工作流

### cancel_workflow(workflowId)

**说明**：取消执行中的工作流。执行中任务协作式取消（`run` 内部无法被强制打断，任务在可调用体返回后落 `canceled`）；未开工任务（含依赖阻塞态）直接置 `canceled`。未知 ID 或已终局的工作流返回 `false`。

**函数签名**：

```python
cancel_workflow(workflowId: str) -> bool
```

```lua
cancel_workflow(workflowId: string) -> boolean
```

**返回**：

- 是否发生取消转换

---

## 查询工作流

### get_workflow(workflowId)

**说明**：获取工作流状态快照。快照读取不阻塞调度。

**函数签名**：

```python
get_workflow(workflowId: str) -> dict | None
```

```lua
get_workflow(workflowId: string) -> table | nil
```

**返回**：工作流快照对象，不存在返回 `None`/`nil`。

### get_all_workflows()

**说明**：获取所有工作流快照列表。

**函数签名**：

```python
get_all_workflows() -> list[dict]
```

```lua
get_all_workflows() -> table
```

---

## 状态词汇表

**工作流状态**（`status`）：

| 状态 | 含义 |
|------|------|
| `running` | 存在未终局任务 |
| `succeeded` | 全部任务 `succeeded` |
| `failed` | 存在 `failed`/`timeout`/`skipped` 任务且无未终局任务 |
| `canceled` | `cancel_workflow` 请求后定稿 |

**任务状态**（`tasks[].status`）：

| 状态 | 含义 |
|------|------|
| `blocked` | 前置任务未全部成功（依赖阻塞态） |
| `pending` | 前置已全部成功，等待调度 |
| `running` | 调度线程执行中 |
| `skipped` | 前置 `failed`/`canceled`/`timeout` 连锁传播跳过（传递：被跳过任务的后置同样跳过） |
| `canceled` | 工作流取消时未开工的任务 |
| `succeeded` / `failed` / `timeout` | 终态，语义同 `wingman.task`（重试额度内的失败不传播，恢复后照常调度后置） |

失败任务快照额外携带 `error` 字段（异常消息，同 `task.error` 语义）。

---

## 工作流快照对象

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 工作流唯一 ID（`wf-<N>`） |
| `name` | string | 工作流名称（未提供为空串） |
| `status` | string | 工作流状态（见上表） |
| `tasks` | array | 任务快照，按定义顺序：`{id, status, dependsOn, error?}` |

::: warning v1 限制
- 任务在单工作流内按拓扑序**串行**执行（并发控制为后续批次项）；跨工作流各自独立调度。
- 终局工作流驻留内存注册表直至进程退出，不自动清理。
- 本模块只负责脚本层本地执行；远程编排（Go server `WorkflowDetailData` DTO 对齐、流程级状态事件、条件分支、子任务聚合）为后续批次项。
:::
