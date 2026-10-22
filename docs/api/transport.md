> [中文](transport.md) | **English**

# Transport

This document is lengthy (884 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [transport.md](transport.md) for the original.

- 模块概述
  - 消息格式
- TCP 客户端
  - tcpConnect(id?, host?, port?)
  - tcpSend(handle, data)
  - tcpIsConnected(handle)
  - tcpDisconnect(handle)
- TCP 服务器
  - tcpListen(id?, host?, port?)
  - tcpGetSessions(handle)
  - tcpSendTo(handle, session_id, data)
  - tcpBroadcast(handle, data)
  - tcpCloseSession(handle, session_id)
  - tcpStop(handle)
- UDP Socket
  - udpSocket(id?)
  - udpBind(handle, host, port)
  - udpSendTo(handle, host, port, data)
  - udpRecvFrom(handle, timeout?)
  - udpClose(handle)
- 完整示例
  - TCP Echo 服务器
  - TCP 客户端
- 可用接口
  - 客户端函数
  - 服务器函数
  - UDP 函数
- 注意事项
