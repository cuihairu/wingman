# Notify API

`wingman.notify` 规划为统一通知与告警出口。

## 目标接口

- `debug(message, meta?)`
- `info(message, meta?)`
- `warn(message, meta?)`
- `error(message, meta?)`
- `toast(title, message, level?)`
- `webhook(url, payload, options?)`
- `bridge(eventPattern, target, options?)`
