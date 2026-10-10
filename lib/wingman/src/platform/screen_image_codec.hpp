// Bitmap 图片编解码薄层：fromFile/save 的平台分支（GDI+/ImageIO/OpenCV/BMP
// 兜底）与跨平台共用的模板匹配核心收口在 platform 分区（screen_image_codec.cpp），
// 本头文件只暴露 Screen 各平台实现共用的模板匹配入口（零平台宏）。
#pragma once

#include "wingman/screen.hpp"

#include <string>

namespace wingman {

// 模板匹配核心（Windows 与 Linux vision 构建共用）：imread 模板 → 截图
// BGRA 数据转 BGR → TM_CCOEFF_NORMED → minMaxLoc 阈值判定，单尺度。
// regionOrigin 为截图区域在屏幕上的原点（结果坐标平移回屏幕坐标系用）。
// 仅在 Windows / WINGMAN_ENABLE_VISION 构建中有定义；macOS 的 findImage
// 是恒 false 桩，不引用本符号。
bool matchTemplateOnBitmap(const std::string& imagePath, uint8_t* bgraData,
                           int width, int height, const Point& regionOrigin,
                           double threshold, Point& result);

} // namespace wingman
