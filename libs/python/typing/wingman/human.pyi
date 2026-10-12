from __future__ import annotations

from typing import Any

def mouse_move(x: int, y: int, duration: int = ...) -> None: ...
def mouse_click(x: int, y: int) -> None: ...
def mouse_right_click(x: int, y: int) -> None:
	"""右键单击（camelCase 形式为 mouse_rightClick）。"""
def mouse_double_click(x: int, y: int) -> None:
	"""双击（camelCase 形式为 mouse_doubleClick）。"""
def mouse_drag(from_x: int, from_y: int, to_x: int, to_y: int) -> None: ...
def mouse_scroll(x: int, y: int, delta: int = ...) -> None: ...
def keyboard_press(vk_code: int) -> None: ...
def keyboard_down(vk_code: int) -> None: ...
def keyboard_up(vk_code: int) -> None: ...
def keyboard_type(text: str, random_case: bool = ...) -> None: ...
def get_config() -> dict[str, Any]:
	"""{delay_min, delay_max, move_speed, typing_variance}（camelCase 形式为 getConfig）。"""
def set_config(key: str, value: Any) -> None:
	"""按键设置单项配置（camelCase 形式为 setConfig）。"""
def set_delay_range(min: int, max: int) -> None:
	"""操作间随机延迟区间（camelCase 形式为 setDelayRange）。"""
def set_move_speed(speed: float) -> None:
	"""鼠标移动速度系数（camelCase 形式为 setMoveSpeed）。"""
def set_typing_variance(variance: float) -> None:
	"""打字节奏随机度（camelCase 形式为 setTypingVariance）。"""
def random_delay(min: int | None = ..., max: int | None = ...) -> None:
	"""随机延迟；缺省用配置区间（camelCase 形式为 randomDelay）。"""
def natural_type(text: str) -> None:
	"""拟人打字（camelCase 形式为 naturalType）。"""
def move_mouse(x1: int, y1: int, x2: int, y2: int, duration: int | None = ...) -> None:
	"""拟人轨迹移动（camelCase 形式为 moveMouse）。"""
def natural_click(x: int, y: int) -> None:
	"""带随机偏移与延迟的拟人点击（camelCase 形式为 naturalClick；需 VISION 构建提供截屏）"""
