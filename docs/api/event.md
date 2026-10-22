> [中文](event.md) | **English**

# Events

This document is lengthy (438 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [event.md](event.md) for the original.

- 模块概述
- 订阅事件
  - on(type, callback, name?) / on(type, callback, name?)
- 一次性订阅
  - once(type, callback) / once(type, callback)
- 发布事件
  - emit(type, payload?, meta?) / emit(type, payload?, meta?)
- 取消订阅
  - off(subscription) / off(subscription)
  - clear(type?) / clear(type?)
- 查询监听器
  - listener(subscription) / listener(subscription)
  - listeners(type) / listeners(type)
- 构造事件对象
  - message(type, payload?, meta?) / message(type, payload?, meta?)
- 事件对象结构
- 内置事件源
- 可用接口
