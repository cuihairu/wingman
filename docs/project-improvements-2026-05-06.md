# Wingman 项目改进设计

**日期**: 2026-05-06
**目标**: 提升项目形象，改善开发者体验

## 问题概述

1. `todo.md` 放在根目录，给外部造成"项目混乱"的印象
2. README 缺少法律免责声明
3. README 没有明确标注 Windows 限定
4. 构建门槛高，依赖安装复杂
5. ONNX 特性缺少示例展示

## 改进方案

### 阶段 1：快速形象改进

| 任务 | 操作 |
|------|------|
| 移动 todo.md | `git mv todo.md docs/development-todo.md` |
| 添加免责声明 | README.md 顶部添加法律声明 |
| 改进 README | 标题添加 [Windows Only]，突出 ONNX 特性 |

### 阶段 2：开发者体验

| 任务 | 操作 |
|------|------|
| 创建 bootstrap.ps1 | 自动安装 vcpkg 和依赖 |
| 添加 ONNX 示例 | `examples/onnx_object_detection.lua` |

### 阶段 3：核心功能展示

| 任务 | 操作 |
|------|------|
| 录制 GIF | 展示截图→检测→点击的完整流程 |
| Lua Orchestration | 实现多机编排 API 绑定 |

## 设计决策

- 渐进式实施：每阶段独立可验证
- 不发版本：仅改进现有内容，不构建 release
- 保持兼容：不破坏现有构建流程
