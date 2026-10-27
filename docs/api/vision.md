> [中文](../zh/api/vision.md) | **English**

# Vision

This document is lengthy (545 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [vision.md](../zh/api/vision.md) for the original.

- 模块概述
- 查找颜色
  - find_color(color, tolerance?, region?) / findColor(color, tolerance?, region?)
- 查找所有颜色
  - find_all_colors(color, tolerance?, region?) / findAllColors(color, tolerance?, region?)
- 检查颜色存在
  - has_color(color, tolerance?, region?) / hasColor(color, tolerance?, region?)
- 获取主要颜色
  - get_dominant_color(region?) / getDominantColor(region?)
- 查找图像
  - find_image(template_path, threshold?, region?) / findImage(templatePath, threshold?, region?)
- 图像模板批量管理（templateAdd / templateFindAll）
- AI 视觉识别（aiSetup / aiLocate / aiElements）
  - ai_setup(cfg) / aiSetup(cfg)
  - ai_setup_status() / aiSetupStatus()
  - ai_locate(desc, region?) / aiLocate(desc, region?)
- 批量元素识别（aiElements）
  - ai_elements(desc, region?) / aiElements(desc, region?)
- 本地 ONNX 检测 provider（aiSetupLocal）
  - ai_setup_local(cfg) / aiSetupLocal(cfg)
- 可用接口
