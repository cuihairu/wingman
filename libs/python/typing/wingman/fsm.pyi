from __future__ import annotations

from typing import Any, Callable, TypedDict

class StateContext(TypedDict, total=False):
    state: str
    from: str
    event: str
    payload: dict[str, Any]

class TransitionContext(TypedDict, total=False):
    from: str
    to: str
    event: str
    payload: dict[str, Any]

StateCallback = Callable[[StateContext], Any]
TransitionCallback = Callable[[TransitionContext], Any]
GuardCallback = Callable[[TransitionContext], bool]

def create(name: str, initial: str) -> str: ...
def state(machine_id: str, state_name: str, on_enter: StateCallback = ..., on_exit: StateCallback = ...) -> bool: ...
def transition(machine_id: str, from_state: str, to_state: str, on: str = ..., guard: GuardCallback = ..., action: TransitionCallback = ...) -> bool: ...
def dispatch(machine_id: str, event: str, payload: dict[str, Any] = ...) -> bool: ...
def current(machine_id: str) -> str: ...
def reset(machine_id: str) -> bool: ...
