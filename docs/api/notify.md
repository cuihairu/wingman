# API: wingman.notify

通知模块，提供统一的通知与告警出口，支持日志、toast、webhook 和事件桥接。

## 模块概述

notify 模块提供多种通知方式：
- **日志通知** - debug、info、warn、error 各级别日志
- **Toast 通知** - 桌面弹窗通知
- **Webhook** - HTTP POST 远程通知
- **事件桥接** - 将事件转发到其他目标

---

## 调试日志

### debug(message, meta?) / debug(message, meta?)

**说明**：输出调试级别日志。

**函数签名**：

```python
debug(message: str, meta: dict = None) -> None
```

```lua
debug(message: string, meta: table = nil) -> nil
```

**参数**：
- `message` - 日志消息
- `meta` - 可选，元数据对象

**返回**：
- 无

---

## 信息日志

### info(message, meta?) / info(message, meta?)

**说明**：输出信息级别日志。

**函数签名**：

```python
info(message: str, meta: dict = None) -> None
```

```lua
info(message: string, meta: table = nil) -> nil
```

**参数**：
- `message` - 日志消息
- `meta` - 可选，元数据对象

**返回**：
- 无

---

## 警告日志

### warn(message, meta?) / warn(message, meta?)

**说明**：输出警告级别日志。

**函数签名**：

```python
warn(message: str, meta: dict = None) -> None
```

```lua
warn(message: string, meta: table = nil) -> nil
```

**参数**：
- `message` - 日志消息
- `meta` - 可选，元数据对象

**返回**：
- 无

---

## 错误日志

### error(message, meta?) / error(message, meta?)

**说明**：输出错误级别日志。

**函数签名**：

```python
error(message: str, meta: dict = None) -> None
```

```lua
error(message: string, meta: table = nil) -> nil
```

**参数**：
- `message` - 日志消息
- `meta` - 可选，元数据对象

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import notify

# 各级别日志
notify.debug("调试信息", {"module": "combat"})
notify.info("脚本启动", {"script": "farm.py"})
notify.warn("资源不足", {"hp": 10})
notify.error("任务失败", {"error": "timeout"})
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- 各级别日志
notify.debug("调试信息", { module = "combat" })
notify.info("脚本启动", { script = "farm.lua" })
notify.warn("资源不足", { hp = 10 })
notify.error("任务失败", { error = "timeout" })
```

:::

---

## Toast 通知

### toast(title, message, level?) / toast(title, message, level?)

**说明**：显示桌面 toast 通知。

**函数签名**：

```python
toast(title: str, message: str, level: str = "info") -> None
```

```lua
toast(title: string, message: string, level: string = "info") -> nil
```

**参数**：
- `title` - 标题
- `message` - 消息内容
- `level` - 可选，级别：`"info"`, `"success"`, `"warning"`, `"error"`，默认 `"info"`

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import notify

# Toast 通知
notify.toast("Wingman", "任务完成", level="success")
notify.toast("警告", "血量过低", level="warning")
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- Toast 通知
notify.toast("Wingman", "任务完成", "success")
notify.toast("警告", "血量过低", "warning")
```

:::

---

## Webhook 通知

### webhook(url, payload, options?) / webhook(url, payload, options?)

**说明**：发送 HTTP POST webhook。

**函数签名**：

```python
webhook(url: str, payload: dict, options: dict = None) -> None
```

```lua
webhook(url: string, payload: table, options: table = nil) -> nil
```

**参数**：
- `url` - 目标 URL
- `payload` - 请求体（JSON 对象）
- `options` - 可选配置（预留）

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import notify

# Webhook
notify.webhook("http://127.0.0.1:9000/hook", {
    "event": "task.done",
    "result": 42
})
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- Webhook
notify.webhook("http://127.0.0.1:9000/hook", {
    event = "task.done",
    result = 42
})
```

:::

---

## 事件桥接

### bridge(event_name, target, options?) / bridge(eventName, target, options?)

**说明**：桥接事件到其他目标。

**函数签名**：

```python
bridge(event_name: str, target: str, options: dict = None) -> None
```

```lua
bridge(eventName: string, target: string, options: table = nil) -> nil
```

**参数**：
- `event_name` / `eventName` - 事件名称（精确匹配）
- `target` - 目标地址，支持 `event://` 协议或 HTTP URL
- `options` - 可选配置（预留）

**返回**：
- 无

**注意**：当前实现仅支持精确匹配，不支持通配符模式。如需监听多个事件，请多次调用 `bridge()`。

:::tabs

== Python

```python:line-numbers
from wingman import notify

# 事件桥接
notify.bridge("combat.enemy_found", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

== Lua

```lua:line-numbers
local notify = require("wingman.notify")

-- 事件桥接
notify.bridge("combat.enemy_found", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `debug(message, meta?)` | `debug(message, meta?)` | 调试日志 | message: 日志消息<br>meta: 元数据(可选) |
| `info(message, meta?)` | `info(message, meta?)` | 信息日志 | message: 日志消息<br>meta: 元数据(可选) |
| `warn(message, meta?)` | `warn(message, meta?)` | 警告日志 | message: 日志消息<br>meta: 元数据(可选) |
| `error(message, meta?)` | `error(message, meta?)` | 错误日志 | message: 日志消息<br>meta: 元数据(可选) |
| `toast(title, message, level?)` | `toast(title, message, level?)` | Toast通知 | title: 标题<br>message: 消息内容<br>level: 级别(默认info) |
| `webhook(url, payload, options?)` | `webhook(url, payload, options?)` | Webhook通知 | url: 目标URL<br>payload: 请求体<br>options: 配置(可选) |
| `bridge(eventName, target, options?)` | `bridge(eventName, target, options?)` | 事件桥接 | eventName: 事件名称<br>target: 目标地址<br>options: 配置(可选) |
