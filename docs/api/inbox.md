> [中文](../zh/api/inbox.md) | **English**

# Inbox

This document is lengthy (621 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [inbox.md](../zh/api/inbox.md) for the original.

- 模块概述
  - 架构设计
  - 消息生命周期
- 连接服务器
  - connect(url, config?)
- 消费消息
  - consume(handle?, timeout?)
- 确认消息
  - ack(handle, msgId)
- 报告完成
  - report(handle, msgId, result?)
- 心跳
  - heartbeat(handle?)
- 连接管理
  - isConnected(handle?)
  - disconnect(handle?)
- 完整示例
  - 消息消费者
- 可用接口
- 协议消息格式
  - 客户端 [服务器]
  - 服务器 [客户端]
- 注意事项
