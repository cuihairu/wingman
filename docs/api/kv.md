> [中文](../zh/api/kv.md) | **English**

# Key-Value Storage

This document is lengthy (1205 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [kv.md](../zh/api/kv.md) for the original.

- 模块概述
  - 使用场景
- String 操作
  - set(key, value, options?)
  - get(key)
  - delete(keys)
  - exists(key)
  - ttl(key)
  - incr(key, delta?)
  - expire(key, seconds)
- Hash 操作
  - hset(hash, field, value)
  - hget(hash, field)
  - hgetall(hash)
  - hdel(hash, field)
  - hexists(hash, field)
  - hkeys(hash)
- List 操作
  - lpush(list, value)
  - rpush(list, value)
  - lpop(list)
  - rpop(list)
  - llen(list)
  - lrange(list, start, stop)
- 持久化
  - save(file_path) / save(filePath)
  - load(file_path) / load(filePath)
  - enable_auto_save(file_path, interval) / enableAutoSave(filePath, interval)
- 完整示例
  - 任务队列管理
- 可用接口
  - String 操作
  - Hash 操作
  - List 操作
  - 持久化
