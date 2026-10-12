from __future__ import annotations

def load(id: str) -> bool:
	"""加载指定档案为当前档案。"""
def save(id: str) -> bool:
	"""保存当前状态到指定档案。"""
def get(id: str) -> tuple[str, str] | None:
	"""查询档案，返回 (id, 名称)；不存在返回 None。"""
def get_active() -> tuple[str, str] | None:
	"""当前激活档案（camelCase 形式为 getActive）。"""
def set_active(id: str) -> bool:
	"""切换激活档案（camelCase 形式为 setActive）。"""
def list() -> list[str]:
	"""全部档案 ID。"""
def find_by_window(title: str) -> tuple[str, str] | None:
	"""按窗口标题匹配档案（camelCase 形式为 findByWindow）。"""
def set_profiles_directory(dir: str) -> bool:
	"""设置档案存储目录（camelCase 形式为 setProfilesDirectory）。"""
def get_profiles_directory() -> str:
	"""档案存储目录（camelCase 形式为 getProfilesDirectory）。"""
def scan() -> bool:
	"""扫描存储目录重建档案列表。"""
def create_template(game_name: str) -> tuple[str, str] | None:
	"""按游戏名创建档案模板（camelCase 形式为 createTemplate）。"""
def export_json(id: str) -> str:
	"""导出档案为 JSON 字符串（camelCase 形式为 exportJson）。"""
def import_json(json: str, id: str | None = ...) -> bool:
	"""从 JSON 导入档案，id 缺省沿用内容自带（camelCase 形式为 importJson）。"""
def export_package(id: str, output_path: str) -> bool:
	"""导出档案为分发包（camelCase 形式为 exportPackage）。"""
def import_package(package_path: str) -> bool:
	"""导入分发包（camelCase 形式为 importPackage）。"""
def delete(id: str) -> bool:
	"""删除档案。"""
