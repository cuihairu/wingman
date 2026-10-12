from __future__ import annotations

from typing import Any, Callable, TypedDict

# "from" 是 Python 保留字，类语法定义不了该键，走函数式 TypedDict
StateContext = TypedDict(
    "StateContext",
    {"state": str, "from": str, "event": str, "payload": dict[str, Any]},
    total=False,
)

TransitionContext = TypedDict(
    "TransitionContext",
    {"from": str, "to": str, "event": str, "payload": dict[str, Any]},
    total=False,
)

StateCallback = Callable[[StateContext], Any]
TransitionCallback = Callable[[TransitionContext], Any]
GuardCallback = Callable[[TransitionContext], bool]

def create(name: str, initial: str) -> str: ...
def state(machine_id: str, state_name: str, on_enter: StateCallback = ..., on_exit: StateCallback = ...) -> bool: ...
def transition(machine_id: str, from_state: str, to_state: str, on: str = ..., guard: GuardCallback = ..., action: TransitionCallback = ...) -> bool: ...
def dispatch(machine_id: str, event: str, payload: dict[str, Any] = ...) -> bool: ...
def current(machine_id: str) -> str: ...
def reset(machine_id: str) -> bool: ...
