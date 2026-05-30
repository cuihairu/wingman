from __future__ import annotations

from typing import Any, Callable, TypedDict

class EventMeta(TypedDict, total=False):
    source: str
    correlationId: str
    priority: int

class EventMessage(TypedDict):
    type: str
    source: str
    correlationId: str
    timestamp: int
    priority: int
    payload: dict[str, Any]

EventHandler = Callable[[EventMessage], Any]

def on(type: str, callback: EventHandler, name: str = ...) -> int: ...
def once(type: str, callback: EventHandler) -> int: ...
def emit(type: str, payload: dict[str, Any] | None = ..., meta: EventMeta | dict[str, Any] | None = ...) -> bool: ...
def off(subscription: int | str) -> bool: ...
def clear() -> None: ...
def message(type: str, payload: dict[str, Any] | None = ..., meta: EventMeta | dict[str, Any] | None = ...) -> EventMessage: ...
