#include "platform/filewatcher_factory.hpp"
#include <spdlog/spdlog.h>

namespace wingman::platform::mac {
std::unique_ptr<IFileWatcher> createFSEventsFileWatcher();
}

namespace wingman::platform {

std::unique_ptr<IFileWatcher> createPlatformFileWatcher() {
    auto watcher = mac::createFSEventsFileWatcher();
    if (!watcher) {
        spdlog::error("[FileWatcher] Failed to initialize macOS FSEvents file watcher");
        watcher = std::make_unique<NullFileWatcher>();
    }
    return watcher;
}

} // namespace wingman::platform
