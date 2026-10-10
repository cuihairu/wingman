#include "wingman/system_watcher.hpp"

#include "wingman/process.hpp"
#include "wingman/window.hpp"

#include <chrono>
#include <utility>

namespace wingman {

SystemWatcher& SystemWatcher::defaultInstance() {
    static SystemWatcher instance;
    return instance;
}

SystemWatcher::~SystemWatcher() {
    stop();
}

uint64_t SystemWatcher::watchProcess(const std::string& name, ProcessCallback callback) {
    if (!callback) return 0;
    uint64_t id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.size() >= kMaxEntries) return 0;
        id = nextId_++;
        Entry entry;
        entry.kind = SystemWatchKind::Process;
        entry.target = name;
        entry.onProcess = std::move(callback);
        entries_.emplace(id, std::move(entry));
    }
    // ensureRunning 自行加锁，调用方不得持锁（非递归锁自死锁）
    ensureRunning();
    return id;
}

uint64_t SystemWatcher::watchWindow(const std::string& title, WindowCallback callback) {
    if (!callback) return 0;
    uint64_t id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.size() >= kMaxEntries) return 0;
        id = nextId_++;
        Entry entry;
        entry.kind = SystemWatchKind::Window;
        entry.target = title;
        entry.onWindow = std::move(callback);
        entries_.emplace(id, std::move(entry));
    }
    ensureRunning();
    return id;
}

bool SystemWatcher::unwatch(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.erase(id) > 0;
}

void SystemWatcher::unwatchAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

size_t SystemWatcher::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

size_t SystemWatcher::count(SystemWatchKind kind) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t n = 0;
    for (const auto& [id, entry] : entries_) {
        if (entry.kind == kind) ++n;
    }
    return n;
}

void SystemWatcher::start(uint32_t pollIntervalMs) {
    if (pollIntervalMs > 0) {
        intervalMs_.store(pollIntervalMs);
    }
    ensureRunning();
}

void SystemWatcher::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
        running_ = false;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool SystemWatcher::isRunning() const {
    return running_.load();
}

// ensureRunning 空转则启动轮询线程。启动判断在锁内、线程创建在锁外：
// 自查退出的 worker 已不再触碰 mutex_，锁外 join 立即返回；上一线程经
// stop() join 过时 joinable() 为假，直接跳过。
void SystemWatcher::ensureRunning() {
    bool needSpawn = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            running_ = true;
            needSpawn = true;
        }
    }
    if (!needSpawn) return;
    if (thread_.joinable()) {
        thread_.join();
    }
    try {
        thread_ = std::thread([this] { workerLoop(); });
    } catch (const std::system_error&) {
        // 线程资源耗尽：登记无效化，保持 running_ 与线程事实一致
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
}

// workerLoop 周期检查：注册清空即自查退出并同步 running_（下次注册自动
// 重启）。退出前不持锁，保证 ensureRunning/stop 的锁外 join 无死锁。
void SystemWatcher::workerLoop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ || entries_.empty()) {
                running_ = false;
                break;
            }
        }
        pollOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs_.load()));
    }
}

void SystemWatcher::pollOnce() {
    // 一拍内先取全量快照（Process/Window 各一次），再锁表逐条 diff 并收集
    // 命中，最后锁外触发——快照/触发的时序纪律与 hotkey 一致：回调可再入
    // watch/unwatch，触发前按 ID 复核条目仍在。
    std::vector<ProcessInfo> processes;
    std::vector<WindowInfo> windows;
    bool haveProcessSnapshot = false;
    bool haveWindowSnapshot = false;

    struct Hit {
        uint64_t id;
        bool isProcess;
        std::string action;
        int64_t pid = 0;
        uint64_t handle = 0;
        std::string name; // 进程名或窗口标题
    };
    std::vector<Hit> hits;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.empty()) return;

        for (auto& [id, entry] : entries_) {
            if (entry.kind == SystemWatchKind::Process) {
                if (!haveProcessSnapshot) {
                    processes = Process::enumerate();
                    haveProcessSnapshot = true;
                }
                // 目标进程集：按注册名过滤（空 = 全部，精确匹配同 Process::find）。
                std::unordered_map<int64_t, std::string> current;
                for (const auto& info : processes) {
                    if (entry.target.empty() || info.name == entry.target) {
                        current.emplace(static_cast<int64_t>(info.pid), info.name);
                    }
                }
                if (entry.primed) {
                    for (const auto& [pid, name] : current) {
                        if (entry.lastProcesses.find(pid) == entry.lastProcesses.end()) {
                            hits.push_back({id, true, "started", pid, 0, name});
                        }
                    }
                    for (const auto& [pid, name] : entry.lastProcesses) {
                        if (current.find(pid) == current.end()) {
                            hits.push_back({id, true, "exited", pid, 0, name});
                        }
                    }
                }
                entry.lastProcesses = std::move(current);
            } else {
                if (!haveWindowSnapshot) {
                    windows = Window::enumerate();
                    haveWindowSnapshot = true;
                }
                std::unordered_map<uint64_t, std::string> current;
                for (const auto& info : windows) {
                    // WindowHandle 在 Windows 是 HWND（不透明指针），需先经
                    // uintptr_t 中转才能转整型（同 window_module 胶水惯例）；
                    // 其余平台本就是 uint64_t，reinterpret_cast 对整型非法
#ifdef _WIN32
                    const uint64_t handle =
                        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(info.handle));
#else
                    const uint64_t handle = static_cast<uint64_t>(info.handle);
#endif
                    if (entry.target.empty() ||
                        info.title.find(entry.target) != std::string::npos) {
                        current.emplace(handle, info.title);
                    }
                }
                if (entry.primed) {
                    for (const auto& [handle, title] : current) {
                        auto prev = entry.lastWindows.find(handle);
                        if (prev == entry.lastWindows.end()) {
                            hits.push_back({id, false, "opened", 0, handle, title});
                        } else if (prev->second != title) {
                            hits.push_back({id, false, "changed", 0, handle, title});
                        }
                    }
                    for (const auto& [handle, title] : entry.lastWindows) {
                        if (current.find(handle) == current.end()) {
                            hits.push_back({id, false, "closed", 0, handle, title});
                        }
                    }
                }
                entry.lastWindows = std::move(current);
            }
            // 首拍只建立基线不触发（primed 置位对两种类别一致生效）。
            entry.primed = true;
        }
    }

    // 锁外触发，触发前按 ID 复核（快照与触发之间可能被注销）。
    for (const auto& hit : hits) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(hit.id);
        if (it == entries_.end()) continue;
        if (hit.isProcess && it->second.onProcess) {
            it->second.onProcess(hit.action, hit.pid, hit.name);
        } else if (!hit.isProcess && it->second.onWindow) {
            it->second.onWindow(hit.action, hit.handle, hit.name);
        }
    }
}

} // namespace wingman
