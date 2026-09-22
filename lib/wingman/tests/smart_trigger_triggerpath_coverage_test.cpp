// smart_trigger.cpp 触发路径补测（2026-09-22 覆盖率第六批）：
// 现有 smart_trigger_test.cpp 全部用例的条件都不满足，checkConditions 各 case
// 体只被 start→stop 快速穿行，executeActions 七种动作、watchLoop 触发段
// （triggerCount++ / maxTriggers / STOP fast-exit）为纯零覆盖。
// 本文件用"恒真条件"驱动真实触发链：
//   - IMAGE_NOT_FOUND + 不存在模板文件   → findImage.found=false → conditionMet=true
//   - TEXT_NOT_FOUND（OCR 无数据 success=false → !success=true）
// 恒假条件（OCR_CONTAINS 指定不可达文本 / EDGE_DETECTED 空 region /
// COLOR_CHANGED 静态画面自比较）用于覆盖 case 体且保持不触发。
// 动作副作用经第二构造注入 platform::mock::MockInput 断言，不触真实输入。
#include <gtest/gtest.h>

#include "wingman/smart_trigger.hpp"
#include "wingman/platform/mock_input.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace wingman;

namespace {

// 轮询等待谓词成立（默认上限 2s，检查间隔 5ms）
bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

TriggerCondition cond(TriggerConditionType type) {
    TriggerCondition c;
    c.type = type;
    return c;
}

TriggerAction act(TriggerActionType type) {
    TriggerAction a;
    a.type = type;
    return a;
}

} // namespace

// ========== start 二次调用：已运行返回 false（42-43 行）==========

TEST(SmartTriggerTriggerPathTest, StartAlreadyRunningReturnsFalse) {
    SmartTrigger t("tp_already_running");
    // OCR_CONTAINS 指定不可达文本：OCR 无数据 success=false → 恒不满足 → 保持 running
    TriggerCondition c = cond(TriggerConditionType::OCR_CONTAINS);
    c.targetText = "ZWINGMAN_UNREACHABLE_TEXT_Z";
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG));
    t.setCheckInterval(40);

    ASSERT_TRUE(t.start());
    // 等 watchLoop 至少跑过一轮（首检 + 一次 interval 睡眠）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(t.isRunning());
    EXPECT_EQ(t.getTriggerCount(), 0);
    EXPECT_FALSE(t.start()); // 已运行 → false
    t.stop();
    EXPECT_FALSE(t.isRunning());
}

// ========== IMAGE_NOT_FOUND 恒真：触发至 maxTriggers 自停（192-209 行）==========

TEST(SmartTriggerTriggerPathTest, ImageNotFoundTriggersUpToMaxThenSelfStops) {
    SmartTrigger t("tp_img_not_found_max");
    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);
    TriggerAction log = act(TriggerActionType::LOG);
    log.logMessage = "triggered";
    t.addAction(log);
    t.setCheckInterval(10);
    t.setMaxTriggers(2);

    ASSERT_TRUE(t.start());
    // 触发 2 次后 watchLoop 自行置 running=false 并 break
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 2);
    t.stop(); // 线程已退出，stop 仅收尾
}

// ========== 全动作类型 + STOP fast-exit（145-189 / 192-196 行）==========

TEST(SmartTriggerTriggerPathTest, StopActionDrivesAllActionTypesAndFastExits) {
    auto mock = std::make_shared<platform::mock::MockInput>();
    SmartTrigger t("tp_all_actions", mock);

    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);

    std::atomic<int> callbackCount{0};

    TriggerAction click = act(TriggerActionType::CLICK);
    click.clickPosition = Point{11, 22};
    TriggerAction key = act(TriggerActionType::KEY_PRESS);
    key.keyCode = 65; // 'A'
    TriggerAction wait = act(TriggerActionType::WAIT);
    wait.waitMs = 1;
    TriggerAction lua = act(TriggerActionType::LUA_SCRIPT);
    lua.luaScript = "print('unused')";
    TriggerAction cb = act(TriggerActionType::CUSTOM_CALLBACK);
    cb.callback = [&callbackCount] { callbackCount++; };
    TriggerAction log = act(TriggerActionType::LOG);
    log.logMessage = "all-actions";
    // STOP 放最后：触发一轮即全部动作走完，watchLoop fast-exit break
    t.addAction(click);
    t.addAction(key);
    t.addAction(wait);
    t.addAction(lua);
    t.addAction(cb);
    t.addAction(log);
    t.addAction(act(TriggerActionType::STOP));
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    t.stop();

    EXPECT_EQ(t.getTriggerCount(), 1);
    EXPECT_EQ(callbackCount.load(), 1);
    // MockInput 副作用：CLICK → mouseMove+click；KEY_PRESS → keyPress
    EXPECT_GE(mock->getMouseMoveCallCount(), 1);
    EXPECT_GE(mock->getClickCallCount(platform::MouseButton::Left), 1);
    EXPECT_GE(mock->getKeyPressCallCount(static_cast<platform::KeyCode>(65)), 1);
}

// ========== CUSTOM_CALLBACK 无回调体：if 分支跳过不崩溃（183-185 行）==========

TEST(SmartTriggerTriggerPathTest, CustomCallbackWithoutFunctionIsSkipped) {
    SmartTrigger t("tp_cb_absent");
    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);
    t.addAction(act(TriggerActionType::CUSTOM_CALLBACK)); // callback 为空
    t.addAction(act(TriggerActionType::STOP));            // 触发一轮即停
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
}

// ========== 文本/OCR 条件体：不触发路径（85-101 行）==========

TEST(SmartTriggerTriggerPathTest, TextAndOcrConditionBodiesNeverTrigger) {
    struct Case {
        const char* name;
        TriggerConditionType type;
    };
    const Case cases[] = {
        {"tp_text_found", TriggerConditionType::TEXT_FOUND},
        {"tp_ocr_contains", TriggerConditionType::OCR_CONTAINS},
        {"tp_ocr_equals", TriggerConditionType::OCR_EQUALS},
    };
    for (const auto& tc : cases) {
        SmartTrigger t(tc.name);
        TriggerCondition c = cond(tc.type);
        c.targetText = "ZWINGMAN_UNREACHABLE_TEXT_Z"; // 无论 OCR 是否可用都不匹配
        c.searchRegion = Rect{0, 0, 8, 8};
        t.addCondition(c);
        t.addAction(act(TriggerActionType::LOG));
        t.setCheckInterval(10);
        t.setMaxTriggers(1);

        ASSERT_TRUE(t.start()) << tc.name;
        std::this_thread::sleep_for(std::chrono::milliseconds(120)); // 覆盖多轮 case 体
        EXPECT_EQ(t.getTriggerCount(), 0) << tc.name;
        EXPECT_TRUE(t.isRunning()) << tc.name;
        t.stop();
        EXPECT_FALSE(t.isRunning()) << tc.name;
    }
}

// ========== TEXT_NOT_FOUND 恒真：OCR 无数据即触发（90-93 行体 + 触发段）==========

TEST(SmartTriggerTriggerPathTest, TextNotFoundTriggersImmediately) {
    SmartTrigger t("tp_text_not_found");
    TriggerCondition c = cond(TriggerConditionType::TEXT_NOT_FOUND);
    c.targetText = "anything";
    c.searchRegion = Rect{0, 0, 8, 8};
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG));
    t.addAction(act(TriggerActionType::STOP));
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
}

// ========== EDGE_DETECTED / COLOR_CHANGED 条件体循环执行（111-133 行）==========

TEST(SmartTriggerTriggerPathTest, EdgeAndColorChangedBodiesCycleWithoutTrigger) {
    // EDGE_DETECTED：空 region → 无边缘 → 不触发
    SmartTrigger edge("tp_edge");
    TriggerCondition ec = cond(TriggerConditionType::EDGE_DETECTED);
    ec.searchRegion = Rect{0, 0, 4, 4};
    edge.addCondition(ec);
    edge.addAction(act(TriggerActionType::LOG));
    edge.setCheckInterval(10);
    edge.setMaxTriggers(1);
    ASSERT_TRUE(edge.start());

    // COLOR_CHANGED：静态画面首轮记录 previousColor，次轮自比较 match → 不触发；
    // 覆盖 hasPreviousColor=false 首轮与 true 次轮两条路径（116-133 行）
    SmartTrigger colorChanged("tp_color_changed");
    colorChanged.addCondition(cond(TriggerConditionType::COLOR_CHANGED));
    colorChanged.addAction(act(TriggerActionType::LOG));
    colorChanged.setCheckInterval(10);
    colorChanged.setMaxTriggers(1);
    ASSERT_TRUE(colorChanged.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(edge.getTriggerCount(), 0);
    EXPECT_EQ(colorChanged.getTriggerCount(), 0);
    edge.stop();
    colorChanged.stop();
    EXPECT_FALSE(edge.isRunning());
    EXPECT_FALSE(colorChanged.isRunning());
}

// ========== 触发后 maxTriggers=1 且动作含 WAIT：间隔下不超触发 ==========

TEST(SmartTriggerTriggerPathTest, MaxTriggersOneCountsExactlyOnce) {
    SmartTrigger t("tp_max_one_wait");
    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);
    TriggerAction w = act(TriggerActionType::WAIT);
    w.waitMs = 20;
    t.addAction(w);
    t.addAction(act(TriggerActionType::LOG));
    t.setCheckInterval(5);
    t.setMaxTriggers(1);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
    // 自停后再等待也不会继续累计
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_EQ(t.getTriggerCount(), 1);
}

// ========== COLOR_NOT_FOUND 恒真：空屏找不到目标色（85-88 行体）==========

TEST(SmartTriggerTriggerPathTest, ColorNotFoundTriggersImmediately) {
    SmartTrigger t("tp_color_not_found");
    TriggerCondition c = cond(TriggerConditionType::COLOR_NOT_FOUND);
    c.targetColor = Color{255, 0, 0}; // Xvfb 静态底色不含纯红 → 找不到 → conditionMet=true
    c.searchRegion = Rect{0, 0, 32, 32};
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG));
    t.addAction(act(TriggerActionType::STOP));
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
}

// ========== IMAGE_FOUND 不匹配路径：case 体执行但不触发（90-93 行体）==========

TEST(SmartTriggerTriggerPathTest, ImageFoundConditionBodyExecutesWithoutMatch) {
    SmartTrigger t("tp_img_found_nomatch");
    TriggerCondition c = cond(TriggerConditionType::IMAGE_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    c.searchRegion = Rect{0, 0, 32, 32};
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG));
    t.setCheckInterval(10);
    t.setMaxTriggers(1);

    ASSERT_TRUE(t.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(120)); // 多轮 case 体
    EXPECT_EQ(t.getTriggerCount(), 0);
    EXPECT_TRUE(t.isRunning());
    t.stop();
    EXPECT_FALSE(t.isRunning());
}

// ========== 默认输入注入分支：不传 MockInput 触发（149-151 行体）==========
// 仅 LOG/STOP 动作，避免 defaultSharedInput 被真实驱动（无点击/按键副作用）。

TEST(SmartTriggerTriggerPathTest, DefaultInputInjectionBranchExecutes) {
    SmartTrigger t("tp_default_input"); // 第二构造不用——input_ 初始为空
    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG));
    t.addAction(act(TriggerActionType::STOP));
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
}

// ========== 显式空 input：executeActions 默认注入分支（149-151 行体）==========
// 第一构造委托第二构造并预注入 defaultSharedInput，input_ 恒非空；
// 仅显式传 nullptr 时 executeActions 的 if (!input_) 才走默认注入。

TEST(SmartTriggerTriggerPathTest, NullInputTriggersDefaultInjection) {
    SmartTrigger t("tp_null_input", nullptr);
    TriggerCondition c = cond(TriggerConditionType::IMAGE_NOT_FOUND);
    c.templatePath = "/nonexistent/wingman_tp_template.png";
    t.addCondition(c);
    t.addAction(act(TriggerActionType::LOG)); // 不碰 input 的动作，避免真实点击
    t.addAction(act(TriggerActionType::STOP));
    t.setCheckInterval(10);

    ASSERT_TRUE(t.start());
    EXPECT_TRUE(waitUntil([&] { return !t.isRunning(); }));
    EXPECT_EQ(t.getTriggerCount(), 1);
}
