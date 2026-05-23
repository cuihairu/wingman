#include "wingman/filewatcher.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "wingman/platform/win/win32_filewatcher.hpp"
using PlatformFileWatcher = wingman::platform::windows::Win32FileWatcher;
#else

namespace wingman::platform {

class NullFileWatcher final : public IFileWatcher {
public:
    bool initialize() override { return true; }
    void shutdown() override {}

    uint64_t watch(const std::string&, bool, FileChangeCallback) override { return 0; }
    bool unwatch(uint64_t) override { return false; }
    size_t unwatchPath(const std::string&) override { return 0; }

    size_t getWatchCount() const override { return 0; }
    bool hasWatches() const override { return false; }

    std::string getBackendName() const override { return "Null"; }
    BackendInfo getBackendInfo() const override {
        return BackendInfo{"Null", "1.0", true, "No-op file watcher backend"};
    }
};

} // namespace wingman::platform

using PlatformFileWatcher = wingman::platform::NullFileWatcher;
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
