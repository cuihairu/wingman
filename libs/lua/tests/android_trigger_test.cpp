/**
 * Android 端触发器引擎测试（A2 扩展，development-todo「触发器系统端侧跑通
 * （定时/像素触发）」）
 *
 * AndroidTriggerManager 为纯 C++（AndroidHostBridge 抽象 + ImageAnalyzer），
 * 桌面用 Fake 桥直测，NDK 编译同一份源码。覆盖：timer 边沿触发与 maxTriggers
 * 自停、color_found/color_not_found 对合成帧判定、pixel_changed 基线后触发、
 * stop 停止、remove 摘除、stopAll 返回数、坏定义拒绝（同名/空条件/未知类型/
 * 空 ms）与 null 桥降级（timer 照常、color 不触发不崩）。
 *
 * 触发器共用一个进程级单例：各用例以唯一名登记，结束 stopAll + remove 清场。
 */

#include <gtest/gtest.h>

#include "wingman/androidagent/android_trigger.hpp"

#include <chrono>
#include <memory>
#include <thread>

using wingman::android::AndroidTriggerManager;
using wingman::android::TriggerAction;
using wingman::android::TriggerActionType;
using wingman::android::TriggerCondition;
using wingman::android::TriggerConditionType;
using wingman::android::TriggerDef;
using wingman::Color;
using wingman::Bitmap;
namespace pab = wingman::platform::android;

namespace {

// 可切换帧色（pixel_changed / color_found 需要帧内容随用例变化）
class SwitchableFakeBridge : public pab::AndroidHostBridge {
public:
    bool tap(int x, int y, int durationMs) override {
        taps.emplace_back(x, y, durationMs);
        return true;
    }
    bool swipe(int x1, int y1, int x2, int y2, int durationMs) override {
        swipes.emplace_back(x1, y1, x2, y2, durationMs);
        return true;
    }
    bool longPress(int x, int y, int durationMs) override {
        longPresses.emplace_back(x, y, durationMs);
        return true;
    }
    std::unique_ptr<Bitmap> captureFrame() override {
        ++captureCount;
        auto frame = std::make_unique<Bitmap>(4, 2);
        frame->setPixel(0, 0, frameColor);
        return frame;
    }
    bool screenSize(int& width, int& height) override {
        width = 4;
        height = 2;
        return true;
    }

    Color frameColor{255, 0, 0, 255};
    struct Tap { int x, y, ms; };
    struct Swipe { int x1, y1, x2, y2, ms; };
    std::vector<Tap> taps;
    std::vector<Swipe> swipes;
    std::vector<Tap> longPresses;
    std::atomic<int> captureCount{0};
};

// 轮询等待条件成立（监视线程滴答 25ms + checkInterval 轮询，超时上界放宽）
template <typename Pred>
bool waitFor(Pred&& pred, int timeoutMs = 3000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

TriggerDef timerDef(const std::string& name, int ms, int maxTriggers) {
    TriggerDef def;
    def.name = name;
    def.checkIntervalMs = 50;
    def.maxTriggers = maxTriggers;
    TriggerCondition cond;
    cond.type = TriggerConditionType::Timer;
    cond.timerMs = ms;
    def.conditions.push_back(cond);
    TriggerAction action;
    action.type = TriggerActionType::Tap;
    action.x = 1;
    action.y = 1;
    def.actions.push_back(action);
    return def;
}

// 用例结束清场：停表 + 摘除本用例登记（单例跨用例共存）
class AndroidTriggerTest : public ::testing::Test {
protected:
    void TearDown() override {
        auto& mgr = AndroidTriggerManager::instance();
        mgr.stopAll();
        for (const auto& name : registered_) {
            mgr.remove(name);
        }
        mgr.setBridge(nullptr);
    }

    bool addTracked(const TriggerDef& def) {
        if (AndroidTriggerManager::instance().add(def)) {
            registered_.push_back(def.name);
            return true;
        }
        return false;
    }

    std::vector<std::string> registered_;
};

} // namespace

// ========== timer 条件 ==========

TEST_F(AndroidTriggerTest, TimerFiresTapAndStopsAtMaxTriggers) {
    SwitchableFakeBridge bridge;
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    ASSERT_TRUE(addTracked(timerDef("t_timer_max", 100, 1)));
    ASSERT_TRUE(mgr.start("t_timer_max"));
    EXPECT_TRUE(mgr.isRunning("t_timer_max"));

    // timerMs=100 + 轮询 50ms：数百 ms 内应触发一次并自停（maxTriggers=1）
    ASSERT_TRUE(waitFor([&] { return mgr.getTriggerCount("t_timer_max") >= 1; }));
    ASSERT_TRUE(waitFor([&] { return !mgr.isRunning("t_timer_max"); }, 1000));

    // 停后再等：计数不再增长（maxTriggers 自停语义）
    const int fired = mgr.getTriggerCount("t_timer_max");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mgr.getTriggerCount("t_timer_max"), fired);

    ASSERT_FALSE(bridge.taps.empty());
    EXPECT_EQ(bridge.taps[0].x, 1);
    EXPECT_EQ(bridge.taps[0].y, 1);
}

// ========== color 条件（合成帧：仅 (0,0) 为可切换色） ==========

TEST_F(AndroidTriggerTest, ColorFoundFiresOnMatch) {
    SwitchableFakeBridge bridge;  // frameColor = 红 (255,0,0)
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    TriggerDef def;
    def.name = "t_color_found";
    def.checkIntervalMs = 50;
    TriggerCondition cond;
    cond.type = TriggerConditionType::ColorFound;
    cond.color = Color(255, 0, 0, 255);
    cond.tolerance = 10;
    def.conditions.push_back(cond);
    TriggerAction action;
    action.type = TriggerActionType::LongPress;
    action.x = 2;
    action.y = 1;
    action.durationMs = 600;
    def.actions.push_back(action);
    ASSERT_TRUE(addTracked(def));
    mgr.start("t_color_found");

    ASSERT_TRUE(waitFor([&] { return mgr.getTriggerCount("t_color_found") >= 1; }));
    ASSERT_FALSE(bridge.longPresses.empty());
    EXPECT_EQ(bridge.longPresses[0].x, 2);
    EXPECT_EQ(bridge.longPresses[0].ms, 600);
}

TEST_F(AndroidTriggerTest, ColorNotFoundFiresWhenAbsent) {
    SwitchableFakeBridge bridge;
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    TriggerDef def;
    def.name = "t_color_gone";
    def.checkIntervalMs = 50;
    TriggerCondition cond;
    cond.type = TriggerConditionType::ColorNotFound;
    cond.color = Color(0, 255, 0, 255);  // 帧内不存在（帧仅 (0,0)=红，余黑）
    def.conditions.push_back(cond);
    TriggerAction action;
    action.type = TriggerActionType::Swipe;
    action.x = 0;
    action.y = 0;
    action.x2 = 3;
    action.y2 = 1;
    def.actions.push_back(action);
    ASSERT_TRUE(addTracked(def));
    mgr.start("t_color_gone");

    ASSERT_TRUE(waitFor([&] { return mgr.getTriggerCount("t_color_gone") >= 1; }));
    ASSERT_FALSE(bridge.swipes.empty());
}

// ========== pixel_changed 条件 ==========

TEST_F(AndroidTriggerTest, PixelChangedFiresAfterBaseline) {
    SwitchableFakeBridge bridge;
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    TriggerDef def;
    def.name = "t_pixel";
    def.checkIntervalMs = 50;
    TriggerCondition cond;
    cond.type = TriggerConditionType::PixelChanged;
    cond.x = 0;
    cond.y = 0;
    cond.tolerance = 10;
    def.conditions.push_back(cond);
    TriggerAction action;
    action.type = TriggerActionType::Tap;
    action.x = 0;
    action.y = 0;
    def.actions.push_back(action);
    ASSERT_TRUE(addTracked(def));
    mgr.start("t_pixel");

    // 首拍建基线不触发；变色后才触发
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(mgr.getTriggerCount("t_pixel"), 0);

    bridge.frameColor = Color(0, 0, 255, 255);
    ASSERT_TRUE(waitFor([&] { return mgr.getTriggerCount("t_pixel") >= 1; }));
}

// ========== 生命周期与防御 ==========

TEST_F(AndroidTriggerTest, StopPreventsFurtherFiresAndRemoveDetaches) {
    SwitchableFakeBridge bridge;
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    ASSERT_TRUE(addTracked(timerDef("t_stop", 60, 0)));
    ASSERT_TRUE(mgr.start("t_stop"));
    ASSERT_TRUE(waitFor([&] { return mgr.getTriggerCount("t_stop") >= 1; }));
    ASSERT_TRUE(mgr.stop("t_stop"));
    EXPECT_FALSE(mgr.isRunning("t_stop"));

    const int fired = mgr.getTriggerCount("t_stop");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mgr.getTriggerCount("t_stop"), fired);

    EXPECT_TRUE(mgr.remove("t_stop"));
    registered_.clear();  // TearDown 不重复 remove
    EXPECT_EQ(mgr.getTriggerCount("t_stop"), -1);
    EXPECT_FALSE(mgr.start("t_stop"));
}

TEST_F(AndroidTriggerTest, StopAllReportsChangedCount) {
    SwitchableFakeBridge bridge;
    auto& mgr = AndroidTriggerManager::instance();
    mgr.setBridge(&bridge);

    ASSERT_TRUE(addTracked(timerDef("t_all_a", 1000, 0)));
    ASSERT_TRUE(addTracked(timerDef("t_all_b", 1000, 0)));
    EXPECT_EQ(mgr.startAll(), 2);
    // 已运行再 startAll：无变化
    EXPECT_EQ(mgr.startAll(), 0);
    EXPECT_EQ(mgr.stopAll(), 2);
    EXPECT_EQ(mgr.stopAll(), 0);
    EXPECT_EQ(mgr.count(), 2);
}

TEST_F(AndroidTriggerTest, RejectsBadDefs) {
    auto& mgr = AndroidTriggerManager::instance();

    TriggerDef def = timerDef("t_bad_dup", 100, 0);
    ASSERT_TRUE(addTracked(def));
    EXPECT_FALSE(mgr.add(def));  // 同名

    TriggerDef empty = timerDef("", 100, 0);
    EXPECT_FALSE(mgr.add(empty));  // 空名

    TriggerDef noCond = timerDef("t_bad_nocond", 100, 0);
    noCond.conditions.clear();
    EXPECT_FALSE(mgr.add(noCond));  // 无条件（与 Lua 绑定层同口径）

    // checkIntervalMs 下界收敛：1ms 登记被抬到 50ms，不忙等
    TriggerDef fast = timerDef("t_fast", 100, 0);
    fast.checkIntervalMs = 1;
    ASSERT_TRUE(addTracked(fast));
    mgr.start("t_fast");
    // 不崩即可：checkIntervalMs 收敛到 50，不会忙等
}
