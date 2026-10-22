> [中文](screen.md) | **English**

# Screen

This document is lengthy (593 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [screen.md](screen.md) for the original.

- 模块概述
  - 坐标系统
  - 区域对象（region）
  - 颜色值
- 截取屏幕
  - capture()
- 截取指定区域
  - captureRegion(region) / capture_region(region)
- 获取像素颜色
  - getPixel(x, y)
- 查找颜色（首个匹配）
  - findColor(color, region, tolerance=10)
- 查找颜色（所有匹配）
  - findColors(color, region, tolerance=10, maxCount=0)
- 图像匹配
  - findImage(imagePath, region?, threshold=0.9)
- 获取屏幕尺寸
  - getScreenWidth() / getScreenHeight()
- 完整示例
  - 游戏挂机脚本
- 可用接口
  - 截图操作
  - 像素操作
  - 图像操作
  - 屏幕尺寸
