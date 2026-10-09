// filewatcher 脚本模块桥接接真测试：ModuleDescriptor 函数直调（同
// script_modules_test 模式）+ 真实 inotify 目录（原生链路由 filewatcher_test
// 覆盖，本文件验桥接面——门控拒绝 / 回调载荷 / 事件面 / 路径簿记）。
// 回调从 inotify 后端线程异步到达，断言一律正向轮询。
#ifdef __linux__

#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/event.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

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

std::string uniqueTempDir() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = fs::temp_directory_path() /
        ("wingman_fwmod_" + std::to_string(suffix) + "-" +
         std::to_string(::getpid()));
    fs::create_directories(dir);
    return dir.string();
}

template <typename Pred>
bool waitFor(Pred pred, int maxMs = 3000) {
    for (int waited = 0; waited < maxMs; waited += 25) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return pred();
}

} // namespace

TEST(FileWatcherModuleTest, RegisteredInRegistry) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("filewatcher", mod));
    EXPECT_NE(findFunction(mod, "watch"), nullptr);
    EXPECT_NE(findFunction(mod, "unwatch"), nullptr);
    EXPECT_NE(findFunction(mod, "unwatchAll"), nullptr);
    EXPECT_NE(findFunction(mod, "isWatching"), nullptr);
    EXPECT_NE(findFunction(mod, "getWatchedPaths"), nullptr);
}

TEST(FileWatcherModuleTest, NonThreadSafeCallableRejected) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("filewatcher", mod));
    const auto* watch = findFunction(mod, "watch");
    ASSERT_NE(watch, nullptr);

    std::string errorText;
    const uint64_t sub = EventHub::instance().subscribe("filewatcher.error",
        [&](const EventMessage& msg) { errorText = msg.payload.value("error", ""); });
    ASSERT_NE(sub, 0u);

    ScriptValue badCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, false);
    const ScriptValue result = (*watch)({ScriptValue::fromString("/tmp"), badCallable});
    EXPECT_EQ(result.asInt(), 0); // 0 = 注册失败
    EXPECT_FALSE(errorText.empty()); // 门控错误事件已携带文案

    EventHub::instance().unsubscribe(sub);
}

TEST(FileWatcherModuleTest, WatchReceivesChangeAndEvent) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("filewatcher", mod));
    const auto* watch = findFunction(mod, "watch");
    const auto* unwatch = findFunction(mod, "unwatch");
    ASSERT_NE(watch, nullptr);
    ASSERT_NE(unwatch, nullptr);

    const std::string dir = uniqueTempDir();

    std::mutex mutex;
    std::vector<std::string> actions; // 回调载荷 type 序列（create+modify 同拍到达）
    std::string lastEventPath;         // filewatcher.changed 事件载荷 path
    int changedEvents = 0;
    const uint64_t evtSub = EventHub::instance().subscribe("filewatcher.changed",
        [&](const EventMessage& msg) {
            std::lock_guard<std::mutex> lock(mutex);
            lastEventPath = msg.payload.value("path", "");
            ++changedEvents;
        });
    ASSERT_NE(evtSub, 0u);

    ScriptValue okCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>& args) -> ScriptValue {
            if (!args.empty() && args[0].isObject()) {
                const auto* type = args[0].get("type");
                std::lock_guard<std::mutex> lock(mutex);
                if (type) actions.push_back(type->asString());
            }
            return ScriptValue::null();
        }, true);
    const ScriptValue reg = (*watch)({ScriptValue::fromString(dir), okCallable});
    ASSERT_GT(reg.asInt(), 0);

    // 写文件触发 inotify 事件（新建 = added）
    const std::string file = (fs::path(dir) / "hello.txt").string();
    {
        std::ofstream(file) << "content";
    }

    const bool got = waitFor([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return !actions.empty();
    });
    EXPECT_TRUE(got) << "script callback not invoked for file creation";
    {
        std::lock_guard<std::mutex> lock(mutex);
        // 写文件触发 create+modify 两拍：断言 added 在序列中出现过
        // （末拍竞争会让最后一个 type 是 modified，不能只断言末元素）
        bool hasAdded = false;
        for (const auto& a : actions) {
            if (a == "added") hasAdded = true;
        }
        EXPECT_TRUE(hasAdded);
        EXPECT_EQ(lastEventPath, file); // 事件面与回调同源
        EXPECT_GT(changedEvents, 0);
    }

    EXPECT_EQ((*unwatch)({ScriptValue::fromString(dir)}).asBool(), true);

    EventHub::instance().unsubscribe(evtSub);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST(FileWatcherModuleTest, PathBookkeepingQueries) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("filewatcher", mod));
    const auto* watch = findFunction(mod, "watch");
    const auto* unwatch = findFunction(mod, "unwatch");
    const auto* unwatchAll = findFunction(mod, "unwatchAll");
    const auto* isWatching = findFunction(mod, "isWatching");
    const auto* getWatchedPaths = findFunction(mod, "getWatchedPaths");
    ASSERT_NE(watch, nullptr);
    ASSERT_NE(unwatch, nullptr);
    ASSERT_NE(unwatchAll, nullptr);
    ASSERT_NE(isWatching, nullptr);
    ASSERT_NE(getWatchedPaths, nullptr);

    // 开工清场（其他用例可能残留）
    (*unwatchAll)({});
    ScriptValue okCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, true);

    const std::string dirA = uniqueTempDir();
    const std::string dirB = uniqueTempDir();

    const ScriptValue regA = (*watch)({ScriptValue::fromString(dirA), okCallable});
    const ScriptValue regB = (*watch)({ScriptValue::fromString(dirB), okCallable, ScriptValue::fromBool(false)});
    ASSERT_GT(regA.asInt(), 0);
    ASSERT_GT(regB.asInt(), 0);

    EXPECT_EQ((*isWatching)({ScriptValue::fromString(dirA)}).asBool(), true);
    EXPECT_EQ((*isWatching)({ScriptValue::fromString("/no-such-path-xyz")}).asBool(), false);
    // 非 bool 的 recursive 按缺省（true）处理，不拒绝（重复注册同路径 = 替换旧观察）
    const ScriptValue regBad = (*watch)({ScriptValue::fromString(dirA), okCallable, ScriptValue::fromString("x")});
    EXPECT_GT(regBad.asInt(), 0);

    // 路径簿记反映到查询面
    bool hasA = false, hasB = false;
    {
        const ScriptValue paths = (*getWatchedPaths)({});
        ASSERT_TRUE(paths.isArray());
        for (const auto& p : paths.arrayVal) {
            if (p.asString() == dirA) hasA = true;
            if (p.asString() == dirB) hasB = true;
        }
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);

    // 参数校验：path 非 string / callback 非 callable
    EXPECT_EQ((*watch)({ScriptValue::fromInt(1), okCallable}).asInt(), 0);
    EXPECT_EQ((*watch)({ScriptValue::fromString(dirA)}).asInt(), 0);

    EXPECT_EQ((*unwatch)({ScriptValue::fromString(dirA)}).asBool(), true);
    EXPECT_EQ((*isWatching)({ScriptValue::fromString(dirA)}).asBool(), false);
    EXPECT_EQ((*unwatch)({ScriptValue::fromString(dirA)}).asBool(), false); // 重复注销

    (*unwatchAll)({});
    EXPECT_EQ((*isWatching)({ScriptValue::fromString(dirB)}).asBool(), false);

    std::error_code ec;
    fs::remove_all(dirA, ec);
    fs::remove_all(dirB, ec);
}

#endif // __linux__
