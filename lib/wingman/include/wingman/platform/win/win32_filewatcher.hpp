#pragma once

#include "wingman/platform/ifilewatcher.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <thread>
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

    WatchItem();
};

/**
 * @brief Windows 文件监控实现
 *
 * 使用 ReadDirectoryChangesW 实现文件变化监控。
 */
class Win32FileWatcher : public IFileWatcher {
public:
    Win32FileWatcher();
    ~Win32FileWatcher() override;

    bool initialize() override;
    void shutdown() override;

    uint64_t watch(const std::string& path, bool recursive, FileChangeCallback callback) override;
    bool unwatch(uint64_t watchId) override;
    size_t unwatchPath(const std::string& path) override;

    size_t getWatchCount() const override;
    bool hasWatches() const override;

    std::string getBackendName() const override;
    BackendInfo getBackendInfo() const override;

private:
    bool initialized_;
    std::atomic<bool> stopping_;
    std::atomic<uint64_t> nextId_;

    std::unordered_map<uint64_t, std::unique_ptr<WatchItem>> watches_;
    mutable std::mutex watchesMutex_;

    // 事件处理线程
    std::thread eventThread_;
    std::mutex eventMutex_;
    std::condition_variable eventCondition_;

    void closeWatchHandle(WatchItem* item);
    bool beginRead(WatchItem* item);
    void eventLoop();
    void checkIOCompletion();
    void processNotification(WatchItem* item, FILE_NOTIFY_INFORMATION* fni);
};

} // namespace wingman::platform::windows

#endif // _WIN32
