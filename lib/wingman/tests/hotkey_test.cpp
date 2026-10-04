#include <gtest/gtest.h>
#include "wingman/hotkey.hpp"
#include "wingman/platform/mock_input.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace wingman;
using platform::KeyCode;
using platform::mock::MockInput;

namespace {
const script::ModuleDescriptor& getHotkeyModule() {
    static std::vector<script::ModuleDescriptor> mods = script::modules::getAllModules();
    for (auto& mod : mods) {
        if (mod.name == "hotkey") return mod;
    }
    return {};
}

// waitFlag 轮询等待条件成立（上限 2s，覆盖多倍轮询间隔的调度抖动）
template <typename Pred>
bool waitFlag(Pred&& pred) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

struct ManagerHarness {
    std::shared_ptr<MockInput> input = std::make_shared<MockInput>();
    HotkeyManager manager{input};

    ManagerHarness() {
        input->initialize({});
        manager.start(10);  // 10ms 轮询，测试快速收敛
    }
    ~ManagerHarness() {
        manager.stop();
        input->shutdown();
    }
};
} // anonymous namespace

// ========== 组合键解析 ==========

TEST(HotkeyParseTest, ValidCombos) {
    HotkeyCombo c;
    ASSERT_TRUE(parseHotkeyCombo("Ctrl+Shift+A", c));
    EXPECT_TRUE(c.ctrl);
    EXPECT_TRUE(c.shift);
    EXPECT_FALSE(c.alt);
    EXPECT_EQ(c.key, KeyCode::A);

    // 大小写不敏感、修饰顺序不限
    ASSERT_TRUE(parseHotkeyCombo("alt+CTRL+f4", c));
    EXPECT_TRUE(c.alt);
    EXPECT_TRUE(c.ctrl);
    EXPECT_EQ(c.key, KeyCode::F4);

    // 裸主键、Control/Escape 别名、数字
    ASSERT_TRUE(parseHotkeyCombo("F5", c));
    EXPECT_EQ(c.key, KeyCode::F5);
    ASSERT_TRUE(parseHotkeyCombo("Control+Return", c));
    EXPECT_TRUE(c.ctrl);
    EXPECT_EQ(c.key, KeyCode::Enter);
    ASSERT_TRUE(parseHotkeyCombo("9", c));
    EXPECT_EQ(c.key, KeyCode::Num9);
    ASSERT_TRUE(parseHotkeyCombo("PgDn", c));
    EXPECT_EQ(c.key, KeyCode::PageDown);
}

TEST(HotkeyParseTest, InvalidCombos) {
    HotkeyCombo c;
    EXPECT_FALSE(parseHotkeyCombo("", c));
    EXPECT_FALSE(parseHotkeyCombo("Ctrl+", c));          // 空段
    EXPECT_FALSE(parseHotkeyCombo("Ctrl+Shift", c));     // 无主键
    EXPECT_FALSE(parseHotkeyCombo("Ctrl+Ctrl+A", c));    // 修饰重复
    EXPECT_FALSE(parseHotkeyCombo("Ctrl+Win+A", c));     // Win/Meta 显式拒绝
    EXPECT_FALSE(parseHotkeyCombo("Ctrl+Foo", c));       // 未知键名
    EXPECT_FALSE(parseHotkeyCombo("A+B", c));            // 双主键
    EXPECT_FALSE(parseHotkeyCombo("++", c));             // 全空段
}

// ========== 注册失败分支 ==========

TEST(HotkeyManagerTest, RegisterRejectsInvalid) {
    ManagerHarness h;
    // 非法组合文本
    EXPECT_EQ(h.manager.registerHotkey("Ctrl+ nonsense", [] {}), 0u);
    // 空回调
    HotkeyCombo combo;
    combo.key = KeyCode::F5;
    EXPECT_EQ(h.manager.registerHotkey(combo, nullptr), 0u);
    EXPECT_EQ(h.manager.count(), 0u);
}

// ========== 触发语义 ==========

TEST(HotkeyManagerTest, RisingEdgeFiresOncePerPress) {
    ManagerHarness h;
    std::atomic<int> fires{0};
    const auto id = h.manager.registerHotkey("Ctrl+Shift+P", [&] { ++fires; });
    ASSERT_NE(id, 0u);

    // 只按主键（缺修饰）不触发
    h.input->keyDown(KeyCode::P);
    EXPECT_FALSE(waitFlag([&] { return fires.load() > 0; }));

    // 补上修饰键 → 上升沿触发一次
    h.input->keyDown(KeyCode::Control);
    h.input->keyDown(KeyCode::Shift);
    EXPECT_TRUE(waitFlag([&] { return fires.load() == 1; }));

    // 按住不重复触发（无重复沿）
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_EQ(fires.load(), 1);

    // 松开 → 无触发（下降沿不触发），计次保持 1
    h.input->keyUp(KeyCode::Control);
    h.input->keyUp(KeyCode::Shift);
    h.input->keyUp(KeyCode::P);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_EQ(fires.load(), 1);

    // 再按 → 第二次触发
    h.input->keyDown(KeyCode::Control);
    h.input->keyDown(KeyCode::Shift);
    h.input->keyDown(KeyCode::P);
    EXPECT_TRUE(waitFlag([&] { return fires.load() == 2; }));
}

TEST(HotkeyManagerTest, UnregisterStopsFiring) {
    ManagerHarness h;
    std::atomic<int> fires{0};
    const auto id = h.manager.registerHotkey("F7", [&] { ++fires; });
    ASSERT_NE(id, 0u);

    h.input->keyDown(KeyCode::F7);
    EXPECT_TRUE(waitFlag([&] { return fires.load() == 1; }));
    h.input->keyUp(KeyCode::F7);

    EXPECT_TRUE(h.manager.unregister(id));
    EXPECT_FALSE(h.manager.unregister(id));  // 重复注销 false
    EXPECT_EQ(h.manager.count(), 0u);

    // 注销后再按不触发（触发前按 ID 复核）
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    h.input->keyDown(KeyCode::F7);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    EXPECT_EQ(fires.load(), 1);
}

TEST(HotkeyManagerTest, CallbackExceptionDoesNotKillWorker) {
    ManagerHarness h;
    std::atomic<int> fires{0};
    // 首个回调抛异常
    h.manager.registerHotkey("F8", [] { throw std::runtime_error("boom"); });
    // 正常注册仍应触发 → worker 存活
    const auto id = h.manager.registerHotkey("F9", [&] { ++fires; });

    h.input->keyDown(KeyCode::F8);
    h.input->keyDown(KeyCode::F9);
    EXPECT_TRUE(waitFlag([&] { return fires.load() == 1; }));
}

TEST(HotkeyManagerTest, AutoRestartAfterIdle) {
    ManagerHarness h;
    std::atomic<int> fires{0};
    const auto id = h.manager.registerHotkey("F6", [&] { ++fires; });
    ASSERT_NE(id, 0u);
    ASSERT_TRUE(h.manager.isRunning());

    // 注销清空 → worker 自查退出（留两个轮询周期余量）
    EXPECT_TRUE(h.manager.unregister(id));
    EXPECT_TRUE(waitFlag([&] { return !h.manager.isRunning(); }));

    // 重新注册 → 自动重启并触发
    const auto id2 = h.manager.registerHotkey("F6", [&] { ++fires; });
    ASSERT_NE(id2, 0u);
    EXPECT_TRUE(h.manager.isRunning());
    h.input->keyDown(KeyCode::F6);
    EXPECT_TRUE(waitFlag([&] { return fires.load() == 1; }));
}

TEST(HotkeyManagerTest, StopAndRestartExplicit) {
    ManagerHarness h;
    (void)h.manager.registerHotkey("F5", [] {});
    EXPECT_TRUE(h.manager.isRunning());
    h.manager.stop();
    EXPECT_FALSE(h.manager.isRunning());
    h.manager.start(10);
    EXPECT_TRUE(h.manager.isRunning());
    // 重复 stop 无害
    h.manager.stop();
    EXPECT_FALSE(h.manager.isRunning());
}

// ========== 模块注册 ==========

TEST(HotkeyModuleTest, RegisteredInAllModules) {
    const auto& mod = getHotkeyModule();
    ASSERT_FALSE(mod.name.empty());
    bool hasRegister = false;
    bool hasUnregister = false;
    for (const auto& f : mod.functions) {
        if (f.name == "register") hasRegister = true;
        if (f.name == "unregister") hasUnregister = true;
    }
    EXPECT_TRUE(hasRegister);
    EXPECT_TRUE(hasUnregister);
}
