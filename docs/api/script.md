> [中文](../zh/api/script.md) | **English**

# Script

This document is lengthy (562 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [script.md](../zh/api/script.md) for the original.

- 设计约束
- 函数列表
- 查询类函数
  - script.list()
  - script.getState(name)
  - script.getStats(name)
  - script.isRunning(name)
  - script.has(name)
- 生命周期控制
  - script.run(name)
  - script.stop(name)
  - script.reload(name)
  - script.load(name, path, config?)
- 环境变量
  - script.setEnv(name, key, value)
  - script.getEnv(name, key)
- 配置管理
  - script.setConfig(key, value)
  - script.getConfig(key)
- 热重载
  - script.setHotReload(enabled)
- 完整示例
  - Python
  - Lua
- 注意事项
