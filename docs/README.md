# Wingman 文档

欢迎来到 Wingman 项目文档！

## 📖 文档导航

### 快速入门

| 文档 | 说明 |
|------|------|
| [快速开始](getting-started.md) | 5 分钟快速上手 Wingman |
| [安装指南](installation.md) | 详细的安装和配置说明 |
| [第一个脚本](guides/first-script.md) | 编写你的第一个自动化脚本 |

### API 参考

| 模块 | 说明 |
|------|------|
| [核心 API](api/core.md) | 屏幕、输入、窗口等核心功能 |
| [数据持久化](api/storage.md) | kv 键值存储、SQLite 数据库 |
| [序列化](api/serialization.md) | JSON、INI 配置文件解析 |
| [脚本 API](api/script.md) | 脚本执行和管理 |
| [调试 API](api/debugging.md) | EmmyLua 调试器接口 |

### 使用指南

| 指南 | 说明 |
|------|------|
| [Lua 脚本指南](guides/lua-scripting.md) | Lua 脚本完整教程 |
| [Python 脚本指南](guides/python-scripting.md) | Python 脚本完整教程 |
| [数据库使用](guides/database.md) | SQLite 数据库使用指南 |
| [配置管理](guides/configuration.md) | INI/JSON 配置管理最佳实践 |
| [触发器系统](guides/triggers.md) | 触发器系统详解 |
| [图像识别](guides/image-recognition.md) | 图像匹配和识别 |
| [OCR 识别](guides/ocr.md) | 文字识别使用指南 |

### 架构设计

| 文档 | 说明 |
|------|------|
| [架构概览](architecture/overview.md) | 系统整体架构 |
| [架构决策](architecture/decisions.md) | 重要设计决策记录 |
| [模块架构](architecture/modules.md) | 模块化设计说明 |
| [平台抽象](architecture/platform-abstraction.md) | 跨平台抽象层设计 |

### 开发相关

| 文档 | 说明 |
|------|------|
| [开发指南](development.md) | 贡献代码和开发流程 |
| [构建系统](build-system.md) | CMake 构建系统说明 |
| [测试指南](testing.md) | 单元测试和集成测试 |
| [代码规范](coding-style.md) | C++/Lua/Python 代码规范 |

### 示例代码

| 类别 | 说明 |
|------|------|
| [基础示例](examples/basic.md) | 简单入门示例 |
| [高级示例](examples/advanced.md) | 复杂应用场景 |
| [集成示例](examples/integration.md) | 与其他系统集成 |

## 🔍 按主题查找

### 我想要...

- **🏃 快速上手** → [快速开始](getting-started.md)
- **📦 安装项目** → [安装指南](installation.md)
- **🎯 查找 API** → [API 参考](api/overview.md)
- **📝 学习脚本** → [Lua 指南](guides/lua-scripting.md) 或 [Python 指南](guides/python-scripting.md)
- **💾 存储数据** → [数据持久化](api/storage.md)
- **⚙️ 管理配置** → [配置管理](guides/configuration.md)
- **🐛 调试脚本** → [调试 API](api/debugging.md)
- **🏗️ 了解架构** → [架构概览](architecture/overview.md)
- **🤝 贡献代码** → [开发指南](development.md)

## 📝 文档索引

### 完整文档列表

```
docs/
├── README.md                    # 本文档 - 文档导航中心
├── getting-started.md           # 快速开始
├── installation.md              # 安装指南
├── development.md               # 开发指南
├── build-system.md              # 构建系统
├── testing.md                   # 测试指南
├── coding-style.md              # 代码规范
├── architecture/
│   ├── overview.md              # 架构概览
│   ├── decisions.md             # 架构决策
│   ├── modules.md               # 模块架构
│   └── platform-abstraction.md  # 平台抽象
├── api/
│   ├── overview.md              # API 总览
│   ├── core.md                  # 核心 API
│   ├── storage.md               # 数据持久化 API
│   ├── serialization.md         # 序列化 API
│   ├── script.md                # 脚本 API
│   └── debugging.md             # 调试 API
├── guides/
│   ├── first-script.md          # 第一个脚本
│   ├── lua-scripting.md         # Lua 脚本指南
│   ├── python-scripting.md     # Python 脚本指南
│   ├── database.md              # 数据库使用
│   ├── configuration.md         # 配置管理
│   ├── triggers.md              # 触发器系统
│   ├── image-recognition.md     # 图像识别
│   └── ocr.md                   # OCR 识别
└── examples/
    ├── basic.md                 # 基础示例
    ├── advanced.md              # 高级示例
    └── integration.md           # 集成示例
```

## 🆘 获取帮助

- **GitHub Issues**: [提交问题](https://github.com/cuihairu/wingman/issues)
- **讨论区**: [GitHub Discussions](https://github.com/cuihairu/wingman/discussions)
- **示例代码**: 查看 `examples/` 目录

## 📊 文档状态

| 文档 | 状态 | 说明 |
|------|------|------|
| 快速开始 | ✅ 完成 | 基础使用说明 |
| 安装指南 | 🚧 进行中 | 需补充 macOS/Linux |
| 核心 API | ✅ 完成 | 主要模块文档 |
| 数据持久化 API | 🆕 新增 | db/kv 模块文档 |
| 序列化 API | 🆕 新增 | json/ini 模块文档 |
| Lua 脚本指南 | ✅ 完成 | 完整教程 |
| Python 脚本指南 | 🚧 进行中 | 需要补充 |
| 架构文档 | ✅ 完成 | 架构设计和决策 |

> **图例**: ✅ 完成 | 🚧 进行中 | 🆕 新增 | 📅 计划中

---

<div align="center">

**[⬆ 返回首页](../README.md)**

</div>
