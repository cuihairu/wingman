> [中文](../zh/api/orchestration.md) | **English**

# API: wingman.orchestration

Workflow module providing workflow submission, dependency scheduling, cancellation, and query functionality.

## Module Overview

The orchestration module executes workflows **locally** inside the runtime:

- **Submit workflows** - Validates the dependency graph (cycle detection) and returns a workflow ID immediately; a single scheduler thread makes scheduling decisions and dispatches ready tasks to dedicated worker threads
- **Dependency scheduling** - A task is scheduled only after all of its prerequisites succeed; prerequisite failures/cancellations/timeouts propagate as skips along the dependency graph
- **Conditional branching** - Tasks may carry a `when` condition, evaluated once prerequisites are satisfied; when it does not hold, the task is skipped along the condition chain (this does not fail the workflow)
- **Concurrency control** - `maxParallel` caps the number of tasks executing simultaneously within a workflow (default 1, serial)
- **Cancel workflows** - Cancel a running workflow (cooperative cancellation; tasks that have not started are canceled outright)
- **Query workflows** - Get workflow- and task-level status snapshots (including the dependency-blocked state)

::: warning Note
Both task `run` bodies and `when` conditions are invoked on the scheduler/worker threads and must be thread-safe callables —
Python functions qualify (GIL); **Lua functions are rejected at submission** (same gating semantics as the `async` flag in `task.submit`).
:::

---

## Submitting a Workflow

### submit_workflow(workflow)

**Description**: Submits a new workflow execution. Returns a workflow ID immediately; the scheduler thread dispatches ready tasks to worker threads in dependency order, and concurrency within the workflow is capped by `maxParallel` (default 1 = serial). The function name is identical on both ends (snake_case).

**Signature**:

```python
submit_workflow(workflow: dict) -> str | None
```

```lua
submit_workflow(workflow: table) -> string | nil
```

**Parameters**:

- `workflow` - Workflow definition object:
  - `name` (string, optional) - Workflow name
  - `maxParallel` (int, optional) - Workflow-level concurrency cap: the number of tasks executing simultaneously; default 1 (serial); `<1` is rejected
  - `tasks` (array, required, non-empty) - Array of task definitions; each task:
    - `id` (string, required) - Unique task identifier (duplicates are rejected)
    - `run` (callable, required) - Task body (must be a thread-safe callable)
    - `dependsOn` (array of string, optional) - Set of prerequisite task ids; this task is scheduled only when all prerequisites are `succeeded`
    - `when` (callable, optional) - Branch condition, evaluated exactly once at the scheduling point after prerequisites are satisfied (see the next section)
    - `timeoutMs` (int, optional) - Timeout in milliseconds, default 30000
    - `maxRetries` (int, optional) - Number of retries on failure, default 0
    - `backoffMs` (int, optional) - Base retry backoff in milliseconds, default 500
    - `backoffFactor` (float, optional) - Backoff multiplier, default 2.0

**Returns**:

- Python: the workflow ID, or `None` on failure
- Lua: the workflow ID, or `nil` on failure

**Rejection rules** (returns `None` and emits an `orchestration.error` event with payload `{"error": reason}`):

- `workflow` is not an object, `tasks` is missing or an empty array, or a task item is missing `id`/`run` or has a mismatched type
- `maxParallel` is not an integer or is `< 1`
- Duplicate task ids
- Unknown prerequisite references, self-dependencies, dependency cycles (DFS cycle detection; rejected at submission)
- `run`/`when` is not a thread-safe callable (Lua function)

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

## Conditional Branching (`when`)

Task items may carry an optional `when` predicate for branch filtering. Its semantics align with dependency propagation and retries:

- **Evaluation timing** - Evaluated **once** at the scheduling point, after all prerequisites are `succeeded` and before the task starts (not re-evaluated on retries); the predicate is evaluated outside the lock and may re-enter `get_workflow`/`cancel_workflow`
- **Truthiness rules** (uniform across languages):

| Return value | Interpretation |
|--------|------|
| `bool` | By value |
| `int` / `float` | Truthy if non-zero |
| `str` | Truthy if non-empty |
| `list` / `dict` | Truthy if non-empty |
| `None` | Falsy |

- **Condition not met → condition-chain `skipped`**: branch filtering is normal control flow and **does not fail the workflow**; downstream tasks are skipped transitively along the dependency graph, and with mixed prerequisites the failure chain takes precedence over the condition chain
- **Predicate raised → recorded as a failure**: the task is set to `failed` with the error message `when predicate threw: ...` (same failure semantics as the task module), and downstream tasks propagate along the failure chain

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

::: warning Note
`when` is gated exactly like `run`: it must be a thread-safe callable, and Lua functions are rejected at submission. A branch
whose condition does not hold does **not** consume a `maxParallel` execution slot (it is recorded at evaluation time and
never enters a worker).
:::

---

## Canceling a Workflow

### cancel_workflow(workflowId)

**Description**: Cancels a running workflow. Running tasks are canceled cooperatively (`run` bodies cannot be forcibly interrupted; the snapshot reflects `canceled` immediately, and the worker wraps up once the body returns naturally); tasks that have not started (including dependency-blocked ones) are set to `canceled` outright. Returns `false` for an unknown ID or an already-finalized workflow.

**Signature**:

```python
cancel_workflow(workflowId: str) -> bool
```

```lua
cancel_workflow(workflowId: string) -> boolean
```

**Returns**:

- Whether a cancellation transition occurred

---

## Querying Workflows

### get_workflow(workflowId)

**Description**: Gets a workflow status snapshot. Snapshot reads do not block scheduling.

**Signature**:

```python
get_workflow(workflowId: str) -> dict | None
```

```lua
get_workflow(workflowId: string) -> table | nil
```

**Returns**: The workflow snapshot object, or `None`/`nil` if it does not exist.

### get_all_workflows()

**Description**: Gets a list of snapshots of all workflows.

**Signature**:

```python
get_all_workflows() -> list[dict]
```

```lua
get_all_workflows() -> table
```

---

## History Ledger

Both workflow **submissions** and **finalizations** are appended to a JSONL ledger in the app data directory (`<appDataDir>/workflows.jsonl`, rotated to `.old` when a single file exceeds 8 MiB); history remains queryable after a process restart. `get_workflow`/`get_all_workflows` reflect in-memory state only. The path can be redirected via the `WINGMAN_WORKFLOW_LEDGER` environment variable (a full path); when the app data directory is unavailable, the ledger is disabled entirely (`history` always returns an empty array).

The ledger records only task ids, the dependency graph, and statuses — **it does not persist `run()` result payloads**; task bodies are script closures and cannot be serialized, so the ledger is **append-only with no automatic recovery** — after a restart, unfinished workflows are not rerun; instead, a synthetic `interrupted` finalization is appended for each leftover entry (see the status vocabulary below), for post-hoc queries and subscriber awareness.

### history(limit = 20)

**Description**: Queries the finalized ledger view, newest first. Entries still in progress (submitted but not finalized) report `status` as `running` without `finishedAt` and sort first; the rest are ordered by `finishedAt` descending. `limit` defaults to 20 and is clamped to 1..500.

**Signature**:

```python
history(limit: int = 20) -> list[dict]
```

```lua
history(limit?: int) -> table
```

**Returns**: An array of views, each `{id, name, run, status, startedAt, finishedAt?, interrupted?, tasks?}`:

- `run`: process identity (pid + first-start milliseconds); distinguishes `workflowId` numbering collisions across processes (each process starts at `wf-1`);
- `tasks`: task id → status (same shape as snapshots); entries given a synthetic `interrupted` finalization do not carry this field;
- `interrupted`: carried only by finalizations synthesized during startup recovery; indicates a leftover from a killed process (not the same as a runtime failure).

Example:

```python
for rec in wingman.orchestration.history(50):
    print(rec["id"], rec["status"], rec.get("interrupted", False))
```

```lua
for _, rec in ipairs(wingman.orchestration.history(50)) do
    print(rec.id, rec.status, rec.interrupted == true)
end
```

Startup recovery (once per process, performed on the first `submit_workflow`/`history` access) emits an `orchestration.interrupted` event (payload `{workflow}`) for each leftover entry.

---

## Status Vocabulary

**Workflow statuses** (`status`):

| Status | Meaning |
|------|------|
| `running` | Tasks exist that are not yet finalized |
| `succeeded` | All tasks `succeeded` (or `skipped` via the condition chain) |
| `failed` | Failure-chain tasks exist (`failed`/`timeout` plus the `skipped` they propagated) and no unfinalized tasks remain |
| `canceled` | Finalized after a `cancel_workflow` request (running tasks canceled cooperatively, workers wrap up in the background) |
| `interrupted` | History ledger only: an in-flight workflow left behind by a killed process, finalized by startup recovery (not the same as a runtime failure) |

**Task statuses** (`tasks[].status`):

| Status | Meaning |
|------|------|
| `blocked` | Prerequisites have not all succeeded (dependency-blocked state) |
| `pending` | All prerequisites succeeded, waiting to be scheduled |
| `running` | Executing on a worker; cooperative cancellation is reflected as `canceled` immediately (without waiting for the body to return) |
| `skipped` | Skipped transitively via the failure chain (propagated from a `failed`/`canceled`/`timeout` prerequisite) or the condition chain (branch filtering when `when` does not hold); snapshots do not distinguish the root cause |
| `canceled` | Tasks that had not started when the workflow was canceled |
| `succeeded` / `failed` / `timeout` | Terminal states, same semantics as `wingman.task` (failures within the retry budget do not propagate; after recovery, downstream tasks are scheduled as usual) |

Snapshots of failed tasks additionally carry an `error` field (the exception message, same semantics as `task.error`).

---

## Workflow-Level Events

The workflow lifecycle is broadcast over the `EventHub` as three `orchestration.*` events (`source` is always `orchestration`).
All of them are emitted from the single scheduler thread, so their **total order** matches the scheduling order: `started` → `task_finished`×N → `finished`.

| Event | Payload | When |
|------|------|------|
| `orchestration.started` | `{workflow, name}` | Scheduler thread startup (emitted as soon as submission succeeds) |
| `orchestration.task_finished` | `{workflow, task, status, error?}` | A single task is recorded in a terminal state (`status` is the task's terminal status word; failures carry `error`) |
| `orchestration.finished` | `{workflow, name, status, tasks}` | The workflow is finalized (`status` ∈ succeeded/failed/canceled) |

The `tasks` count fields of `orchestration.finished`:

| Field | Counting rule |
|------|------|
| `succeeded` / `failed` / `skipped` / `canceled` | Counted across the four recorded states (`failed` includes `timeout`) |
| `other` | Tasks still executing and unaccounted for within the cancellation window (appears only then; the snapshot is authoritative for the underlying terminal state) |
| `total` | Total number of tasks (sum of the other fields) |

::: warning Task event semantics
- Tasks `skipped` via failure-chain/condition-chain propagation do **not** emit an individual `task_finished` (they never actually executed) and are visible only in the `finished` counts; tasks still executing within the cancellation window likewise get no individual event.
- Module-level `task.*` events are disabled for tasks inside an orchestration (`emitEvents` gating); observability goes uniformly through the `orchestration.*` events in this section, avoiding duplicate dual-channel reporting.
:::

```python:line-numbers
from wingman import event_hub, orchestration

event_hub.subscribe("orchestration.finished",
                    lambda msg: print(msg.payload["status"], msg.payload["tasks"]),
                    "report-watcher")
workflow_id = orchestration.submit_workflow({"name": "daily_report", "tasks": [...]})
```

---

## Workflow Snapshot Object

| Field | Type | Description |
|------|------|------|
| `id` | string | Unique workflow ID (`wf-<N>`) |
| `name` | string | Workflow name (empty string if not provided) |
| `status` | string | Workflow status (see the table above) |
| `tasks` | array | Task snapshots in definition order: `{id, status, dependsOn, error?, result?}` |

**Subtask output aggregation**: the `result` field of a succeeded task carries the return value of `run()` (as-is, any ScriptValue such as
Int/String/Object); if it returns `null` the field is omitted (indistinguishable from producing no output);
failed/timed-out/canceled/skipped tasks do not carry it. Downstream tasks can read upstream `result` values via
`get_workflow` inside their `run` to compose data flows; outputs of nested workflows can likewise be aggregated
level by level into the parent workflow snapshot (`run` submits the child workflow, waits for its finalization,
and returns its result).

::: warning v1 limitations
- Concurrency within a workflow is capped by `maxParallel` (default 1, serial); separate workflows are scheduled independently, with no cross-workflow cap.
- The `when` condition is for branch filtering only — no loops or nested sub-workflows (subtask aggregation is a later batch item).
- Finalized workflows stay in the in-memory registry until the process exits; there is no automatic cleanup.
- This module covers local script-level execution only; remote orchestration (Go server `WorkflowDetailData` DTO alignment, subtask aggregation) is a later batch item.
:::
