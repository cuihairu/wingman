#ifdef __APPLE__

#include "wingman/platform/ifilewatcher.hpp"
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wingman::platform::mac {

class FSEventsFileWatcher : public IFileWatcher {
public:
    FSEventsFileWatcher() = default;
    ~FSEventsFileWatcher() override { shutdown(); }

    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        // 锁内摘除条目、锁外停流（同 win32/inotify 回调路径模式：缩短临界区，
        // 飞行中的回调经控制块 shared_ptr 拷贝持有载荷，不依赖 map 存活）
        std::vector<std::pair<uint64_t, WatchInfo>> retired;
        {
            std::lock_guard lock(mutex_);
            for (auto& [id, info] : watches_) {
                retired.emplace_back(id, std::move(info));
            }
            watches_.clear();
            initialized_ = false;
        }
        for (auto& [id, info] : retired) {
            releaseStream(info.stream, info.queue, info.context);
        }
    }

    uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) override {
        if (!initialized_) return 0;

        CFStringRef cfPath = CFStringCreateWithCString(nullptr, path.c_str(), kCFStringEncodingUTF8);
        CFArrayRef pathsToWatch = CFArrayCreate(nullptr, (const void**)&cfPath, 1, nullptr);

        uint64_t id = nextId_++;

        // 回调载荷控制块：CF 回调只给 void*，载荷必须与 watches_ map 生命周期
        // 解耦——堆上 shared_ptr 容器由 unwatch 侧 delete（仅删容器），飞行中的
        // 回调已拷贝走 shared_ptr，引用计数保证载荷存活到回调返回。
        auto context = std::make_shared<CallbackContext>();
        context->watchId = id;
        context->callback = std::move(callback);
        auto* contextHolder = new std::shared_ptr<CallbackContext>(std::move(context));

        FSEventStreamContext ctx{};
        ctx.info = contextHolder;

        FSEventStreamRef stream = FSEventStreamCreate(
            nullptr,
            &FSEventsFileWatcher::eventCallback,
            &ctx,
            pathsToWatch,
            kFSEventStreamEventIdSinceNow,
            0.5,  // latency in seconds
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer
        );

        CFRelease(pathsToWatch);
        CFRelease(cfPath);

        if (!stream) {
            spdlog::error("FSEventsFileWatcher: FSEventStreamCreate failed for {}", path);
            delete contextHolder;
            return 0;
        }

        // 每 stream 专用串行队列：事件回调严格串行（保持发生顺序），不与他
        // stream 的回调并发交错。queue 由 stream 持有，释放责任在 releaseStream。
        dispatch_queue_t queue = dispatch_queue_create("wingman.fsevents.watch", DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(stream, queue);

        if (!FSEventStreamStart(stream)) {
            spdlog::error("FSEventsFileWatcher: FSEventStreamStart failed for {}", path);
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
            dispatch_release(queue);
            delete contextHolder;
            return 0;
        }

        std::lock_guard lock(mutex_);
        watches_[id] = {path, recursive, stream, queue, contextHolder};
        return id;
    }

    bool unwatch(uint64_t watchId) override {
        WatchInfo retired;
        {
            std::lock_guard lock(mutex_);
            auto it = watches_.find(watchId);
            if (it == watches_.end()) return false;
            retired = std::move(it->second);
            watches_.erase(it);
        }
        releaseStream(retired.stream, retired.queue, retired.context);
        return true;
    }

    size_t unwatchPath(const std::string& path) override {
        std::vector<WatchInfo> retired;
        {
            std::lock_guard lock(mutex_);
            for (auto it = watches_.begin(); it != watches_.end();) {
                if (it->second.path == path) {
                    retired.push_back(std::move(it->second));
                    it = watches_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& info : retired) {
            releaseStream(info.stream, info.queue, info.context);
        }
        return retired.size();
    }

    size_t getWatchCount() const override {
        std::lock_guard lock(mutex_);
        return watches_.size();
    }

    bool hasWatches() const override {
        std::lock_guard lock(mutex_);
        return !watches_.empty();
    }

    std::string getBackendName() const override { return "FSEvents"; }
    BackendInfo getBackendInfo() const override {
        return {"FSEvents", "1.0", initialized_, "macOS FSEvents file watcher"};
    }

private:
    bool initialized_ = false;
    std::atomic<uint64_t> nextId_{1};

    // CF 回调载荷（watch() 经控制块移交，回调经 shared_ptr 拷贝持有）
    struct CallbackContext {
        uint64_t watchId = 0;
        FileChangeCallback callback;
    };

    struct WatchInfo {
        std::string path;
        bool recursive;
        FSEventStreamRef stream = nullptr;
        dispatch_queue_t queue = nullptr;
        std::shared_ptr<CallbackContext>* context = nullptr;  // 堆上控制块容器（回调在此）
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, WatchInfo> watches_;

    static uint64_t nowMilliseconds() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
        );
    }

    // 停流释放（锁外调用）。Release 后飞行回调已持有载荷 shared_ptr 拷贝；
    // 此处 delete 只销毁控制块容器，不销毁载荷本体。
    static void releaseStream(FSEventStreamRef stream, dispatch_queue_t queue,
                              std::shared_ptr<CallbackContext>* context) {
        if (stream) {
            FSEventStreamStop(stream);
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
        }
        if (queue) {
            dispatch_release(queue);
        }
        delete context;
    }

    static void eventCallback(
        ConstFSEventStreamRef /*streamRef*/,
        void* clientCallBackInfo,
        size_t numEvents,
        void* eventPaths,
        const FSEventStreamEventFlags eventFlags[],
        const FSEventStreamEventId /*eventIds*/[])
    {
        auto* holder = static_cast<std::shared_ptr<CallbackContext>*>(clientCallBackInfo);
        if (!holder || !*holder) return;
        // 拷贝持有：unwatch/shutdown 侧 delete 控制块容器后，本回调仍持有效载荷
        std::shared_ptr<CallbackContext> context = *holder;

        auto** paths = static_cast<char**>(eventPaths);
        const uint64_t timestamp = nowMilliseconds();

        for (size_t i = 0; i < numEvents; i++) {
            FileChange change;
            change.path = paths[i] ? paths[i] : "";
            change.timestamp = timestamp;

            if (eventFlags[i] & kFSEventStreamEventFlagItemCreated) change.type = FileChangeType::Added;
            else if (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) change.type = FileChangeType::Removed;
            else if (eventFlags[i] & kFSEventStreamEventFlagItemModified) change.type = FileChangeType::Modified;
            else if (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) {
                change.type = (eventFlags[i] & kFSEventStreamEventFlagItemCreated)
                    ? FileChangeType::RenamedNew : FileChangeType::RenamedOld;
            }

            if (context->callback) {
                try {
                    context->callback(change);
                } catch (const std::exception& e) {
                    spdlog::error("FSEventsFileWatcher: callback exception: {}", e.what());
                } catch (...) {
                    spdlog::error("FSEventsFileWatcher: callback exception (unknown)");
                }
            }
        }
    }
};

// 工厂导出：facade 经前向声明消费（无公开头文件，同 linux x11_factory 模式）。
std::unique_ptr<IFileWatcher> createFSEventsFileWatcher() {
    auto watcher = std::unique_ptr<IFileWatcher>(new FSEventsFileWatcher());
    watcher->initialize();
    return watcher;
}

} // namespace wingman::platform::mac

#endif // __APPLE__
