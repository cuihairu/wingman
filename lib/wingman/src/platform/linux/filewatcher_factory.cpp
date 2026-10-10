// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo
// （同 x11_factory.cpp）
#if defined(linux)
#undef linux
#endif

#include "platform/filewatcher_factory.hpp"
#include <spdlog/spdlog.h>

namespace wingman::platform::linux {
std::unique_ptr<IFileWatcher> createInotifyFileWatcher();
}

namespace wingman::platform {

std::unique_ptr<IFileWatcher> createPlatformFileWatcher() {
    auto watcher = linux::createInotifyFileWatcher();
    if (!watcher || !watcher->getBackendInfo().isInitialized) {
        spdlog::error("[FileWatcher] Failed to initialize Linux inotify file watcher");
    }
    return watcher;
}

} // namespace wingman::platform
