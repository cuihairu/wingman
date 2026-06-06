#ifdef __linux__

#include "wingman/platform/ifilewatcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

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
            close(inotifyFd_);
            inotifyFd_ = -1;
        }
        std::lock_guard lock(mutex_);
        watches_.clear();
        wdToId_.clear();
        initialized_ = false;
    }

    uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) override {
        if (!initialized_) return 0;

        uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;
        int wd = inotify_add_watch(inotifyFd_, path.c_str(), mask);
        if (wd < 0) {
            spdlog::error("InotifyFileWatcher: failed to watch {}", path);
            return 0;
        }

        uint64_t id = nextId_++;
        std::lock_guard lock(mutex_);
        watches_[id] = {path, recursive, callback, wd};
        wdToId_[wd] = id;

        if (recursive) {
            // TODO: walk subdirectories and add watches
        }

        return id;
    }

    bool unwatch(uint64_t watchId) override {
        std::lock_guard lock(mutex_);
        auto it = watches_.find(watchId);
        if (it == watches_.end()) return false;
        inotify_rm_watch(inotifyFd_, it->second.wd);
        wdToId_.erase(it->second.wd);
        watches_.erase(it);
        return true;
    }

    size_t unwatchPath(const std::string& path) override {
        std::lock_guard lock(mutex_);
        size_t count = 0;
        for (auto it = watches_.begin(); it != watches_.end();) {
            if (it->second.path == path) {
                inotify_rm_watch(inotifyFd_, it->second.wd);
                wdToId_.erase(it->second.wd);
                it = watches_.erase(it);
                count++;
            } else {
                ++it;
            }
        }
        return count;
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
    int inotifyFd_ = -1;
    std::atomic<bool> running_{false};
    std::thread pollThread_;
    std::atomic<uint64_t> nextId_{1};

    struct WatchInfo {
        std::string path;
        bool recursive;
        FileChangeCallback callback;
        int wd;
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, WatchInfo> watches_;
    std::unordered_map<int, uint64_t> wdToId_;

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
                std::lock_guard lock(mutex_);
                auto it = wdToId_.find(event->wd);
                if (it != wdToId_.end()) {
                    auto wIt = watches_.find(it->second);
                    if (wIt != watches_.end()) {
                        FileChange change;
                        change.path = wIt->second.path;
                        if (event->len > 0) {
                            change.path += "/";
                            change.path += event->name;
                        }
                        change.timestamp = 0;

                        if (event->mask & IN_CREATE) change.type = FileChangeType::Added;
                        else if (event->mask & IN_DELETE) change.type = FileChangeType::Removed;
                        else if (event->mask & IN_MODIFY) change.type = FileChangeType::Modified;
                        else if (event->mask & IN_MOVED_FROM) change.type = FileChangeType::RenamedOld;
                        else if (event->mask & IN_MOVED_TO) change.type = FileChangeType::RenamedNew;

                        wIt->second.callback(change);
                    }
                }
                i += sizeof(struct inotify_event) + event->len;
            }
        }
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
