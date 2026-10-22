> [中文](ini.md) | **English**

# INI

This document is lengthy (527 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [ini.md](ini.md) for the original.

- 格式规范
- API 函数
  - ini.decode(content)
  - ini.encode(data)
  - ini.get(data, section, key?)
  - ini.set(data, section, key, value)
  - ini.delete(data, section, key?)
  - ini.has_section(data, section)
  - ini.has_key(data, section, key)
  - ini.sections(data)
  - ini.keys(data, section)
  - ini.merge(base, override)
- 完整示例
  - Python
  - Lua
- 转义序列
- 最佳实践
  - 1. 配置文件命名
  - 2. Section 命名
  - 3. Key 命名
  - 4. 注释使用
  - 5. 配置合并
- 注意事项
