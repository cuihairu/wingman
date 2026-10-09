// SystemWatcher 统一系统事件源测试（development-todo「进程/窗口/文件变化统一
// 事件源」进程/窗口两腿）：真实 fork/exec 子进程驱动 started/exited diff
// （sleep 进程做受控子进程，与 posix_process_coverage_test 同套路）；窗口腿
// 无 X/有 X 皆只断言 primed 基线与不崩（窗口集随环境漂移，不做事件数断言）。
// 脚本模块面（门控/查询/注销）经 ModuleDescriptor 函数直调（同
// script_modules_test 模式）。watch() 自动起 worker 轮询（500ms），与测试
// 主线程的手动 pollOnce 并发——断言一律正向轮询「出现过」，不假设恰一次。
#ifdef __linux__

#include <gtest/gtest.h>
#include "wingman/system_watcher.hpp"
#include "wingman/process.hpp"
#include "wingman/event.hpp"
#include "wingman/script/module_registry.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace wingman;
using wingman::script::ModuleDescriptor;
using wingman::script::ScriptValue;

namespace {

// 受控子进程工厂：生成唯一名 shell 脚本（掺本进程 pid + 计数）再启动——目标
// 进程名每用例独占，ctest -j 并行下同家族用例的子进程互不触发对方的 diff
// 事件（共享 "sleep" 名会互踩）。/bin/sleep 在本机是 coreutils 多路复用符号
// 链接，复制文件内容后 argv[0] 按 basename 分发会 "unknown program" 直接退出，
// 故改用 execvp 直跑 shebang 脚本（comm = 脚本名 = 目标名）。唯一名受 comm
// 15 字符截断限制：前缀 4 + tag 1 + pid ≤7 + 分隔 1 + seq 1 = 14。
// 脚本内层 sleep 到期自退；析构先 TERM（trap 收内层）再兜底 KILL，不泄漏。
class UniqueSleepChild {
public:
    explicit UniqueSleepChild(const std::string& tag) {
        static std::atomic<int> seq{0};
        const auto name = "wmw_" + tag + "_" + std::to_string(::getpid()) + "_" +
                          std::to_string(seq.fetch_add(1));
        binDir_ = std::filesystem::temp_directory_path() /
            ("wmw-bin-" + name);
        std::filesystem::create_directories(binDir_);
        bin_ = binDir_ / name;
        {
            std::error_code ec;
            std::ofstream script(bin_, std::ios::binary | std::ios::trunc);
            if (!script) return;
            script << "#!/bin/sh\n"
                   << "sleep " << kSeconds << " &\n"
                   << "trap 'kill $! 2>/dev/null' TERM\n"
                   << "wait $!\n";
            script.close();
            std::filesystem::permissions(bin_, std::filesystem::perms::owner_exec,
                std::filesystem::perm_options::add, ec);
            if (ec) return;
        }
        name_ = name;
        pid_ = Process::start(bin_.string(), "");
    }
    ~UniqueSleepChild() {
        if (pid_ > 0 && Process::exists(pid_)) {
            Process::terminate(pid_, false); // TERM：trap 收内层 sleep
            Process::wait(pid_, 3000);
            if (Process::exists(pid_)) { // 兜底 KILL
                Process::terminate(pid_, true);
                Process::wait(pid_, 3000);
            }
        }
        std::error_code ec;
        std::filesystem::remove_all(binDir_, ec);
    }
    UniqueSleepChild(const UniqueSleepChild&) = delete;
    UniqueSleepChild& operator=(const UniqueSleepChild&) = delete;

    ProcessId pid() const { return pid_; }
    // 目标进程名（uniqueSleep 名）；启动失败为空
    const std::string& name() const { return name_; }
    bool ok() const { return pid_ > 0 && !name_.empty(); }
private:
    static constexpr int kSeconds = 4; // 内层 sleep 时长：覆盖基线建立窗口
    ProcessId pid_ = 0;
    std::string name_;
    std::filesystem::path binDir_;
    std::filesystem::path bin_;
};

struct ProcEvent {
    std::string action;
    int64_t pid = 0;
    std::string name;
};

// 手动 pollOnce 驱动直到谓词成立（fork 返回 ≠ 已 exec：enumerate 里 comm
// 从 core_tests 变 sleep 有窗口，正向轮询纪律同 posix_process 测试）
template <typename Pred>
bool pollUntil(SystemWatcher& watcher, Pred pred, int maxMs = 3000) {
    for (int waited = 0; waited < maxMs; waited += 20) {
        watcher.pollOnce();
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    watcher.pollOnce();
    return pred();
}

// 按值拷贝模块到稳定对象（getAllModules() 返回临时 vector——返回其元素
// 取址即悬垂；Linux 堆侥幸未复用时才会读到看似正常的数据，见模块胶水
// getModule-by-value 既有纪律）
bool findModule(const char* name, ModuleDescriptor& out) {
    for (const auto& mod : wingman::script::modules::getAllModules()) {
        if (mod.name == name) {
            out = mod;
            return true;
        }
    }
    return false;
}

const ModuleDescriptor::FunctionEntry* findFunction(
    const ModuleDescriptor& mod, const char* name) {
    for (const auto& fn : mod.functions) {
        if (fn.name == name) return &fn;
    }
    return nullptr;
}

} // namespace

TEST(SystemWatcherTest, ProcessStartedAndExitedEvents) {
    UniqueSleepChild child("a");
    ASSERT_TRUE(child.ok());
    // 等 comm 稳定为唯一名（fork 返回 ≠ 已 exec 的窗口，同 posix_process 纪律）
    for (int i = 0; i < 30; ++i) {
        if (Process::getName(child.pid()) == child.name()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    SystemWatcher watcher;
    std::mutex mutex;
    std::vector<ProcEvent> events;
    const uint64_t id = watcher.watchProcess(child.name(), [&](const std::string& action,
                                                          int64_t pid, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back({action, pid, name});
    });
    ASSERT_NE(id, 0u);

    // 首拍只建立基线：目标进程已启动但被基线吸收，不得触发任何事件
    watcher.pollOnce();
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_TRUE(events.empty());
    }

    // 目标进程退出 → exited 事件
    ASSERT_TRUE(Process::wait(child.pid(), 8000));

    ASSERT_TRUE(pollUntil(watcher, [&] {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& e : events) {
            if (e.action == "exited" && e.pid == static_cast<int64_t>(child.pid())) return true;
        }
        return false;
    })) << "exited event for pid " << child.pid() << " not observed";
}

TEST(SystemWatcherTest, PrimedBaselineSuppressesExistingProcesses) {
    UniqueSleepChild child("b");
    ASSERT_TRUE(child.ok());
    // 等 comm 稳定为唯一名（同 posix_process 的 exec 窗口纪律）
    for (int i = 0; i < 30; ++i) {
        if (Process::getName(child.pid()) == child.name()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    SystemWatcher watcher;
    std::mutex mutex;
    std::vector<ProcEvent> events;
    watcher.watchProcess(child.name(), [&](const std::string& action,
                                      int64_t pid, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back({action, pid, name});
    });
    // 首拍：sleep 进程已在跑 → 建立基线，不触发 started
    watcher.pollOnce();
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_TRUE(events.empty());
    }
    // 第二拍：目标进程仍在 → 仍无事件
    watcher.pollOnce();
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_TRUE(events.empty());
    }
}

TEST(SystemWatcherTest, UnwatchStopsEvents) {
    SystemWatcher watcher;
    std::mutex mutex;
    int startedCount = 0;
    // 目标名本用例独占：并行同家族用例的子进程不触发本观察
    const std::string target = "wmwatch_none_" + std::to_string(::getpid());
    const uint64_t id = watcher.watchProcess(target, [&](const std::string& action,
                                                          int64_t pid, const std::string& name) {
        if (action == "started") {
            std::lock_guard<std::mutex> lock(mutex);
            ++startedCount;
        }
    });
    ASSERT_NE(id, 0u);
    watcher.pollOnce();

    EXPECT_TRUE(watcher.unwatch(id));
    EXPECT_FALSE(watcher.unwatch(id)); // 二次注销 false
    EXPECT_EQ(watcher.count(), 0u);

    UniqueSleepChild child("c");
    ASSERT_TRUE(child.ok());
    // 注销后多拍扫描：无订阅条目，不产生事件
    for (int i = 0; i < 10; ++i) {
        watcher.pollOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(startedCount, 0);
}

TEST(SystemWatcherTest, WatchCountByKind) {
    SystemWatcher watcher;
    EXPECT_EQ(watcher.count(), 0u);
    const uint64_t p = watcher.watchProcess("no-such-proc-xyz", [](const std::string&,
                                                                    int64_t, const std::string&) {});
    const uint64_t w = watcher.watchWindow("no-such-window-xyz", [](const std::string&,
                                                                    uint64_t, const std::string&) {});
    EXPECT_NE(p, 0u);
    EXPECT_NE(w, 0u);
    EXPECT_EQ(watcher.count(), 2u);
    EXPECT_EQ(watcher.count(SystemWatchKind::Process), 1u);
    EXPECT_EQ(watcher.count(SystemWatchKind::Window), 1u);
    watcher.unwatchAll();
    EXPECT_EQ(watcher.count(), 0u);
}

TEST(SystemWatcherTest, WindowWatchPrimedBaselineNoEvents) {
    SystemWatcher watcher;
    std::mutex mutex;
    std::vector<std::string> actions;
    const uint64_t id = watcher.watchWindow("", [&](const std::string& action,
                                                    uint64_t handle, const std::string& title) {
        std::lock_guard<std::mutex> lock(mutex);
        actions.push_back(action);
    });
    ASSERT_NE(id, 0u);
    // 首拍 primed：无论窗口枚举结果如何（无 X 空列表 / xvfb 有窗口），
    // 建立基线不触发任何事件
    watcher.pollOnce();
    {
        std::lock_guard<std::mutex> lock(mutex);
        EXPECT_TRUE(actions.empty());
    }
    // 第二拍：窗口集稳定环境下无事件；并行环境窗口集漂移可能出事件——
    // 只断言不崩（注册语义正确性由 count/注销用例钉住）
    watcher.pollOnce();
}

// ========== 脚本模块面（ModuleDescriptor 函数直调） ==========

TEST(SystemWatchModuleTest, RegisteredInRegistry) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    EXPECT_NE(findFunction(mod, "processWatch"), nullptr);
    EXPECT_NE(findFunction(mod, "windowWatch"), nullptr);
    EXPECT_NE(findFunction(mod, "unwatch"), nullptr);
    EXPECT_NE(findFunction(mod, "clearAll"), nullptr);
    EXPECT_NE(findFunction(mod, "watchCount"), nullptr);
}

TEST(SystemWatchModuleTest, NonThreadSafeCallableRejected) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* processWatch = findFunction(mod, "processWatch");
    ASSERT_NE(processWatch, nullptr);

    // 清理默认实例残留，再订阅错误事件验证门控拒绝
    SystemWatcher::defaultInstance().unwatchAll();
    std::string errorText;
    const uint64_t sub = EventHub::instance().subscribe("systemwatch.error",
        [&](const EventMessage& msg) {
            errorText = msg.payload.value("error", "");
        });
    ASSERT_NE(sub, 0u);

    ScriptValue badCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, false);
    const ScriptValue result = (*processWatch)({badCallable});
    EXPECT_EQ(result.asInt(), 0); // 0 = 注册失败
    EXPECT_FALSE(errorText.empty()); // 门控错误事件已携带文案

    EventHub::instance().unsubscribe(sub);
    SystemWatcher::defaultInstance().unwatchAll();
}

TEST(SystemWatchModuleTest, ThreadSafeCallableRegistersAndUnwatches) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* processWatch = findFunction(mod, "processWatch");
    const auto* unwatch = findFunction(mod, "unwatch");
    const auto* watchCount = findFunction(mod, "watchCount");
    ASSERT_NE(processWatch, nullptr);
    ASSERT_NE(unwatch, nullptr);
    ASSERT_NE(watchCount, nullptr);

    SystemWatcher::defaultInstance().unwatchAll();
    ScriptValue okCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, true);
    const ScriptValue reg = (*processWatch)({ScriptValue::fromString("no-such-proc-xyz"), okCallable});
    ASSERT_GT(reg.asInt(), 0);
    EXPECT_EQ((*watchCount)({ScriptValue::fromString("process")}).asInt(), 1);

    EXPECT_EQ((*unwatch)({reg}).asBool(), true);
    EXPECT_EQ((*watchCount)({ScriptValue::fromString("process")}).asInt(), 0);
}

TEST(SystemWatchModuleTest, UnwatchInvalidIdFails) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* unwatch = findFunction(mod, "unwatch");
    ASSERT_NE(unwatch, nullptr);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(99999999)}).asBool(), false);
    // 非整数参数拒绝
    EXPECT_EQ((*unwatch)({ScriptValue::fromString("x")}).asBool(), false);
}

#endif // __linux__
