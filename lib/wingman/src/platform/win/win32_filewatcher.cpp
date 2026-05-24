#include "wingman/platform/win/win32_filewatcher.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wingman::platform::windows {

WatchItem::WatchItem()
    : id(0)
    , directoryHandle(INVALID_HANDLE_VALUE)
    , recursive(false)
    , active(false) {
    buffer.resize(4096);
    memset(&overlapped, 0, sizeof(overlapped));
}

Win32FileWatcher::Win32FileWatcher()
    : initialized_(false)
    , stopping_(false)
    , nextId_(1) {}

Win32FileWatcher::~Win32FileWatcher() {
    shutdown();
}

bool Win32FileWatcher::initialize() {
    if (initialized_) {
        return true;
    }

    stopping_ = false;
    eventThread_ = std::thread([this]() { eventLoop(); });

    initialized_ = true;
    spdlog::info("[Win32FileWatcher] Initialized");
    return true;
}

void Win32FileWatcher::shutdown() {
    if (!initialized_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(watchesMutex_);
        for (auto& pair : watches_) {
            closeWatchHandle(pair.second.get());
        }
        watches_.clear();
    }

    stopping_ = true;
    eventCondition_.notify_all();

    if (eventThread_.joinable()) {
        eventThread_.join();
    }

    initialized_ = false;
    spdlog::info("[Win32FileWatcher] Shutdown");
}

uint64_t Win32FileWatcher::watch(const std::string& path, bool recursive, FileChangeCallback callback) {
    if (!initialized_) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(watchesMutex_);

    char fullPath[MAX_PATH];
    GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, nullptr);

    const DWORD attrs = GetFileAttributesA(fullPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        spdlog::error("[Win32FileWatcher] Path not found: {}", fullPath);
        return 0;
    }

    const bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

    std::string watchPath = fullPath;
    if (!isDirectory) {
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        _splitpath_s(fullPath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
        watchPath = std::string(drive) + dir;
    }

    auto item = std::make_unique<WatchItem>();
    item->id = nextId_++;
    item->path = watchPath;
    item->recursive = recursive;
    item->callback = std::move(callback);

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

    if (!beginRead(item.get())) {
        CloseHandle(item->directoryHandle);
        item->directoryHandle = INVALID_HANDLE_VALUE;
        return 0;
    }

    item->active = true;
    const uint64_t watchId = item->id;
    watches_[watchId] = std::move(item);

    spdlog::debug("[Win32FileWatcher] Started watch {} for: {}", watchId, watchPath);
    return watchId;
}

bool Win32FileWatcher::unwatch(uint64_t watchId) {
    std::lock_guard<std::mutex> lock(watchesMutex_);

    const auto it = watches_.find(watchId);
    if (it == watches_.end()) {
        return false;
    }

    closeWatchHandle(it->second.get());
    watches_.erase(it);

    spdlog::debug("[Win32FileWatcher] Stopped watch: {}", watchId);
    return true;
}

size_t Win32FileWatcher::unwatchPath(const std::string& path) {
    char fullPath[MAX_PATH];
    GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, nullptr);

    std::lock_guard<std::mutex> lock(watchesMutex_);

    size_t count = 0;
    for (auto it = watches_.begin(); it != watches_.end();) {
        if (_stricmp(it->second->path.c_str(), fullPath) == 0) {
            closeWatchHandle(it->second.get());
            it = watches_.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

size_t Win32FileWatcher::getWatchCount() const {
    std::lock_guard<std::mutex> lock(watchesMutex_);
    return watches_.size();
}

bool Win32FileWatcher::hasWatches() const {
    std::lock_guard<std::mutex> lock(watchesMutex_);
    return !watches_.empty();
}

std::string Win32FileWatcher::getBackendName() const {
    return "Win32";
}

BackendInfo Win32FileWatcher::getBackendInfo() const {
    return BackendInfo{
        "Win32",
        "1.0",
        initialized_,
        "Windows ReadDirectoryChangesW API"
    };
}

void Win32FileWatcher::closeWatchHandle(WatchItem* item) {
    if (item->active && item->directoryHandle != INVALID_HANDLE_VALUE) {
        CancelIoEx(item->directoryHandle, nullptr);
        CloseHandle(item->directoryHandle);
        item->directoryHandle = INVALID_HANDLE_VALUE;
        item->active = false;
    }
}

bool Win32FileWatcher::beginRead(WatchItem* item) {
    DWORD bytesReturned = 0;
    const BOOL result = ReadDirectoryChangesW(
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

void Win32FileWatcher::eventLoop() {
    while (!stopping_) {
        std::unique_lock<std::mutex> lock(eventMutex_);
        eventCondition_.wait_for(lock, std::chrono::milliseconds(100));
        lock.unlock();

        checkIOCompletion();
    }
}

void Win32FileWatcher::checkIOCompletion() {
    std::lock_guard<std::mutex> lock(watchesMutex_);

    for (auto& pair : watches_) {
        WatchItem* item = pair.second.get();
        if (!item->active) {
            continue;
        }

        DWORD bytesTransferred = 0;
        if (!GetOverlappedResult(item->directoryHandle, &item->overlapped, &bytesTransferred, FALSE)) {
            const DWORD error = GetLastError();
            if (error == ERROR_IO_INCOMPLETE) {
                continue;
            }

            spdlog::warn("[Win32FileWatcher] IO error: {}, restarting", error);
            beginRead(item);
            continue;
        }

        if (bytesTransferred == 0) {
            beginRead(item);
            continue;
        }

        auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(item->buffer.data());
        while (true) {
            processNotification(item, fni);

            if (fni->NextEntryOffset == 0) {
                break;
            }
            fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<uint8_t*>(fni) + fni->NextEntryOffset
            );
        }

        beginRead(item);
    }
}

void Win32FileWatcher::processNotification(WatchItem* item, FILE_NOTIFY_INFORMATION* fni) {
    const int length = static_cast<int>(fni->FileNameLength / sizeof(wchar_t));
    const std::wstring wfileName(fni->FileName, length);

    const int size = WideCharToMultiByte(CP_UTF8, 0, wfileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fileName(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wfileName.c_str(), -1, fileName.data(), size, nullptr, nullptr);

    std::string fullPath = item->path;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += '\\';
    }
    fullPath += fileName;

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

    FileChange event;
    event.type = type;
    event.path = fullPath;
    event.timestamp = GetTickCount64();

    if (item->callback) {
        try {
            item->callback(event);
        } catch (const std::exception& e) {
            spdlog::error("[Win32FileWatcher] Callback exception: {}", e.what());
        }
    }
}

} // namespace wingman::platform::windows

#endif // _WIN32
