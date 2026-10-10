// Windows 实现：Win32 剪贴板后端装配
#include "platform/clipboard_factory.hpp"
#include "platform/win/win32_clipboard.hpp"
#include "wingman/platform/iclipboard.hpp"
#include <spdlog/spdlog.h>

namespace wingman::platform {

std::unique_ptr<IClipboard> createPlatformClipboard() {
    auto clipboard = std::make_unique<win::Win32Clipboard>();
    if (!clipboard->initialize()) {
        spdlog::error("[Clipboard] Failed to initialize platform clipboard");
        return nullptr;
    }
    return clipboard;
}

} // namespace wingman::platform
