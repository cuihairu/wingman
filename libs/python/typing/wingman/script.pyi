from __future__ import annotations

from typing import Any

def list() -> list[dict[str, Any]]:
	"""全部脚本信息 [{name, path, state, language, lastError?}]。"""
def get_state(name: str) -> str:
	"""脚本状态字符串；未知名返回 "unknown"（camelCase 形式为 getState）。"""
def is_running(name: str) -> bool:
	"""是否运行中（camelCase 形式为 isRunning）。"""
def has(name: str) -> bool:
	"""脚本是否已注册。"""
def reload(name: str) -> bool:
	"""重载脚本。"""
def run(name: str) -> bool:
	"""运行脚本。"""
def stop(name: str) -> bool:
	"""停止脚本。"""
def set_env(name: str, key: str, value: str) -> bool:
	"""设置环境变量（当前为全局环境，name 参数保留兼容调用形态；camelCase 形式为 setEnv）。"""
def get_env(name: str, key: str) -> str:
	"""读取环境变量，未设置返回空串（camelCase 形式为 getEnv）。"""
def set_config(key: str, value: str) -> bool:
	"""设置全局配置（camelCase 形式为 setConfig）。"""
def get_config(key: str) -> str:
	"""读取全局配置，未设置返回空串（camelCase 形式为 getConfig）。"""
def set_hot_reload(enabled: bool) -> bool:
	"""开关脚本热重载（camelCase 形式为 setHotReload）。"""
def load(name: str, path: str, config: dict[str, Any] | None = ...) -> bool:
	"""注册脚本文件；config 含 autoReload/sandboxed/timeoutMs。"""
