#include "wingman/filewatcher.hpp"
#include "platform/filewatcher_factory.hpp"
#include <memory>

namespace wingman {

// ========== FileWatcher Implementation ==========

// 平台后端选择收敛到各平台工厂文件（src/platform/<os>/filewatcher_factory.cpp），
// 本文件保持零平台宏（薄层纪律，docs/platform-abstraction-design.md §8）。
platform::IFileWatcher& FileWatcher::instance() {
    static std::unique_ptr<platform::IFileWatcher> instance = [] {
        return platform::createPlatformFileWatcher();
    }();
    return *instance;
}

// ========== Convenience Static Methods ==========

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
