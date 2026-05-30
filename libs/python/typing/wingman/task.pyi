from __future__ import annotations

from typing import Any, Callable, Literal, TypedDict, Union

class RetryOptions(TypedDict, total=False):
    max: int
    backoffMs: int
    factor: float

class TaskOptions(TypedDict, total=False):
    timeoutMs: int
    retry: RetryOptions
    metadata: dict[str, Any]

class TaskInfo(TypedDict, total=False):
    id: str
    status: TaskStatus
    result: Any
    error: str
    metadata: dict[str, Any]

TaskStatus = Literal["pending", "running", "succeeded", "failed", "canceled", "timeout"]

WorkCallback = Callable[[], Any]

def submit(work: WorkCallback, options: TaskOptions = ...) -> str: ...
def cancel(task_id: str) -> bool: ...
def status(task_id: str) -> TaskStatus: ...
def wait(task_id: str, timeout_ms: int = ...) -> bool: ...
def result(task_id: str) -> Any: ...
def error(task_id: str) -> str: ...
def retry(task_id: str, options: RetryOptions = ...) -> bool: ...
