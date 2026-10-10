// Linux 实现：X11 剪贴板后端装配
#include "platform/clipboard_factory.hpp"
#include "wingman/platform/iclipboard.hpp"

// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间前需要 undo（同 x11_factory.cpp）
#undef linux

namespace wingman::platform::linux {
std::unique_ptr<IClipboard> createX11Clipboard();
}

namespace wingman::platform {

std::unique_ptr<IClipboard> createPlatformClipboard() {
    // X11Clipboard 工厂内部已 initialize()；无 DISPLAY/无 xclip 时自身优雅
    // 降级，返回空则由装配处降级 Null（语义一致）
    return linux::createX11Clipboard();
}

} // namespace wingman::platform
