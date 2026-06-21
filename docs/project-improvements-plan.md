# Wingman 项目改进实施计划

**目标：** 统一 Python API 到 `wingman.*`，补齐 typing，并把实战常用的事件、状态机、任务编排和通知机制做成统一脚本 API。

## 当前结论

- Python 公开导入必须统一为 `wingman.<module>`，不保留历史顶层包。
- Python typing 需要随模块一起维护，当前基础 typing 已有，新增模块必须同步补 `.pyi`。
- 事件注册、状态机和任务编排是后续模块的基础，应优先于通知、计划任务和高级工作流。
- 现有 `ModuleDescriptor` 的 `ScriptValue` 目前没有跨语言回调类型；要实现 Lua/Python 一致的 `event.on(name, callback)`，需要先补可调用值的封送。

## 功能覆盖判断

### 已覆盖的常见场景

- 屏幕截图、像素/颜色/图像检测
- 鼠标、键盘、人性化输入
- 窗口、进程、系统信息
- OCR、二维码、验证码/TOTP
- HTTP、JSON、KV、配置
- UI Automation 基础查找和控件操作
- 宏录制回放基础能力
- 脚本管理、热加载、调试基础

### 仍偏弱或缺失的实战工具

| 方向 | 需要补齐的能力 | 优先级 |
|------|----------------|--------|
| 事件系统 | 跨模块事件、脚本回调、一次性监听、取消订阅、事件桥接 | P0 |
| 状态机 | 状态定义、转移、guard、enter/exit、事件驱动迁移 | P0 |
| 任务编排 | 提交、取消、状态、等待、重试、超时、退避 | P0 |
| 通知 | 日志、toast、托盘、Webhook、任务/事件桥接 | P1 |
| UI 控件树 | 树遍历、批量查询、条件等待、稳定句柄、事件监听统一 | P1 |
| 图像模板 | 模板注册、批量识别、分组、缓存、阈值策略 | P1 |
| 录制回放 | 录制事件流、可编辑脚本、回放状态、失败恢复 | P1 |
| 热键 | 全局热键、组合键、按下/释放事件、节流 | P2 |
| 定时器 | `setTimeout`、`setInterval`、cron-like 计划任务 | P2 |
| 文件工具 | 读写、复制、移动、watch、路径工具 | P2 |
| 异常恢复 | retry、timeout、circuit breaker、fallback、错误事件 | P2 |

## P0 API 草案

### `wingman.event`

Lua:

```lua
local wingman = require("wingman")

local id = wingman.event.on("combat.enemy_found", function(e)
    print(e.type, e.payload.x, e.payload.y)
end)

wingman.event.once("task.done", function(e)
    print("done", e.payload.taskId)
end)

wingman.event.emit("combat.enemy_found", { x = 100, y = 200 }, {
    source = "vision",
    correlationId = "scan-1",
    priority = 1,
})

wingman.event.off(id)
```

Python:

```python
from wingman import event

def on_enemy_found(e: event.EventMessage) -> None:
    print(e["type"], e["payload"])

sub_id = event.on("combat.enemy_found", on_enemy_found)
event.emit("combat.enemy_found", {"x": 100, "y": 200}, source="vision")
event.off(sub_id)
```

事件对象字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 事件名 |
| `source` | string | 来源模块 |
| `correlationId` | string | 关联 ID |
| `timestamp` | integer | Unix ms |
| `priority` | integer | 优先级 |
| `payload` | object | 业务载荷 |

### `wingman.fsm`

```python
from wingman import fsm

machine = fsm.create("combat", initial="idle")
machine.state("idle", on_enter=lambda ctx: print("idle"))
machine.state("fight")
machine.transition("idle", "fight", on="enemy_found")
machine.transition("fight", "idle", on="enemy_lost")

machine.dispatch("enemy_found", {"target": "boss"})
assert machine.current() == "fight"
```

必须支持：

- `create(name, initial, options?)`
- `state(name, on_enter?, on_exit?)`
- `transition(from, to, on?, guard?, action?)`
- `dispatch(event, payload?)`
- `current()`
- `reset()`
- 自动发出 `fsm.changed`

### `wingman.task`

```python
from wingman import task

task_id = task.submit(
    lambda ctx: {"ok": True},
    retry={"max": 3, "backoffMs": 500},
    timeoutMs=5000,
)

result = task.wait(task_id, timeoutMs=6000)
```

必须支持：

- `submit(callable | spec, options?)`
- `cancel(taskId)`
- `status(taskId)`
- `wait(taskId, timeoutMs?)`
- `result(taskId)`
- `error(taskId)`
- `retry(taskId, options?)`
- 生命周期事件：`task.submitted`、`task.started`、`task.succeeded`、`task.failed`、`task.canceled`、`task.timeout`

## P1 API 草案

### `wingman.notify`

```python
from wingman import notify

notify.info("script started", {"script": "farm.py"})
notify.toast("Wingman", "task finished", level="success")
notify.webhook("http://127.0.0.1:9000/hook", {"event": "task.done"})
```

必须支持：

- `debug/info/warn/error(message, meta?)`
- `toast(title, message, level?)`
- `webhook(url, payload, options?)`
- `bridge(eventPattern, target, options?)`
- 通知失败不能阻塞主任务，失败应发出 `notify.failed`

### `wingman.orchestration`

短期不单独做复杂引擎，先复用 `task` + `fsm`：

- `submit_workflow(spec)`
- `cancel_workflow(workflowId)`
- `get_workflow(workflowId)`
- `get_all_workflows()`
- 工作流节点内部映射为 task，流程状态映射为 fsm

## 实施顺序

1. 扩展 `ScriptValue`，支持跨 Lua/Python 的 callback 封送。
2. 完成 `EventHub` 脚本 API：`on/once/off/emit/clear`。
3. 补 `wingman.event.pyi` 和 `docs/api/event.md`。
4. 基于事件实现 `fsm`。
5. 基于事件实现 `task` 生命周期。
6. 实现 `notify` 的日志/webhook/事件桥接。
7. 把 `orchestration` 从 stub 收敛到 `task + fsm`。

## 完成标准

- Lua 和 Python 的模块名、函数名、事件对象字段一致。
- 每个新增脚本模块都有 `.pyi`、API 文档和至少一组核心单测。
- `event`、`fsm`、`task` 的生命周期都能通过事件观察。
- README 只展示 `wingman.*` 导入方式。
