#include "wingman/filewatcher.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "wingman/platform/win/win32_filewatcher.hpp"
using PlatformFileWatcher = wingman::platform::windows::Win32FileWatcher;
#elif defined(__linux__)
#include "wingman/platform/linux/inotify_filewatcher.hpp"
using PlatformFileWatcher = wingman::platform::linux::InotifyFileWatcher;
#elif defined(__APPLE__)
#include "wingman/platform/mac/fsevents_filewatcher.hpp"
using PlatformFileWatcher = wingman::platform::macos::FSEventsFileWatcher;
#else
#error "No file watcher implementation for this platform"
#endif

namespace wingman {

// ========== FileWatcher 实现 ==========

platform::IFileWatcher& FileWatcher::instance() {
    static std::unique_ptr<PlatformFileWatcher> instance = [] {
        auto watcher = std::make_unique<PlatformFileWatcher>();
        if (!watcher->initialize()) {
            spdlog::error("[FileWatcher] Failed to initialize platform file watcher");
        }
        return watcher;
    }();
    return *instance;
}

// ========== 便捷静态方法 ==========

uint64_t FileWatcher::watch(const std::string& path,
                            std::function<void(const platform::FileChange&)> callback,
                            bool recursive) {
    return instance().watch(path, recursive, callback);
}

bool FileWatcher::unwatch(uint64_t watchId) {
    return instance().unwatch(watchId);
}

size_t FileWatcher::unwatchPath(const std::string& path) {
    return instance().unwatchPath(path);
}

size_t FileWatcher::getWatchCount() {
    return instance().getWatchCount();
}

bool FileWatcher::hasWatches() {
    return instance().hasWatches();
}

} // namespace wingman
