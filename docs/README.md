# Wingman 文档

欢迎来到 Wingman 项目文档。

## 文档导航

### 快速入门

| 文档 | 说明 |
|------|------|
| [快速开始](getting-started.md) | 本地编译、运行第一个脚本 |
| [安装指南](installation.md) | 平台依赖、vcpkg 和故障排查 |
| [用户指南](user-guide.md) | 常见使用流程和脚本示例 |
| [VS Code 开发环境](development-environment.md) | Lua/Python 补全、调试和任务配置 |
| [配置指南](guides/configuration.md) | INI/JSON 配置管理实践 |

### API 参考

| 模块 | 说明 |
|------|------|
| [API 总览](api/overview.md) | 脚本 API 的整体设计和命名约定 |
| [API 索引](api/index.md) | 所有模块入口 |
| [核心 API](api/core.md) | 屏幕、输入、窗口等核心能力 |
| [数据持久化](api/storage.md) | kv 键值存储和 SQLite |
| [序列化](api/serialization.md) | JSON、INI 等格式处理 |
| [脚本 API](api/script.md) | 脚本执行、任务和事件 |
| [调试 API](api/debugging.md) | Lua 调试接口说明 |

### 使用指南

| 指南 | 说明 |
|------|------|
| [数据库使用](guides/database.md) | SQLite 和 kv 的使用方式 |
| [触发器系统](guides/triggers.md) | 自动化触发条件与动作 |
| [UIA 指南](guides/uia-guide.md) | Windows UI Automation 使用说明 |
| [YOLO 指南](guides/yolo-guide.md) | 可选 ML/ONNX 模型准备与加载验证 |

### 架构与开发

| 文档 | 说明 |
|------|------|
| [架构概览](architecture.md) | Runtime、GUI、Orchestrator 的边界 |
| [架构决策](architecture-decisions.md) | 关键设计决策记录 |
| [平台抽象设计](platform-abstraction-design.md) | 跨平台抽象层设计 |
| [UIA 抽象设计](uia-abstraction-design.md) | UI Automation 抽象层设计 |
| [项目结构](project-structure.md) | 目录职责和模块划分 |
| [开发指南](DEVELOPMENT.md) | 构建、测试和贡献流程 |
| [VS Code 开发环境](development-environment.md) | 仓库级编辑器配置和脚本调试工作流 |
| [开发工具](DEVELOPER_TOOLS.md) | 本地开发辅助工具 |
| [依赖说明](dependencies.md) | 依赖来源和用途 |

### 示例代码

| 类别 | 说明 |
|------|------|
| [示例索引](examples/index.md) | 所有示例入口 |
| [Hello World](examples/hello-world.md) | 最小脚本示例 |
| [像素检测](examples/pixel-detection.md) | 屏幕取色与条件判断 |
| [图像匹配](examples/image-matching.md) | 图像查找示例 |
| [自动循环](examples/auto-loop.md) | 持续检测和执行动作 |
| [宏录制](examples/macro-record.md) | 录制和回放操作 |
| [UI Automation](examples/ui-automation.md) | Windows 控件自动化 |

## 功能状态说明

默认构建以本地 Runtime、Lua 脚本、屏幕/输入/窗口等核心能力为主。以下模块需要额外依赖、构建开关或仍处于迭代中：

| 模块 | 当前状态 |
|------|----------|
| OCR | 可选能力，依赖 Tesseract/运行时配置 |
| ML/YOLO | 可选能力，当前脚本层支持 ONNX 模型加载与 IO 元数据查询；目标检测高层 API 仍在规划中 |
| Vision | 基础图像能力可用，高级检测按具体构建配置启用 |
| Debugger | Lua 调试接口存在，需配合调试插件/配置使用 |
| Orchestration | Go server + Dashboard + runtime agent 远程编排，属于独立部署链路 |

## 获取帮助

- [GitHub Issues](https://github.com/cuihairu/wingman/issues)
- [GitHub Discussions](https://github.com/cuihairu/wingman/discussions)
- 示例脚本目录：[`examples/`](../examples/)

---

**[返回首页](../README.md)**
