from __future__ import annotations

from typing import Protocol, runtime_checkable

# ========== Type Definitions ==========

# INI 数据类型：{section: {key: value}}
IniData = dict[str, dict[str, str]]

# Section 数据类型：{key: value}
IniSection = dict[str, str]

# ========== ini Module Functions ==========

def decode(content: str) -> IniData:
	"""
	解析 INI 格式字符串

	Args:
		content: INI 格式的内容

	Returns:
		解析后的数据 {section: {key: value}}

	Example:
		data = ini.decode("""
		[Server]
		host = localhost
		port = 8080

		[Database]
		host = db.example.com
		port = 5432
		""")
	"""
	...

def encode(data: IniData) -> str:
	"""
	将数据编码为 INI 格式字符串

	Args:
		data: INI 数据 {section: {key: value}}

	Returns:
		INI 格式字符串

	Example:
		data = {
			"Server": {"host": "localhost", "port": "8080"},
			"Database": {"host": "db.example.com", "port": "5432"}
		}
		content = ini.encode(data)
	"""
	...

def get(data: IniData, section: str, key: str | None = ...) -> IniSection | str | None:
	"""
	获取配置值

	Args:
		data: INI 数据
		section: section 名称
		key: key 名称（可选，不提供则返回整个 section）

	Returns:
		如果提供 key：返回对应值或 None
		如果不提供 key：返回整个 section 或 None

	Example:
		# 获取整个 section
		server = ini.get(data, "Server")

		# 获取特定值
		host = ini.get(data, "Server", "host")
		port = ini.get(data, "Server", "port")
	"""
	...

def set(data: IniData, section: str, key: str, value: str) -> str:
	"""
	设置配置值

	Args:
		data: INI 数据（会被修改）
		section: section 名称
		key: key 名称
		value: 新值

	Returns:
		设置的值

	Example:
		ini.set(data, "Server", "host", "192.168.1.1")
		ini.set(data, "Server", "port", "9000")
	"""
	...

def delete(data: IniData, section: str, key: str | None = ...) -> bool:
	"""
	删除配置

	Args:
		data: INI 数据（会被修改）
		section: section 名称
		key: key 名称（可选，不提供则删除整个 section）

	Returns:
		是否成功删除

	Example:
		# 删除整个 section
		ini.delete(data, "OldSection")

		# 删除特定 key
		ini.delete(data, "Server", "debug")
	"""
	...

def has_section(data: IniData, section: str) -> bool:
	"""
	检查 section 是否存在

	Args:
		data: INI 数据
		section: section 名称

	Returns:
		是否存在

	Example:
		if ini.has_section(data, "Server"):
			print("Server configuration exists")
	"""
	...

def has_key(data: IniData, section: str, key: str) -> bool:
	"""
	检查 key 是否存在

	Args:
		data: INI 数据
		section: section 名称
		key: key 名称

	Returns:
		是否存在

	Example:
		if ini.has_key(data, "Server", "host"):
			print("Server host is configured")
	"""
	...

def sections(data: IniData) -> list[str]:
	"""
	获取所有 section 名称

	Args:
		data: INI 数据

	Returns:
		section 名称列表

	Example:
		for section in ini.sections(data):
			print(f"Section: {section}")
	"""
	...

def keys(data: IniData, section: str) -> list[str]:
	"""
	获取指定 section 的所有 key 名称

	Args:
		data: INI 数据
		section: section 名称

	Returns:
		key 名称列表

	Example:
		for key in ini.keys(data, "Server"):
			value = ini.get(data, "Server", key)
			print(f"{key} = {value}")
	"""
	...

def merge(base: IniData, override: IniData) -> IniData:
	"""
	合并两个 INI 数据

	Args:
		base: 基础数据
		override: 覆盖数据

	Returns:
		合并后的数据

	Example:
		# 合并用户配置到默认配置
		config = ini.merge(default_config, user_config)
	"""
	...
