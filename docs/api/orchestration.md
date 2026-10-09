# API: wingman.orchestration

工作流模块，提供工作流提交、依赖调度、取消与查询功能。

## 模块概述

orchestration 模块在 runtime 内**本地执行**工作流：

- **提交工作流** - 校验依赖图（环检测）后立即返回工作流 ID；单调度线程做调度决策，就绪任务派发到独立 worker 线程执行
- **依赖调度** - 前置任务全部成功才调度后置任务；前置失败/取消/超时沿依赖图传播跳过
- **条件分支** - 任务可带 `when` 条件，前置满足后求值，不成立按条件链跳过（不判工作流失败）
- **并发控制** - `maxParallel` 约束工作流内同时执行的任务数（默认 1 串行）
- **取消工作流** - 取消执行中的工作流（协作式取消，未开工任务直接取消）
- **查询工作流** - 获取工作流与任务级状态快照（含依赖阻塞态）

::: warning 注意
任务 `run` 与条件 `when` 均在调度线程/worker 线程上调用，必须是线程安全可调用体——
Python 函数满足（GIL），**Lua 函数提交即拒绝**（同 `task.submit` 的 `async` 门控语义）。
:::

---

## 提交工作流

### submit_workflow(workflow)

**说明**：提交一个新的工作流执行。立即返回工作流 ID；调度线程按依赖序派发就绪任务到 worker 线程，工作流内并发度受 `maxParallel` 约束（默认 1 = 串行）。两端函数名一致（均为 snake_case）。

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
  - `maxParallel` (int，可选) - 工作流级并发上限：同时执行中的任务数，默认 1（串行）；`<1` 拒绝
  - `tasks` (array，必填，非空) - 任务定义数组，每个任务：
    - `id` (string，必填) - 任务唯一标识（重复即拒绝）
    - `run` (callable，必填) - 任务执行体（须线程安全可调用体）
    - `dependsOn` (array of string，可选) - 前置任务 id 集合；前置全部 `succeeded` 才调度本任务
    - `when` (callable，可选) - 分支条件，前置满足后的调度点求值一次（详见下节）
    - `timeoutMs` (int，可选) - 超时毫秒数，默认 30000
    - `maxRetries` (int，可选) - 失败重试次数，默认 0
    - `backoffMs` (int，可选) - 重试退避基准毫秒数，默认 500
    - `backoffFactor` (float，可选) - 退避因子，默认 2.0

**返回**：

- Python: 工作流 ID，失败返回 `None`
- Lua: 工作流 ID，失败返回 `nil`

**拒绝规则**（返回 `None` 并发出 `orchestration.error` 事件，负载 `{"error": 原因}`）：

- `workflow` 非对象、`tasks` 缺失/空数组、任务项缺 `id`/`run` 或类型不符
- `maxParallel` 非整数或 `< 1`
- 重复任务 id
- 未知前置引用、自依赖、依赖环（DFS 环检测，提交即拒绝）
- `run`/`when` 非线程安全可调用体（Lua 函数）

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
-- Lua 可调用体非线程安全，run/when 直接提交会被拒绝（orchestration.error 事件）。
-- Lua 侧如需编排，可由 run 调 wingman.task.submit（async=false 同步任务）封装。
```

:::

---

## 条件分支（when）

任务项可选 `when` 谓词实现分支过滤。语义与依赖传播/重试对齐：

- **求值时机** - 前置任务全部 `succeeded` 后、开工前的调度点求值**一次**（不随重试重复）；谓词在锁外求值，可重入 `get_workflow`/`cancel_workflow`
- **真值口径**（跨语言统一）：

| 返回值 | 判定 |
|--------|------|
| `bool` | 按值 |
| `int` / `float` | 非 0 为真 |
| `str` | 非空为真 |
| `list` / `dict` | 非空为真 |
| `None` | 假 |

- **不成立 → 条件链 `skipped`**：分支过滤是正常控制流，**不判工作流失败**；后置任务沿依赖图连锁跳过，混合前置时失败链优先于条件链
- **求值异常 → 按失败落账**：任务置 `failed` 并携带错误信息 `when predicate threw: ...`（沿用 task 模块失败语义），后置按失败链传播

```python:line-numbers
import wingman.orchestration as orch

def publish():
    ...  # 正式发布

def dry_run():
    ...  # 演练

orch.submit_workflow({
    "tasks": [
        {"id": "build", "run": build},
        # 两个分支互斥：由 flag 决定走哪边
        {"id": "publish", "run": publish, "dependsOn": ["build"],
         "when": lambda: release_flag},
        {"id": "dry_run", "run": dry_run, "dependsOn": ["build"],
         "when": lambda: not release_flag},
    ]
})
# 只有一条分支执行，另一条 skipped；工作流终态 succeeded
```

::: warning 注意
`when` 与 `run` 同门控：必须线程安全可调用体，Lua 函数提交即拒绝。条件不成立
的分支**不占** `maxParallel` 执行额度（求值即落账，不进 worker）。
:::

---

## 取消工作流

### cancel_workflow(workflowId)

**说明**：取消执行中的工作流。执行中任务协作式取消（`run` 内部无法被强制打断；快照即时反映 `canceled`，工作体自然返回后 worker 收尾）；未开工任务（含依赖阻塞态）直接置 `canceled`。未知 ID 或已终局的工作流返回 `false`。

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
| `succeeded` | 全部任务 `succeeded`（或按条件链 `skipped`） |
| `failed` | 存在失败链任务（`failed`/`timeout` 及其传播的 `skipped`）且无未终局任务 |
| `canceled` | `cancel_workflow` 请求后定稿（执行中任务协作取消，worker 后台收尾） |

**任务状态**（`tasks[].status`）：

| 状态 | 含义 |
|------|------|
| `blocked` | 前置任务未全部成功（依赖阻塞态） |
| `pending` | 前置已全部成功，等待调度 |
| `running` | worker 执行中；协作取消即时反映 `canceled`（不等工作体返回） |
| `skipped` | 失败链（前置 `failed`/`canceled`/`timeout` 传播）或条件链（`when` 不成立的分支过滤）连锁跳过，传递；快照不区分根因 |
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
- 工作流内并发由 `maxParallel` 约束（默认 1 串行）；跨工作流各自独立调度、互不设限。
- 条件 `when` 只作分支过滤，不做循环/子工作流嵌套（子任务聚合为后续批次项）。
- 终局工作流驻留内存注册表直至进程退出，不自动清理。
- 本模块只负责脚本层本地执行；远程编排（Go server `WorkflowDetailData` DTO 对齐、流程级状态事件、子任务聚合）为后续批次项。
:::
