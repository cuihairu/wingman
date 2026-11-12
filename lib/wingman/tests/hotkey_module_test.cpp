// hotkey 脚本模块桥接接真测试：ModuleDescriptor 函数直调（同
// script_modules_test 模式）。参数校验/线程安全门控/注册注销簿记全平台一致
// （Windows CI 是 codecov 唯一数据源，本文件无条件编译，平台差异收进文件内
// 分支——同 system_watcher_test.cpp 先例）；回调体触发在 Linux 走
// defaultSharedInput（XTEST 合成按键）驱动 defaultInstance 轮询线程：
// comboDown 经 XQueryKeymap 读键态，XTEST 注入对轮询可见。无 X 环境
// GTEST_SKIP。2026-11-12 覆盖率短板推进（hotkey_module 24%→）。
#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/event.hpp"
#include "wingman/hotkey.hpp"

#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <atomic>
#include <chrono>

#include "wingman/platform/input_factory.hpp"
#endif

using wingman::script::ModuleDescriptor;
using wingman::script::ScriptValue;
using wingman::EventHub;
using wingman::EventMessage;

namespace {

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

ScriptValue nullCallable(const std::vector<ScriptValue>&) {
    return ScriptValue::null();
}

#ifdef __linux__
template <typename Pred>
bool waitFor(Pred pred, int maxMs = 3000) {
    for (int waited = 0; waited < maxMs; waited += 25) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return pred();
}
#endif

} // namespace

TEST(HotkeyModuleTest, RegisteredInRegistry) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("hotkey", mod));
    EXPECT_NE(findFunction(mod, "register"), nullptr);
    EXPECT_NE(findFunction(mod, "unregister"), nullptr);
}

TEST(HotkeyModuleTest, RegisterArgumentValidation) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("hotkey", mod));
    const auto* reg = findFunction(mod, "register");
    ASSERT_NE(reg, nullptr);

    // 空参 / combo 非 string / callback 非 callable → 0（0 = 注册失败）
    EXPECT_EQ((*reg)({}).asInt(), 0);
    EXPECT_EQ((*reg)({ScriptValue::fromInt(5), ScriptValue::fromCallable(nullCallable, true)}).asInt(), 0);
    EXPECT_EQ((*reg)({ScriptValue::fromString("Ctrl+P"), ScriptValue::fromInt(1)}).asInt(), 0);
}

TEST(HotkeyModuleTest, NonThreadSafeCallableRejected) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("hotkey", mod));
    const auto* reg = findFunction(mod, "register");
    ASSERT_NE(reg, nullptr);

    // 清场后订阅错误事件验证门控拒绝（回调从后台轮询线程触发）
    wingman::HotkeyManager::defaultInstance().stop();
    std::string errorText;
    const uint64_t sub = EventHub::instance().subscribe("hotkey.error",
        [&](const EventMessage& msg) { errorText = msg.payload.value("error", ""); });
    ASSERT_NE(sub, 0u);

    ScriptValue badCallable = ScriptValue::fromCallable(
        [](const std::vector<ScriptValue>&) { return ScriptValue::null(); }, false);
    const ScriptValue result = (*reg)({ScriptValue::fromString("Ctrl+P"), badCallable});
    EXPECT_EQ(result.asInt(), 0);
    EXPECT_FALSE(errorText.empty()); // 门控错误事件已携带文案

    EventHub::instance().unsubscribe(sub);
}

TEST(HotkeyModuleTest, RegisterUnregisterBookkeeping) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("hotkey", mod));
    const auto* reg = findFunction(mod, "register");
    const auto* unreg = findFunction(mod, "unregister");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(unreg, nullptr);

    wingman::HotkeyManager::defaultInstance().stop(); // 清场：簿记从零起算

    // 非法组合文本：manager 解析失败返回 0，模块原样透传
    EXPECT_EQ((*reg)({ScriptValue::fromString("Ctrl+ nonsense"), ScriptValue::fromCallable(nullCallable, true)}).asInt(), 0);

    // 合法组合：登记成功 → 注销 true → 重复注销 false
    const int64_t id = (*reg)({ScriptValue::fromString("Ctrl+F9"), ScriptValue::fromCallable(nullCallable, true)}).asInt();
    ASSERT_GT(id, 0);
    EXPECT_EQ(wingman::HotkeyManager::defaultInstance().count(), 1u);

    // unregister 校验：缺参 / 非整数 → false
    EXPECT_EQ((*unreg)({}).asBool(), false);
    EXPECT_EQ((*unreg)({ScriptValue::fromString("x")}).asBool(), false);

    EXPECT_EQ((*unreg)({ScriptValue::fromInt(id)}).asBool(), true);
    EXPECT_EQ((*unreg)({ScriptValue::fromInt(id)}).asBool(), false);
    EXPECT_EQ(wingman::HotkeyManager::defaultInstance().count(), 0u);
    wingman::HotkeyManager::defaultInstance().stop();
}

#ifdef __linux__

// 回调体真实触发：XTEST 合成按键 → defaultInstance 轮询（30ms）→ 上升沿
// 触发。正常回调验 callback({}) 通路；抛异常回调两拍分别走
// std::exception 与 ... 两条 catch。
TEST(HotkeyModuleTest, ComboPressFiresCallbackAndErrorPaths) {
    ModuleDescriptor mod;
    ASSERT_TRUE(findModule("hotkey", mod));
    const auto* reg = findFunction(mod, "register");
    const auto* unreg = findFunction(mod, "unregister");
    ASSERT_NE(reg, nullptr);
    ASSERT_NE(unreg, nullptr);

    auto input = wingman::platform::defaultSharedInput();
    if (!input || !input->initialize({})) {
        GTEST_SKIP() << "X display unavailable — combo firing needs a live X server";
    }

    wingman::HotkeyManager::defaultInstance().stop(); // 清场后重新登记

    std::mutex mutex;
    int normalFires = 0;
    const ScriptValue normalCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>&) {
            std::lock_guard<std::mutex> lock(mutex);
            ++normalFires;
            return ScriptValue::null();
        }, true);

    // 抛异常回调：第 1 次触发抛 std::exception，第 2 次抛非 std 对象
    std::atomic<int> throwPhase{0};
    const ScriptValue throwingCallable = ScriptValue::fromCallable(
        [&](const std::vector<ScriptValue>&) {
            if (throwPhase.fetch_add(1) == 0) {
                throw std::runtime_error("hotkey probe");
            }
            throw 42;
            return ScriptValue::null(); // 不可达：返回类型须与 CallableFunc 对齐
        }, true);

    const int64_t idNormal = (*reg)({ScriptValue::fromString("Ctrl+Alt+F9"), normalCallable}).asInt();
    const int64_t idThrow = (*reg)({ScriptValue::fromString("Ctrl+Alt+F10"), throwingCallable}).asInt();
    ASSERT_GT(idNormal, 0);
    ASSERT_GT(idThrow, 0);

    using wingman::platform::KeyCode;
    const auto pressCombo = [&](KeyCode key) {
        input->keyDown(KeyCode::Control);
        input->keyDown(KeyCode::Alt);
        input->keyDown(key);
    };
    const auto releaseCombo = [&](KeyCode key) {
        input->keyUp(key);
        input->keyUp(KeyCode::Alt);
        input->keyUp(KeyCode::Control);
    };

    // Ctrl+Alt+F9：正常回调触发（预算放宽：轮询线程在高负载下可能数秒后才被首次调度）
    pressCombo(KeyCode::F9);
    const bool fired = waitFor([&] {
        std::lock_guard<std::mutex> lock(mutex);
        return normalFires > 0;
    }, 15000);
    releaseCombo(KeyCode::F9);
    EXPECT_TRUE(fired) << "normal callback not fired via synthetic combo";

    // Ctrl+Alt+F10 两拍：分别落入两条 catch（模块内吞掉异常，不得崩测试进程）
    pressCombo(KeyCode::F10);
    EXPECT_TRUE(waitFor([&] { return throwPhase.load() >= 1; }, 10000));
    releaseCombo(KeyCode::F10);
    std::this_thread::sleep_for(std::chrono::milliseconds(60)); // 留出下降沿复位
    pressCombo(KeyCode::F10);
    EXPECT_TRUE(waitFor([&] { return throwPhase.load() >= 2; }, 10000));
    releaseCombo(KeyCode::F10);

    EXPECT_EQ((*unreg)({ScriptValue::fromInt(idNormal)}).asBool(), true);
    EXPECT_EQ((*unreg)({ScriptValue::fromInt(idThrow)}).asBool(), true);
    wingman::HotkeyManager::defaultInstance().stop();
    // 注意：不得 input->shutdown()——shared input 是进程级单例，关掉会让
    // 依赖其已初始化态的后续用例（MacroRecorder playback 注入）静默空转
}

#endif // __linux__
