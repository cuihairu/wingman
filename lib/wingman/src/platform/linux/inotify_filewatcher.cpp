#ifdef __linux__

#include "wingman/platform/ifilewatcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace wingman::platform::linux {

class InotifyFileWatcher : public IFileWatcher {
public:
    InotifyFileWatcher() = default;
    ~InotifyFileWatcher() override { shutdown(); }

    bool initialize() override {
        inotifyFd_ = inotify_init1(IN_NONBLOCK);
        if (inotifyFd_ < 0) {
            spdlog::error("InotifyFileWatcher: inotify_init failed");
            return false;
        }
        running_ = true;
        pollThread_ = std::thread([this] { pollLoop(); });
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        running_ = false;
        if (pollThread_.joinable()) pollThread_.join();
        if (inotifyFd_ >= 0) {
            std::lock_guard lock(mutex_);
            for (const auto& [wd, _] : wdToIds_) {
                inotify_rm_watch(inotifyFd_, wd);
            }
            watches_.clear();
            wdToIds_.clear();
            wdToPath_.clear();
            close(inotifyFd_);
            inotifyFd_ = -1;
        }
        initialized_ = false;
    }

    uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) override {
        if (!initialized_) return 0;

        uint64_t id = nextId_++;
        std::lock_guard lock(mutex_);
        watches_[id] = {path, recursive, std::move(callback), {}};

        if (!addPathWatchLocked(id, path)) {
            watches_.erase(id);
            return 0;
        }

        if (recursive) {
            addRecursiveWatchesLocked(id, path);
        }
        return id;
    }

    bool unwatch(uint64_t watchId) override {
        std::lock_guard lock(mutex_);
        return removeWatchLocked(watchId);
    }

    size_t unwatchPath(const std::string& path) override {
        std::lock_guard lock(mutex_);
        std::vector<uint64_t> ids;
        for (const auto& [id, watch] : watches_) {
            if (watch.path == path) {
                ids.push_back(id);
            }
        }

        for (uint64_t id : ids) {
            removeWatchLocked(id);
        }
        return ids.size();
    }

    size_t getWatchCount() const override {
        std::lock_guard lock(mutex_);
        return watches_.size();
    }

    bool hasWatches() const override {
        std::lock_guard lock(mutex_);
        return !watches_.empty();
    }

    std::string getBackendName() const override { return "inotify"; }
    BackendInfo getBackendInfo() const override {
        return {"inotify", "1.0", initialized_, "Linux inotify file watcher"};
    }

private:
    bool initialized_ = false;
    int inotifyFd_ = -1;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
    std::atomic<uint64_t> nextId_{1};

    struct WatchInfo {
        std::string path;
        bool recursive;
        FileChangeCallback callback;
        std::vector<int> wds;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, WatchInfo> watches_;
    std::unordered_map<int, std::vector<uint64_t>> wdToIds_;
    std::unordered_map<int, std::string> wdToPath_;

    static uint64_t nowMilliseconds() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
        );
    }

    bool addPathWatchLocked(uint64_t watchId, const std::string& path) {
        uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB |
            IN_CLOSE_WRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;

        int wd = inotify_add_watch(inotifyFd_, path.c_str(), mask);
        if (wd < 0) {
            spdlog::error("InotifyFileWatcher: failed to watch {}", path);
            return false;
        }

        auto watchIt = watches_.find(watchId);
        if (watchIt == watches_.end()) {
            inotify_rm_watch(inotifyFd_, wd);
            return false;
        }

        auto& watchWds = watchIt->second.wds;
        if (std::find(watchWds.begin(), watchWds.end(), wd) == watchWds.end()) {
            watchWds.push_back(wd);
        }

        auto& ids = wdToIds_[wd];
        if (std::find(ids.begin(), ids.end(), watchId) == ids.end()) {
            ids.push_back(watchId);
        }
        wdToPath_[wd] = path;
        return true;
    }

    void addRecursiveWatchesLocked(uint64_t watchId, const std::string& rootPath) {
        std::error_code ec;
        if (!std::filesystem::is_directory(rootPath, ec)) {
            return;
        }

        const auto options = std::filesystem::directory_options::skip_permission_denied;
        for (std::filesystem::recursive_directory_iterator it(rootPath, options, ec), end;
             it != end;
             it.increment(ec)) {
            if (ec) {
                spdlog::warn("InotifyFileWatcher: recursive walk skipped entry under {}: {}", rootPath, ec.message());
                ec.clear();
                continue;
            }

            std::error_code typeEc;
            if (it->is_directory(typeEc)) {
                addPathWatchLocked(watchId, it->path().string());
            }
        }
    }

    bool removeWatchLocked(uint64_t watchId) {
        auto it = watches_.find(watchId);
        if (it == watches_.end()) return false;

        const auto wds = it->second.wds;
        for (int wd : wds) {
            auto idsIt = wdToIds_.find(wd);
            if (idsIt == wdToIds_.end()) continue;

            auto& ids = idsIt->second;
            ids.erase(std::remove(ids.begin(), ids.end(), watchId), ids.end());
            if (ids.empty()) {
                inotify_rm_watch(inotifyFd_, wd);
                wdToIds_.erase(idsIt);
                wdToPath_.erase(wd);
            }
        }

        watches_.erase(it);
        return true;
    }

    void removeDescriptorLocked(int wd) {
        auto idsIt = wdToIds_.find(wd);
        if (idsIt != wdToIds_.end()) {
            for (uint64_t id : idsIt->second) {
                auto watchIt = watches_.find(id);
                if (watchIt == watches_.end()) continue;
                auto& wds = watchIt->second.wds;
                wds.erase(std::remove(wds.begin(), wds.end(), wd), wds.end());
            }
            wdToIds_.erase(idsIt);
        }
        wdToPath_.erase(wd);
    }

    static FileChangeType changeTypeFromMask(uint32_t mask) {
        if (mask & IN_MOVED_FROM) return FileChangeType::RenamedOld;
        if (mask & IN_MOVED_TO) return FileChangeType::RenamedNew;
        if (mask & IN_CREATE) return FileChangeType::Added;
        if (mask & IN_DELETE) return FileChangeType::Removed;
        return FileChangeType::Modified;
    }

    void pollLoop() {
        constexpr size_t EVENT_BUF_LEN = 4096;
        char buffer[EVENT_BUF_LEN];

        while (running_) {
            int length = read(inotifyFd_, buffer, EVENT_BUF_LEN);
            if (length <= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            int i = 0;
            while (i < length) {
                auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);

                std::vector<std::pair<FileChangeCallback, FileChange>> pendingCallbacks;
                {
                    std::lock_guard lock(mutex_);

                    if (event->mask & IN_IGNORED) {
                        removeDescriptorLocked(event->wd);
                        i += sizeof(struct inotify_event) + event->len;
                        continue;
                    }

                    auto idsIt = wdToIds_.find(event->wd);
                    auto pathIt = wdToPath_.find(event->wd);
                    if (idsIt != wdToIds_.end() && pathIt != wdToPath_.end()) {
                        const auto ids = idsIt->second;

                        FileChange change;
                        change.type = changeTypeFromMask(event->mask);
                        change.path = pathIt->second;
                        if (event->len > 0) {
                            change.path = (std::filesystem::path(change.path) / event->name).string();
                        }
                        change.timestamp = nowMilliseconds();

                        const bool isDirectory = (event->mask & IN_ISDIR) != 0;
                        const bool directoryEnteredTree = isDirectory &&
                            ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO));

                        if (directoryEnteredTree) {
                            for (uint64_t id : ids) {
                                auto watchIt = watches_.find(id);
                                if (watchIt != watches_.end() && watchIt->second.recursive) {
                                    addPathWatchLocked(id, change.path);
                                    addRecursiveWatchesLocked(id, change.path);
                                }
                            }
                        }

                        for (uint64_t id : ids) {
                            auto watchIt = watches_.find(id);
                            if (watchIt != watches_.end() && watchIt->second.callback) {
                                pendingCallbacks.emplace_back(watchIt->second.callback, change);
                            }
                        }
                    }
                }

                for (const auto& [callback, change] : pendingCallbacks) {
                    try {
                        callback(change);
                    } catch (const std::exception& e) {
                        spdlog::error("InotifyFileWatcher: callback exception: {}", e.what());
                    }
                }

                i += sizeof(struct inotify_event) + event->len;
            }
        }
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
