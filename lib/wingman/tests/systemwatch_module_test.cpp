// systemwatch 脚本模块桥接缺口补测：system_watcher_test 已盖注册/注销/门控
// 基础面，本文件补齐剩余分支——参数校验矩阵、windowWatch 门控与簿记、
// clearAll、watchCount 全分支，以及回调体真实触发（payload 构建/EventHub
// 广播/脚本回调/两条 catch）。
// 进程腿触发跨平台（受控子进程工厂同 system_watcher_test：Linux=唯一名
// shebang 脚本、Windows=复制 ping.exe 唯一名自退）；窗口腿在无 WM 的 Xvfb
// 上借 TestX11Window 直写 _NET_CLIENT_LIST（enumerate 数据源），仅 Linux
// 编译、无 X 运行时 skip。Windows CI 是 codecov 唯一数据源，文件无条件编译，
// 平台差异收进文件内分支（同 system_watcher_test.cpp 先例）。
#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/event.hpp"
#include "wingman/system_watcher.hpp"
#include "wingman/process.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
// 窗口腿（Linux-only）设施：TestX11Window / X11ServerLockGuard /
// openX11Display（经头内 using 进入本 TU；Xlib 头随之带入）
#include "platform_x11_test_common.hpp"
#endif

using wingman::script::ModuleDescriptor;
using wingman::script::ScriptValue;
using wingman::EventHub;
using wingman::EventMessage;

namespace {

bool findModule(const char* name, ModuleDescriptor& out) {
    for (const auto& mod : wingman::script::modules::getAllModules()) {
        if (mod.name == name) {
            out = mod; // 按值拷贝：getAllModules() 临时 vector 取址即悬垂
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

ScriptValue nullCallable(const std::vector<ScriptValue>&) {
    return ScriptValue::null();
}

template <typename Pred>
bool waitFor(Pred pred, int maxMs = 6000) {
    for (int waited = 0; waited < maxMs; waited += 50) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return pred();
}

// 受控子进程工厂（同 system_watcher_test 的 UniqueSleepChild）：唯一名子进程，
// 掺本进程 pid 避免并行互踩；到期自退，析构宽限终止 + 兜底强杀。
class ModuleProbeChild {
public:
    explicit ModuleProbeChild(const std::string& tag) {
        static std::atomic<int> seq{0};
        const auto name = "wsw_" + tag + "_" +
                          std::to_string(wingman::Process::getCurrentId()) + "_" +
                          std::to_string(seq.fetch_add(1));
        binDir_ = std::filesystem::temp_directory_path() /
            ("wsw-bin-" + name);
        std::filesystem::create_directories(binDir_);
#ifdef _WIN32
        name_ = name + ".exe"; // 进程名 = 镜像文件名含 .exe（两处口径一致）
        bin_ = binDir_ / name_;
        char sysDir[MAX_PATH] = {};
        if (GetSystemDirectoryA(sysDir, MAX_PATH) == 0) return;
        std::error_code copyEc;
        std::filesystem::copy_file(
            std::filesystem::path(sysDir) / "ping.exe", bin_,
            std::filesystem::copy_options::overwrite_existing, copyEc);
        if (copyEc) return;
        pid_ = wingman::Process::start(bin_.string(),
                                       "127.0.0.1 -n " + std::to_string(kSeconds + 1), "");
#else
        name_ = name;
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
        pid_ = wingman::Process::start(bin_.string(), "", "");
#endif
    }
    ~ModuleProbeChild() {
        if (pid_ > 0 && wingman::Process::exists(pid_)) {
            wingman::Process::terminate(pid_, false);
            wingman::Process::wait(pid_, 3000);
            if (wingman::Process::exists(pid_)) {
                wingman::Process::terminate(pid_, true);
                wingman::Process::wait(pid_, 3000);
            }
        }
        std::error_code ec;
        std::filesystem::remove_all(binDir_, ec);
    }
    ModuleProbeChild(const ModuleProbeChild&) = delete;
    ModuleProbeChild& operator=(const ModuleProbeChild&) = delete;

    wingman::ProcessId pid() const { return pid_; }
    const std::string& name() const { return name_; }
    bool ok() const { return pid_ > 0 && !name_.empty(); }
private:
    static constexpr int kSeconds = 6;
    wingman::ProcessId pid_ = 0;
    std::string name_;
    std::filesystem::path binDir_;
    std::filesystem::path bin_;
};

struct ProcEvent {
    std::string action;
    int64_t pid = 0;
    std::string name;
};

} // namespace

TEST(SystemWatchModuleGapsTest, ProcessWatchArgumentValidation) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* processWatch = findFunction(mod, "processWatch");
    ASSERT_NE(processWatch, nullptr);

    wingman::SystemWatcher::defaultInstance().unwatchAll(); // 清场
    // 空参 / 末参非 callable → 0
    EXPECT_EQ((*processWatch)({}).asInt(), 0);
    EXPECT_EQ((*processWatch)({ScriptValue::fromString("not-a-callable")}).asInt(), 0);
    EXPECT_EQ((*processWatch)({ScriptValue::fromInt(1)}).asInt(), 0);
    EXPECT_EQ(wingman::SystemWatcher::defaultInstance().count(), 0u);
}

TEST(SystemWatchModuleGapsTest, WindowWatchValidationAndBookkeeping) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* windowWatch = findFunction(mod, "windowWatch");
    const auto* unwatch = findFunction(mod, "unwatch");
    ASSERT_NE(windowWatch, nullptr);
    ASSERT_NE(unwatch, nullptr);

    wingman::SystemWatcher::defaultInstance().unwatchAll(); // 清场

    // 空参 / 末参非 callable → 0
    EXPECT_EQ((*windowWatch)({}).asInt(), 0);
    EXPECT_EQ((*windowWatch)({ScriptValue::fromString("just-a-title")}).asInt(), 0);

    // 非线程安全 callable 拒绝 + systemwatch.error 事件（processWatch 门控
    // 已有既有用例，windowWatch 腿同门控独立覆盖）
    std::string errorText;
    const uint64_t sub = EventHub::instance().subscribe("systemwatch.error",
        [&](const EventMessage& msg) { errorText = msg.payload.value("error", ""); });
    ASSERT_NE(sub, 0u);
    ScriptValue badCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, false);
    EXPECT_EQ((*windowWatch)({badCallable}).asInt(), 0);
    EXPECT_EQ((*windowWatch)({ScriptValue::fromString("t"), badCallable}).asInt(), 0);
    EXPECT_FALSE(errorText.empty());
    EventHub::instance().unsubscribe(sub);

    // 线程安全 callable：带标题登记 → 注销 true → 重复注销 false
    const int64_t id = (*windowWatch)({ScriptValue::fromString("no-such-window-xyz"),
                                       ScriptValue::fromCallable(nullCallable, true)}).asInt();
    ASSERT_GT(id, 0);
    EXPECT_EQ(wingman::SystemWatcher::defaultInstance().count(wingman::SystemWatchKind::Window), 1u);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(id)}).asBool(), true);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(id)}).asBool(), false);
}

TEST(SystemWatchModuleGapsTest, ClearAllAndWatchCountVariants) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* processWatch = findFunction(mod, "processWatch");
    const auto* windowWatch = findFunction(mod, "windowWatch");
    const auto* clearAll = findFunction(mod, "clearAll");
    const auto* watchCount = findFunction(mod, "watchCount");
    ASSERT_NE(processWatch, nullptr);
    ASSERT_NE(windowWatch, nullptr);
    ASSERT_NE(clearAll, nullptr);
    ASSERT_NE(watchCount, nullptr);

    wingman::SystemWatcher::defaultInstance().unwatchAll(); // 清场：计数从零起算

    ScriptValue okCallable = ScriptValue::fromCallable(nullCallable, true);
    ASSERT_GT((*processWatch)({ScriptValue::fromString("p-xyz"), okCallable}).asInt(), 0);
    ASSERT_GT((*windowWatch)({ScriptValue::fromString("w-xyz"), okCallable}).asInt(), 0);

    // watchCount 全分支：缺省 = 全部；process/window 按类别；未知类别 = 0
    EXPECT_EQ((*watchCount)({}).asInt(), 2);
    EXPECT_EQ((*watchCount)({ScriptValue::fromString("process")}).asInt(), 1);
    EXPECT_EQ((*watchCount)({ScriptValue::fromString("window")}).asInt(), 1);
    EXPECT_EQ((*watchCount)({ScriptValue::fromString("bogus")}).asInt(), 0);

    // clearAll 清空全部并返回 null
    const ScriptValue cleared = (*clearAll)({});
    EXPECT_EQ((*watchCount)({}).asInt(), 0);
    EXPECT_TRUE(cleared.isNull() || cleared.asInt() == 0);
}

// 回调体真实触发（跨平台）：受控子进程驱动 started/exited。
// priming 纪律：事件只在 primed 置位后（非首拍）触发——注册后起 canary
// 子进程，观察到其 started 即证明 worker 存活且 ≥1 拍已跑（全部条目已
// primed）；其后 child2 的 started/exited 全部免竞态。canary/child2 的
// started 按 pid 断言不卡名字：首拍若落进 fork→exec 窗口（高负载下拉长），
// 事件带 pre-exec 过渡名——名字正确性属 watcher diff 语义（既有 watcher
// 用例钉），本用例只钉模块桥接面。记录回调验 payload 构建（object 形态 +
// get 三键）；抛异常回调多拍事件自然走两条 catch。等待预算放宽：worker
// 线程与轮询节拍在共享机高负载下可能被拖到秒级。
TEST(SystemWatchModuleGapsTest, ProcessCallbackPayloadEventAndErrorPaths) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* processWatch = findFunction(mod, "processWatch");
    const auto* unwatch = findFunction(mod, "unwatch");
    ASSERT_NE(processWatch, nullptr);
    ASSERT_NE(unwatch, nullptr);

    wingman::SystemWatcher::defaultInstance().unwatchAll(); // 清场

    std::mutex mutex;
    std::vector<ProcEvent> callbackEvents; // 脚本回调载荷（object 形态）
    const ScriptValue recordingCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>& args) {
            if (args.empty() || !args[0].isObject()) return ScriptValue::null();
            ProcEvent e;
            if (const auto* a = args[0].get("action")) e.action = a->asString();
            if (const auto* p = args[0].get("pid")) e.pid = p->asInt();
            if (const auto* n = args[0].get("name")) e.name = n->asString();
            std::lock_guard<std::mutex> lock(mutex);
            callbackEvents.push_back(e);
            return ScriptValue::null();
        }, true);

    // 抛异常回调：第 1 次触发抛 std::exception，第 2 次起抛非 std 对象
    std::atomic<int> throwPhase{0};
    const ScriptValue throwingCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>&) {
            if (throwPhase.fetch_add(1) == 0) {
                throw std::runtime_error("systemwatch probe");
            }
            throw 42;
            return ScriptValue::null(); // 不可达：返回类型须与 CallableFunc 对齐
        }, true);

    // EventHub 广播面（原生层订阅桥接的通路）
    std::atomic<int> hubStarted{0};
    const uint64_t sub = EventHub::instance().subscribe("systemwatch.process",
        [&](const EventMessage& msg) {
            if (msg.payload.value("action", "") == "started") {
                ++hubStarted;
            }
        });
    ASSERT_NE(sub, 0u);

    // 模块层空名单参形态同时覆盖 name 提取的 callable-only 分支
    const int64_t idRec = (*processWatch)({recordingCallable}).asInt();
    const int64_t idThrow = (*processWatch)({throwingCallable}).asInt();
    ASSERT_GT(idRec, 0);
    ASSERT_GT(idThrow, 0);

    const auto hasEvent = [&](const char* action, wingman::ProcessId pid) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& e : callbackEvents) {
            if (e.action == action && e.pid == static_cast<int64_t>(pid)) return true;
        }
        return false;
    };

    // priming 证明：周期性起探针直到观察到任一 started。首拍会静默吸收
    // 先于它存在的进程（饥饿场景首拍可迟到数秒），后于首拍起的探针必发
    // started——逐轮起探针与首拍落地时刻解耦。观察到即证明 worker 存活且
    // 全部条目已 primed。
    std::vector<std::unique_ptr<ModuleProbeChild>> probes;
    bool primed = false;
    for (int attempt = 0; attempt < 10 && !primed; ++attempt) {
        probes.push_back(std::make_unique<ModuleProbeChild>("q"));
        ASSERT_TRUE(probes.back()->ok());
        primed = waitFor([&] {
            for (const auto& p : probes) {
                if (hasEvent("started", p->pid())) return true;
            }
            return false;
        }, 2000);
    }
    ASSERT_TRUE(primed)
        << "no probe started observed — worker never ran a post-primed poll";

    // child2 在 primed 证明之后起 → started 必触发（名字可能是过渡名，按 pid 断言）
    ModuleProbeChild child2("r");
    ASSERT_TRUE(child2.ok());
    for (int i = 0; i < 30; ++i) {
        if (wingman::Process::getName(child2.pid()) == child2.name()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(waitFor([&] { return hasEvent("started", child2.pid()); }, 10000))
        << "started payload not delivered to script callback";
    EXPECT_TRUE(waitFor([&] { return hubStarted.load() > 0; }, 10000))
        << "systemwatch.process event not broadcast";

    // child2 已入基线，宽限终止 → exited；抛异常回调两拍随事件流走过
    wingman::Process::terminate(child2.pid(), false);
    ASSERT_TRUE(wingman::Process::wait(child2.pid(), 5000));
    EXPECT_TRUE(waitFor([&] { return hasEvent("exited", child2.pid()); }, 10000))
        << "exited payload not delivered to script callback";
    EXPECT_TRUE(waitFor([&] { return throwPhase.load() >= 1; }))
        << "std::exception catch path not exercised";
    EXPECT_TRUE(waitFor([&] { return throwPhase.load() >= 2; }))
        << "catch(...) path not exercised";

    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(idRec)}).asBool(), true);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(idThrow)}).asBool(), true);
    EventHub::instance().unsubscribe(sub);
    wingman::SystemWatcher::defaultInstance().unwatchAll();
}

#ifdef __linux__

// 窗口腿回调体真实触发：TestX11Window 直写 _NET_CLIENT_LIST（enumerate
// 数据源）→ opened；翻 _NET_WM_NAME → changed；销毁 → closed；第二窗再走
// 一轮 opened/closed。抛异常窗口回调随事件多拍自然走两条 catch。
// priming 纪律：共享机高负载下 worker 线程可能数秒后才被调度（首拍被饿），
// 若窗口刺激早于首拍会被基线吸收、opened 永不触发——先注册后起进程 canary，
// 观察到进程 started 即证明 worker 存活且同一 pollOnce 周期已给全部条目
// 置位 primed，窗口刺激全部排在其后，全链路免竞态。skip 探测在注册之前
// （GTEST_SKIP 直接返回，先注册会在无 X 环境向后续用例泄漏条目）。
TEST(SystemWatchModuleGapsTest, WindowCallbackOpenedChangedClosedPaths) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("systemwatch", mod));
    const auto* windowWatch = findFunction(mod, "windowWatch");
    const auto* unwatch = findFunction(mod, "unwatch");
    ASSERT_NE(windowWatch, nullptr);
    ASSERT_NE(unwatch, nullptr);

    X11ServerLockGuard x11Lock; // 根属性是 X server 全局状态，跨进程串行化
    Display* d = openX11Display(nullptr);
    if (!d) {
        GTEST_SKIP() << "X display unavailable — window firing needs a live X server";
    }

    wingman::SystemWatcher::defaultInstance().unwatchAll(); // 清场

    const std::string token = "wswprobe-" + std::to_string(wingman::Process::getCurrentId());

    std::mutex mutex;
    int openedCount = 0;
    int changedCount = 0;
    int closedCount = 0;
    const ScriptValue recordingCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>& args) {
            if (args.empty() || !args[0].isObject()) return ScriptValue::null();
            if (const auto* a = args[0].get("action")) {
                std::lock_guard<std::mutex> lock(mutex);
                const std::string action = a->asString();
                if (action == "opened") ++openedCount;
                else if (action == "changed") ++changedCount;
                else if (action == "closed") ++closedCount;
            }
            return ScriptValue::null();
        }, true);

    std::atomic<int> throwPhase{0};
    const ScriptValue throwingCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>&) {
            if (throwPhase.fetch_add(1) == 0) {
                throw std::runtime_error("window probe");
            }
            throw 42;
            return ScriptValue::null(); // 不可达：返回类型须与 CallableFunc 对齐
        }, true);

    // 进程 canary：注册后起子进程 → started 必发（pid 断言，名字可能是过渡名）
    const auto* processWatch = findFunction(mod, "processWatch");
    ASSERT_NE(processWatch, nullptr);
    std::atomic<int> canaryStarted{0};
    const ScriptValue canaryCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>& args) {
            if (args.empty() || !args[0].isObject()) return ScriptValue::null();
            if (const auto* a = args[0].get("action")) {
                if (a->asString() == "started") ++canaryStarted;
            }
            return ScriptValue::null();
        }, true);

    // 记录回调挂唯一名 token（子串匹配路径）；抛异常回调挂空名（全窗口 diff）
    const int64_t idRec = (*windowWatch)({ScriptValue::fromString(token), recordingCallable}).asInt();
    const int64_t idThrow = (*windowWatch)({throwingCallable}).asInt();
    const int64_t idCanary = (*processWatch)({canaryCallable}).asInt();
    ASSERT_GT(idRec, 0);
    ASSERT_GT(idThrow, 0);
    ASSERT_GT(idCanary, 0);

    ModuleProbeChild canary("w");
    ASSERT_TRUE(canary.ok());
    for (int i = 0; i < 30; ++i) {
        if (wingman::Process::getName(canary.pid()) == canary.name()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // 预算放宽：worker 线程在高负载下可能数秒后才被首次调度
    ASSERT_TRUE(waitFor([&] { return canaryStarted.load() > 0; }, 15000))
        << "process canary never fired — worker thread not scheduled";

    // —— worker 已证明存活且 primed，窗口刺激从此免竞态 ——
    const auto countGE = [&](const int* counter, int want) {
        std::lock_guard<std::mutex> lock(mutex);
        return *counter >= want;
    };

    // 建窗：标题含 token → opened（payload 构建 + EventHub 广播 + 回调）；
    // 翻标题（本连接写属性 + XSync，server 态对 facade 连接立即可见）→ changed；
    // 第二窗一轮 opened/closed。至此抛异常回调至少两拍，两条 catch 均已走过。
    const std::string title = "Wingman SW Probe " + token;
    {
        TestX11Window win(10, 10, 120, 80, title.c_str());
        ASSERT_TRUE(win.valid());
        EXPECT_TRUE(waitFor([&] { return countGE(&openedCount, 1); }, 10000))
            << "opened not delivered to script callback";

        const std::string newTitle = "Wingman SW Renamed " + token;
        XChangeProperty(d, static_cast<Window>(win.handle()),
                        XInternAtom(d, "_NET_WM_NAME", False),
                        XInternAtom(d, "UTF8_STRING", False), 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(newTitle.c_str()),
                        static_cast<int>(newTitle.size()));
        XSync(d, False);
        EXPECT_TRUE(waitFor([&] { return countGE(&changedCount, 1); }, 10000))
            << "changed not delivered to script callback";

        const std::string title2 = "Wingman SW Second " + token;
        {
            TestX11Window win2(10, 100, 120, 80, title2.c_str());
            ASSERT_TRUE(win2.valid());
            EXPECT_TRUE(waitFor([&] { return countGE(&openedCount, 2); }, 10000))
                << "second opened not delivered";
        } // win2 析构销毁 → closed
        EXPECT_TRUE(waitFor([&] { return countGE(&closedCount, 1); }, 10000))
            << "closed (win2) not delivered";
    } // win 析构销毁（根属性清理）→ closed
    EXPECT_TRUE(waitFor([&] { return countGE(&closedCount, 2); }, 10000))
        << "closed (win) not delivered";
    EXPECT_TRUE(waitFor([&] { return throwPhase.load() >= 2; }))
        << "catch paths not exercised";

    XCloseDisplay(d);

    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(idRec)}).asBool(), true);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(idThrow)}).asBool(), true);
    EXPECT_EQ((*unwatch)({ScriptValue::fromInt(idCanary)}).asBool(), true);
    wingman::SystemWatcher::defaultInstance().unwatchAll();
}

#endif // __linux__
