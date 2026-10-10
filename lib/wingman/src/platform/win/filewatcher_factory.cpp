#include "platform/filewatcher_factory.hpp"
#include "platform/win/win32_filewatcher.hpp"
#include <spdlog/spdlog.h>

namespace wingman::platform {

std::unique_ptr<IFileWatcher> createPlatformFileWatcher() {
    auto watcher = std::make_unique<win::Win32FileWatcher>();
    if (!watcher->initialize()) {
        spdlog::error("[FileWatcher] Failed to initialize platform file watcher");
    }
    return watcher;
}

} // namespace wingman::platform
