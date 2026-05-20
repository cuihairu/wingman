#include "wingman/platform/ifilewatcher.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wingman::platform::windows {

/**
 * @brief 监控项
 */
struct WatchItem {
    uint64_t id;
    std::string path;
    HANDLE directoryHandle;
    OVERLAPPED overlapped;
    std::vector<uint8_t> buffer;
    bool recursive;
    FileChangeCallback callback;
    bool active;

    WatchItem() : id(0), directoryHandle(INVALID_HANDLE_VALUE), recursive(false), active(false) {
        buffer.resize(4096);
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = this;
    }
};

/**
 * @brief Windows 文件监控实现
 *
 * 使用 ReadDirectoryChangesW 实现文件变化监控。
 */
class Win32FileWatcher : public IFileWatcher {
public:
    Win32FileWatcher() : initialized_(false), stopping_(false), nextId_(1) {}

    ~Win32FileWatcher() override {
        shutdown();
    }

    bool initialize() override {
        if (initialized_) return true;

        // 启动事件处理线程
        stopping_ = false;
        eventThread_ = std::thread([this]() { eventLoop(); });

        initialized_ = true;
        spdlog::info("[Win32FileWatcher] Initialized");
        return true;
    }

    void shutdown() override {
        if (!initialized_) return;

        // 停止所有监控
        std::lock_guard<std::mutex> lock(watchesMutex_);
        for (auto& pair : watches_) {
            closeWatchHandle(pair.second.get());
        }
        watches_.clear();

        // 停止事件线程
        stopping_ = true;
        eventCondition_.notify_all();

        if (eventThread_.joinable()) {
            eventThread_.join();
        }

        initialized_ = false;
        spdlog::info("[Win32FileWatcher] Shutdown");
    }

    uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) override {
        if (!initialized_) return 0;

        std::lock_guard<std::mutex> lock(watchesMutex_);

        // 转换路径为绝对路径
        char fullPath[MAX_PATH];
        GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, nullptr);

        // 检查是否是目录
        DWORD attrs = GetFileAttributesA(fullPath);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            spdlog::error("[Win32FileWatcher] Path not found: {}", fullPath);
            return 0;
        }

        bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

        // 如果是文件，获取其目录
        std::string watchPath = fullPath;
        if (!isDirectory) {
            char drive[_MAX_DRIVE];
            char dir[_MAX_DIR];
            _splitpath_s(fullPath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
            watchPath = std::string(drive) + dir;
        }

        // 创建监控项
        auto item = std::make_unique<WatchItem>();
        item->id = nextId_++;
        item->path = watchPath;
        item->recursive = recursive;
        item->callback = callback;

        // 打开目录
        item->directoryHandle = CreateFileA(
            watchPath.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (item->directoryHandle == INVALID_HANDLE_VALUE) {
            spdlog::error("[Win32FileWatcher] Failed to open directory: {}", watchPath);
            return 0;
        }

        // 开始异步读取
        if (!beginRead(item.get())) {
            CloseHandle(item->directoryHandle);
            return 0;
        }

        item->active = true;
        uint64_t watchId = item->id;
        watches_[watchId] = std::move(item);

        spdlog::debug("[Win32FileWatcher] Started watch {} for: {}", watchId, watchPath);
        return watchId;
    }

    bool unwatch(uint64_t watchId) override {
        std::lock_guard<std::mutex> lock(watchesMutex_);

        auto it = watches_.find(watchId);
        if (it == watches_.end()) {
            return false;
        }

        closeWatchHandle(it->second.get());
        watches_.erase(it);

        spdlog::debug("[Win32FileWatcher] Stopped watch: {}", watchId);
        return true;
    }

    size_t unwatchPath(const std::string& path) override {
        char fullPath[MAX_PATH];
        GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, nullptr);

        std::lock_guard<std::mutex> lock(watchesMutex_);

        size_t count = 0;
        for (auto it = watches_.begin(); it != watches_.end();) {
            if (_stricmp(it->second->path.c_str(), fullPath) == 0) {
                closeWatchHandle(it->second.get());
                it = watches_.erase(it);
                count++;
            } else {
                ++it;
            }
        }

        return count;
    }

    size_t getWatchCount() const override {
        std::lock_guard<std::mutex> lock(watchesMutex_);
        return watches_.size();
    }

    bool hasWatches() const override {
        std::lock_guard<std::mutex> lock(watchesMutex_);
        return !watches_.empty();
    }

    std::string getBackendName() const override {
        return "Win32";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Win32",
            "1.0",
            initialized_,
            "Windows ReadDirectoryChangesW API"
        };
    }

private:
    bool initialized_;
    std::atomic<bool> stopping_;
    std::atomic<uint64_t> nextId_;

    std::unordered_map<uint64_t, std::unique_ptr<WatchItem>> watches_;
    mutable std::mutex watchesMutex_;

    // 事件处理线程
    std::thread eventThread_;
    std::queue<FileChange> eventQueue_;
    std::mutex eventMutex_;
    std::condition_variable eventCondition_;

    void closeWatchHandle(WatchItem* item) {
        if (item->active && item->directoryHandle != INVALID_HANDLE_VALUE) {
            // 取消 IO
            CancelIo(item->directoryHandle);

            // 等待完成
            Sleep(50);

            CloseHandle(item->directoryHandle);
            item->directoryHandle = INVALID_HANDLE_VALUE;
            item->active = false;
        }
    }

    bool beginRead(WatchItem* item) {
        DWORD bytesReturned = 0;
        BOOL result = ReadDirectoryChangesW(
            item->directoryHandle,
            item->buffer.data(),
            static_cast<DWORD>(item->buffer.size()),
            item->recursive,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SECURITY,
            &bytesReturned,
            &item->overlapped,
            nullptr
        );

        if (!result && GetLastError() != ERROR_IO_PENDING) {
            spdlog::error("[Win32FileWatcher] ReadDirectoryChangesW failed: {}", GetLastError());
            return false;
        }

        return true;
    }

    void eventLoop() {
        while (!stopping_) {
            std::unique_lock<std::mutex> lock(eventMutex_);

            // 检查是否有事件要处理
            if (eventQueue_.empty()) {
                // 等待事件或停止信号，超时检查 IO
                eventCondition_.wait_for(lock, std::chrono::milliseconds(100));
            }

            // 处理所有待处理事件
            while (!eventQueue_.empty() && !stopping_) {
                FileChange event = eventQueue_.front();
                eventQueue_.pop();
                lock.unlock();

                // 调用回调
                try {
                    event.callback(event);
                } catch (const std::exception& e) {
                    spdlog::error("[Win32FileWatcher] Callback exception: {}", e.what());
                }

                lock.lock();
            }

            // 检查异步 IO 完成状态
            checkIOCompletion();
        }
    }

    void checkIOCompletion() {
        std::lock_guard<std::mutex> lock(watchesMutex_);

        for (auto& pair : watches_) {
            WatchItem* item = pair.second.get();
            if (!item->active) continue;

            DWORD bytesTransferred = 0;
            if (!GetOverlappedResult(item->directoryHandle, &item->overlapped, &bytesTransferred, FALSE)) {
                DWORD error = GetLastError();
                if (error == ERROR_IO_INCOMPLETE) {
                    // IO 还在进行中
                    continue;
                }

                // 错误，重新开始读取
                spdlog::warn("[Win32FileWatcher] IO error: {}, restarting", error);
                beginRead(item);
                continue;
            }

            if (bytesTransferred == 0) {
                // 无变化，重新开始读取
                beginRead(item);
                continue;
            }

            // 处理通知
            FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(item->buffer.data());
            while (true) {
                processNotification(item, fni);

                if (fni->NextEntryOffset == 0) break;
                fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<uint8_t*>(fni) + fni->NextEntryOffset
                );
            }

            // 重新开始读取
            beginRead(item);
        }
    }

    void processNotification(WatchItem* item, FILE_NOTIFY_INFORMATION* fni) {
        // 转换文件名
        int length = fni->FileNameLength / sizeof(wchar_t);
        std::wstring wfileName(fni->FileName, length);

        // 转换为 UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, wfileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string fileName(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wfileName.c_str(), -1, &fileName[0], size, nullptr, nullptr);

        // 构建完整路径
        std::string fullPath = item->path;
        if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
            fullPath += '\\';
        }
        fullPath += fileName;

        // 映射变化类型
        FileChangeType type = FileChangeType::Modified;
        switch (fni->Action) {
            case FILE_ACTION_ADDED:
                type = FileChangeType::Added;
                break;
            case FILE_ACTION_REMOVED:
                type = FileChangeType::Removed;
                break;
            case FILE_ACTION_MODIFIED:
                type = FileChangeType::Modified;
                break;
            case FILE_ACTION_RENAMED_OLD_NAME:
                type = FileChangeType::RenamedOld;
                break;
            case FILE_ACTION_RENAMED_NEW_NAME:
                type = FileChangeType::RenamedNew;
                break;
        }

        // 创建事件
        FileChange event;
        event.type = type;
        event.path = fullPath;
        event.timestamp = GetTickCount64();

        // 加入事件队列
        {
            std::lock_guard<std::mutex> lock(eventMutex_);
            eventQueue_.push(event);
        }
        eventCondition_.notify_one();
    }
};

} // namespace wingman::platform::windows

#endif // _WIN32
