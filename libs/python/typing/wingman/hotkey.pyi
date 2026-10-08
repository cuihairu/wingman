from __future__ import annotations

from typing import Any, Callable

# 全局热键：回调从 HotkeyManager 后台轮询线程触发，需传 Python 函数
# （线程安全 callable；Lua callable 会被拒绝并以 id=0 返回）。

HotkeyCallback = Callable[[], Any]


def register(combo: str, callback: HotkeyCallback) -> int: ...
def unregister(id: int) -> bool: ...
