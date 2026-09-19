from __future__ import annotations

from typing import Any, Callable

TimerCallback = Callable[[], Any]

def after(ms: int, callback: TimerCallback) -> int:
    """一次性定时器：ms 毫秒后在 timer 线程触发一次 callback，返回 timerId。"""
def set_timeout(ms: int, callback: TimerCallback) -> int:
    """after 的 JS 风格别名（camelCase 形式为 setTimeout）。"""
def every(ms: int, callback: TimerCallback) -> int:
    """周期定时器：每 ms 毫秒触发一次 callback，直到 clear_timer/clear_all。"""
def set_interval(ms: int, callback: TimerCallback) -> int:
    """every 的 JS 风格别名（camelCase 形式为 setInterval）。"""
def clear_timer(timerId: int) -> bool:
    """取消一次性或周期定时器；已触发或不存在返回 False（camelCase 形式为 clearTimer）。"""
def cancel(timerId: int) -> bool:
    """clear_timer 的别名。"""
def clear_timeout(timerId: int) -> bool:
    """clear_timer 的 JS 风格别名（camelCase 形式为 clearTimeout）。"""
def clear_interval(timerId: int) -> bool:
    """clear_timer 的 JS 风格别名（camelCase 形式为 clearInterval）。"""
def exists(timerId: int) -> bool:
    """查询定时器是否仍在等待触发。"""
def count() -> int:
    """当前待触发的定时器数量。"""
def clear_all() -> int:
    """清理全部待触发定时器，返回清理数量（camelCase 形式为 clearAll）。"""
def sleep(ms: int) -> None:
    """同步阻塞当前脚本线程 ms 毫秒（负值视为 0）。"""
