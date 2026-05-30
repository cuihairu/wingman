from __future__ import annotations

from typing import Any, Callable, Literal, TypedDict

TaskStatus = Literal["pending", "running", "succeeded", "failed", "canceled", "timeout"]

class RetryOptions(TypedDict, total=False):
    max: int
    backoffMs: int
    factor: float

class TaskOptions(TypedDict, total=False):
    name: str
    timeoutMs: int
    retry: RetryOptions
    metadata: dict[str, Any]

class TaskInfo(TypedDict, total=False):
    id: str
    name: str
    status: TaskStatus
    result: Any
    error: str
    metadata: dict[str, Any]

def submit(work: Callable[[dict[str, Any]], Any] | dict[str, Any], options: TaskOptions | dict[str, Any] | None = ...) -> str: ...
def cancel(taskId: str) -> bool: ...
def status(taskId: str) -> TaskStatus: ...
def wait(taskId: str, timeoutMs: int | None = ...) -> TaskInfo: ...
def result(taskId: str) -> Any: ...
def error(taskId: str) -> str | None: ...
def retry(taskId: str, options: RetryOptions | dict[str, Any] | None = ...) -> bool: ...
