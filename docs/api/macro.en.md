> [中文](macro.md) | **English**

# Macro

This document is lengthy (731 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [macro.md](macro.md) for the original.

- 模块概述
- 设计约束
- 录制控制
  - start()
  - stop()
  - pause()
  - resume()
  - clear()
- 状态查询
  - isRecording()
  - isPaused()
  - getEventCount()
  - status()
- 持久化
  - saveToLua(path)
  - saveToJSON(path)
  - loadFromJSON(path)
- 回放
  - playback(speed?, repeat?)
- 可用接口
- 完整示例
  - 录制 + 回放（热键驱动）
  - 录制 + 持久化 + 加载回放
- 注意事项
