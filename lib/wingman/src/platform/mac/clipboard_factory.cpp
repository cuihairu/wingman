// macOS 实现：Cocoa 剪贴板后端装配
#include "platform/clipboard_factory.hpp"
#include "wingman/platform/iclipboard.hpp"

namespace wingman::platform::mac {
std::unique_ptr<IClipboard> createCocoaClipboard();
}

namespace wingman::platform {

std::unique_ptr<IClipboard> createPlatformClipboard() {
    // Cocoa 工厂内部已 initialize()（幂等）；返回空由装配处降级 Null
    return mac::createCocoaClipboard();
}

} // namespace wingman::platform
