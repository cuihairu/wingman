// Windows 截图薄层声明：HBITMAP → Bitmap 转换。
// Windows 类型不进公共头（公共层平台宏必须为 0，见
// docs/platform-abstraction-design.md §8）。
#pragma once

#include <memory>
#include <windows.h>

namespace wingman {
class Bitmap;
}

namespace wingman::platform::win {

std::unique_ptr<Bitmap> bitmapFromHBITMAP(HBITMAP hbitmap);

} // namespace wingman::platform::win
