# Notify API

`wingman.notify` 提供统一的通知与告警出口，支持日志、toast、webhook 和事件桥接。

## Python

```python
from wingman import notify

# 日志通知
notify.debug("调试信息", {"module": "combat"})
notify.info("脚本启动", {"script": "farm.py"})
notify.warn("资源不足", {"hp": 10})
notify.error("任务失败", {"error": "timeout"})

# Toast 通知
notify.toast("Wingman", "任务完成", level="success")

# Webhook
notify.webhook("http://127.0.0.1:9000/hook", {
    "event": "task.done",
    "result": 42
})

# 事件桥接
notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

## Lua

```lua
local notify = require("wingman.notify")

notify.debug("调试信息", { module = "combat" })
notify.info("脚本启动", { script = "farm.py" })
notify.warn("资源不足", { hp = 10 })
notify.error("任务失败", { error = "timeout" })

notify.toast("Wingman", "任务完成", "success")

notify.webhook("http://127.0.0.1:9000/hook", {
    event = "task.done",
    result = 42
})

notify.bridge("combat.*", "event://logging.combat_events")
notify.bridge("task.failed", "http://127.0.0.1:9000/alert")
```

## 可用接口

### `debug(message, meta?)`

输出调试级别日志。

- `message`: 日志消息
- `meta`: 可选，元数据对象

### `info(message, meta?)`

输出信息级别日志。

- `message`: 日志消息
- `meta`: 可选，元数据对象

### `warn(message, meta?)`

输出警告级别日志。

- `message`: 日志消息
- `meta`: 可选，元数据对象

### `error(message, meta?)`

输出错误级别日志。

- `message`: 日志消息
- `meta`: 可选，元数据对象

### `toast(title, message, level?)`

显示桌面 toast 通知。

- `title`: 标题
- `message`: 消息内容
- `level`: 可选，级别：`"info"`, `"success"`, `"warning"`, `"error"`，默认 `"info"`

### `webhook(url, payload, options?)`

发送 HTTP POST webhook。

- `url`: 目标 URL
- `payload`: 请求体（JSON 对象）
- `options`: 可选配置（预留）

### `bridge(eventPattern, target, options?)`

桥接事件到其他目标。

- `eventPattern`: 事件模式（支持通配符）
- `target`: 目标位置
  - `event://<event_name>`: 转发到另一个事件
  - `http://<url>`: 转发到 webhook
- `options`: 可选配置
  - `transform`: 转换函数，用于修改载荷

## 事件

通知系统会发出以下事件：

```python
event.on("notify.log", lambda e: print(f"[{e['payload']['level']}] {e['payload']['message']}"))
event.on("notify.toast", lambda e: print(f"Toast: {e['payload']['title']} - {e['payload']['message']}"))
event.on("notify.failed", lambda e: print(f"通知失败: {e['payload']['type']}"))
```

### 事件类型

- `notify.log`: 任何日志（包含 `level`, `message`, `timestamp`, `meta`）
- `notify.log.DEBUG`: 调试日志
- `notify.log.INFO`: 信息日志
- `notify.log.WARN`: 警告日志
- `notify.log.ERROR`: 错误日志
- `notify.toast`: Toast 通知（包含 `title`, `message`, `level`）
- `notify.webhook.sent`: Webhook 已发送
- `notify.failed`: 通知发送失败（包含 `type`, `url/error`, `error`）
