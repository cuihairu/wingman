// 剪贴板平台工厂薄层接口：各平台目录导出 createPlatformClipboard()
// （内部完成 initialize，失败/无后端返回 nullptr）。平台分支由 CMake
// 按目录选源收敛，本头文件与调用点保持零平台宏（薄层纪律，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include <memory>

namespace wingman::platform {

class IClipboard;

std::unique_ptr<IClipboard> createPlatformClipboard();

} // namespace wingman::platform
