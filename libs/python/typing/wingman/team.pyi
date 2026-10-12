from __future__ import annotations

from typing import Any, Callable

def join_team(team_id: str, member_id: str | None = ...) -> bool:
	"""加入队伍；memberId 缺省自动生成（camelCase 形式为 joinTeam）。"""
def leave_team() -> bool:
	"""离开队伍（camelCase 形式为 leaveTeam）。"""
def create_vote(subject: str, timeout: float | None = ...) -> bool:
	"""发起投票（camelCase 形式为 createVote）。"""
def cast_vote(vote_id: str, response: str) -> bool:
	"""投票（camelCase 形式为 castVote）。"""
def get_vote_result(vote_id: str) -> str:
	"""查询投票结果（camelCase 形式为 getVoteResult）。"""
def report_status(status: dict[str, Any] | str) -> bool:
	"""上报本机状态（camelCase 形式为 reportStatus）。"""
def broadcast(message: dict[str, Any] | str) -> bool:
	"""向全队广播消息（camelCase 形式为 broadcast）。"""
def get_team_status() -> str:
	"""队伍状态（camelCase 形式为 getTeamStatus）。"""
def is_joined() -> bool:
	"""是否已入队（camelCase 形式为 isJoined）。"""
def get_member_id() -> str:
	"""本机成员 ID（camelCase 形式为 getMemberId）。"""
def on(event: str, callback: Callable[[str], Any]) -> bool:
	"""订阅队伍事件（join/leave/message/vote 等，事件名带 team. 前缀）；
	callback 入参为事件负载的 JSON 字符串。"""
