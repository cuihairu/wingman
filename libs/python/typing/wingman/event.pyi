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

class ListenerInfo(TypedDict):
    id: int
    type: str
    name: str
    once: bool

def on(type: str, callback: EventHandler, name: str = ...) -> int: ...
def once(type: str, callback: EventHandler) -> int: ...
def emit(type: str, payload: dict[str, Any] | None = ..., meta: EventMeta | dict[str, Any] | None = ...) -> bool: ...
def off(subscription: int | str) -> bool: ...
def listener(subscription: int | str) -> ListenerInfo | None: ...
def listeners(type: str) -> list[ListenerInfo]: ...
def clear(type: str | None = ...) -> None: ...
def message(type: str, payload: dict[str, Any] | None = ..., meta: EventMeta | dict[str, Any] | None = ...) -> EventMessage: ...
