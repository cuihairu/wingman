from __future__ import annotations

def totp(secret: str, digits: int = ..., period: int = ...) -> str:
	"""TOTP 动态码（Base32 密钥；位数与周期可调）。"""
def steam_guard(secret: str) -> str:
	"""Steam 风格 5 位动态码（camelCase 形式为 steamGuard）。"""
def verify(secret: str, code: str, digits: int = ..., period: int = ..., window: int = ...) -> bool:
	"""校验动态码，window 为允许的周期偏移。"""
def remaining(period: int = ...) -> int:
	"""当前周期剩余秒数。"""
