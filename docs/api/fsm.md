# FSM API

`wingman.fsm` 规划为事件驱动状态机模块。

## 目标接口

- `create(name, initial, options?)`
- `state(name, on_enter?, on_exit?)`
- `transition(from, to, on?, guard?, action?)`
- `dispatch(event, payload?)`
- `current()`
- `reset()`

状态变化应自动触发 `fsm.changed` 事件。
