> [中文](../zh/api/task.md) | **English**

# Task

This document is lengthy (497 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [task.md](../zh/api/task.md) for the original.

- 模块概述
- 提交任务
  - submit(work, options?) / submit(work, options?)
- 获取任务状态
  - status(task_id) / status(taskId)
- 等待任务完成
  - wait(task_id, timeout_ms?) / wait(taskId, timeoutMs?)
- 获取任务结果
  - result(task_id) / result(taskId)
- 获取错误信息
  - error(task_id) / error(taskId)
- 取消任务
  - cancel(task_id) / cancel(taskId)
- 暂停 / 恢复任务
  - pause(task_id) / pause(taskId)
  - resume(task_id) / resume(taskId)
- 重试任务
  - retry(task_id, options?) / retry(taskId, options?)
- 可用接口
- 生命周期事件
