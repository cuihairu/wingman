from __future__ import annotations

from typing import Any, Callable

def create(name: str) -> bool:
	"""创建行为树。"""
def tick(name: str) -> str:
	"""驱动一次树，返回节点状态字符串。"""
def remove(name: str) -> None:
	"""删除行为树。"""
def sequence(name: str | None = ...) -> int:
	"""顺序节点，返回节点句柄。"""
def selector(name: str | None = ...) -> int:
	"""选择节点，返回节点句柄。"""
def parallel(name: str | None = ..., policy: str | None = ...) -> int:
	"""并行节点；policy 为成功/失败判定策略，返回节点句柄。"""
def wait(milliseconds: int) -> int:
	"""等待节点，返回节点句柄。"""
def inverter(child_handle: int) -> int:
	"""反转子节点结果，返回装饰节点句柄。"""
def repeat(child_handle: int, count: int | None = ...) -> int:
	"""重复子节点；缺省为无限重复，返回装饰节点句柄。"""
def add_child(parent_handle: int, child_handle: int) -> bool:
	"""挂接子节点（camelCase 形式为 addChild）。"""
def condition(name: str, callback: Callable[[], bool]) -> int:
	"""条件叶节点，callback 返回真值即成功，返回节点句柄。"""
def action(name: str, callback: Callable[[], str]) -> int:
	"""动作叶节点，callback 返回状态字符串，返回节点句柄。"""
def set_root(tree_name: str, node_handle: int) -> bool:
	"""设置树的根节点（camelCase 形式为 setRoot）。"""
