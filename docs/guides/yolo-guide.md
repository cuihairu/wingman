> [中文](yolo-guide.md) | **English**

# YOLO Guide

This document is lengthy (349 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [yolo-guide.md](yolo-guide.md) for the original.

- 什么是 YOLO？
- 什么是 ONNX？
- 第一步：安装 Ultralytics
- 第二步：下载预训练模型
  - 自动下载（首次使用时）
  - 手动下载
- 第三步：导出为 ONNX 格式
  - 基本导出
  - 高级导出选项
  - 推荐配置（游戏自动化）
- 第四步：在 Wingman 中验证模型
  - Python 示例
  - Lua 示例
- 第五步：接入本地检测（vision.aiSetupLocal + aiLocate/aiElements）
  - aiSetupLocal(cfg)
  - Python 示例
  - Lua 示例
- 能力现状与边界
- 检测结果格式
- COCO 数据集类别
- 性能优化建议
- 常见问题
  - Q: 模型检测不到对象？
  - Q: 检测速度太慢？
  - Q: 能否检测自定义类别？
- 参考资源
