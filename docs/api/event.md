# Event API

`wingman.event` 提供跨模块事件触发和订阅接口。

## Python

```python
from wingman import event

sub_id = event.emit(
    "task.started",
    {"taskId": "t-1"},
    {"source": "task", "priority": 1},
)
```

## 可用接口

### `emit(type, payload=None, meta=None)`

触发事件。

### `off(subscription)`

取消订阅。参数可以是订阅 ID 或名称。

### `clear()`

清空全部事件监听。

### `message(type, payload=None, meta=None)`

构造标准事件对象，供调试或测试使用。

## 事件对象

标准事件对象字段：

- `type`
- `source`
- `correlationId`
- `timestamp`
- `priority`
- `payload`
