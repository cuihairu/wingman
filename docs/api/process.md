> [中文](../zh/api/process.md) | **English**

# Process

This document is lengthy (539 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [process.md](../zh/api/process.md) for the original.

- 模块概述
  - 进程 ID（PID）
- 查找进程
  - find(name)
- 启动进程
  - start(path, args?, working_dir?) / start(path, args?, workingDir?)
- 等待进程结束
  - wait(pid, timeout?)
- 终止进程
  - terminate(pid, force)
- 检查进程是否存在
  - exists(pid)
- 等待进程出现
  - wait_for(name, timeout?) / waitFor(name, timeout?)
- 完整示例
  - 进程管理脚本
- 可用接口
  - 进程查找与启动
  - 进程控制
