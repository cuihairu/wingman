from __future__ import annotations

from typing import Any

def start() -> bool:
	"""开始录制，返回是否处于录制中。"""
def stop() -> bool:
	"""停止录制。"""
def pause() -> bool:
	"""暂停录制。"""
def resume() -> bool:
	"""恢复录制。"""
def clear() -> bool:
	"""清空已录制的事件序列。"""
def is_recording() -> bool:
	"""是否正在录制（camelCase 形式为 isRecording）。"""
def is_paused() -> bool:
	"""是否处于暂停（camelCase 形式为 isPaused）。"""
def get_event_count() -> int:
	"""已录制事件数（camelCase 形式为 getEventCount）。"""
def save_to_lua(path: str) -> bool:
	"""导出为可执行的 Lua 脚本（camelCase 形式为 saveToLua）。"""
def save_to_json(path: str) -> bool:
	"""导出为结构化 JSON（camelCase 形式为 saveToJSON）。"""
def load_from_json(path: str) -> bool:
	"""从 JSON 文件加载事件序列（camelCase 形式为 loadFromJSON）。"""
def playback(speed: int = ..., repeat: int = ...) -> bool:
	"""回放；speed 百分比（100 = 原速），repeat 次数。"""
def status() -> dict[str, Any]:
	"""{recording: bool, paused: bool, eventCount: int}。"""
