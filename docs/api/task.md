# Task API

`wingman.task` 规划为任务提交与生命周期管理模块。

## 目标接口

- `submit(work, options?)`
- `cancel(taskId)`
- `status(taskId)`
- `wait(taskId, timeoutMs?)`
- `result(taskId)`
- `error(taskId)`
- `retry(taskId, options?)`

任务生命周期应发出 `task.*` 事件。
