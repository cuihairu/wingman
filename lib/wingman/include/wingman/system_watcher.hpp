#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace wingman {

// 观察类别：进程快照 diff / 窗口快照 diff（文件腿由 FileWatcher 承担）。
enum class SystemWatchKind { Process, Window };

// 统一系统事件源（轮询式 v1）：后台线程按固定间隔快照 Process::enumerate()
// 与 Window::enumerate()，与上一拍做 diff → 命中注册的观察目标时触发回调。
// 补齐「进程/窗口/文件变化统一事件源」的进程与窗口两腿（文件变化由
// FileWatcher 原生回调和 filewatcher 脚本模块提供，不在本类内重复）。
// 经 Process/Window 既有跨平台抽象，无平台宏（符合薄层纪律）。
// 生命周期：首个注册自动启动轮询；注销清空后线程自查退出（下次注册自动
// 重启）；stop()/析构显式停止并 join。回调在锁外触发、可能晚于注销一拍
// （触发前按 ID 复核，已注销不触发）。
// v1 已知限制：轮询间隔内出现又消失的进程/窗口可能漏检（与 hotkey 同型）。
class SystemWatcher {
public:
    // 进程事件：pid 为进程号，name 为进程名。action ∈ started|exited。
    using ProcessCallback =
        std::function<void(const std::string& action, int64_t pid, const std::string& name)>;
    // 窗口事件：handle 为窗口句柄，title 为标题。action ∈ opened|closed|changed
    // （changed = 同 handle 标题变化）。
    using WindowCallback =
        std::function<void(const std::string& action, uint64_t handle, const std::string& title)>;

    // 进程级默认实例（脚本 systemwatch 模块用；函数局部静态规避静态初始化
    // 顺序问题）。测试请自行构造。
    static SystemWatcher& defaultInstance();

    SystemWatcher() = default;
    ~SystemWatcher();

    SystemWatcher(const SystemWatcher&) = delete;
    SystemWatcher& operator=(const SystemWatcher&) = delete;

    // 注册进程观察。name 为空 = 观察全部进程；非空 = 按进程名精确匹配
    // （与 Process::find 一致）。返回注册 ID（0 = 失败：回调为空或已达上限）。
    uint64_t watchProcess(const std::string& name, ProcessCallback callback);
    // 注册窗口观察。title 为空 = 全部窗口；非空 = 标题子串匹配（与
    // Window::find 一致）。
    uint64_t watchWindow(const std::string& title, WindowCallback callback);

    // 注销（任一类别）；ID 不存在返回 false。
    bool unwatch(uint64_t id);
    // 清空全部注册（幂等）。
    void unwatchAll();

    size_t count() const;
    size_t count(SystemWatchKind kind) const;

    // 手动控制轮询（脚本模块用注册/注销自动启停即可）
    void start(uint32_t pollIntervalMs = 500);
    void stop();
    bool isRunning() const;

    // 单拍扫描（测试确定性驱动；生产由 workerLoop 周期调用）。对全部注册
    // 逐条 diff 并触发回调。公开以便测试不依赖真实计时。
    void pollOnce();

private:
    struct Entry {
        SystemWatchKind kind;
        std::string target; // 空 = 全部
        ProcessCallback onProcess;
        WindowCallback onWindow;
        // 上一拍快照：进程 pid→name，窗口 handle→title。
        std::unordered_map<int64_t, std::string> lastProcesses;
        std::unordered_map<uint64_t, std::string> lastWindows;
        bool primed = false; // 首拍建立基线，只记快照不触发
    };

    void ensureRunning();
    void workerLoop();

    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Entry> entries_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> intervalMs_{500};
    uint64_t nextId_ = 1;

    // 触发命中数上限（防病态 diff 一次性洪泛，与事件系统资源上限同型口径）。
    static constexpr size_t kMaxEntries = 1000;
};

} // namespace wingman
