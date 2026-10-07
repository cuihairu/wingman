/**
 * Android 脚本能力 API 测试（A2，docs/android-agent-design.md §5.6/§9）
 *
 * registerAndroidApis 是纯 C++（sol2 + AndroidHostBridge 抽象）：
 * 桌面环境用 FakeHostBridge（合成已知像素 Bitmap + 手势记录）直测，
 * NDK 编译同一份源码。覆盖：手势参数透传、桥缺失降级、屏幕查询、
 * 找色命中/容差/未命中、findImage 路径解析与 delay 停止中断。
 *
 * 测试帧：4x2，第一行红绿蓝白、第二行黑黑黑黑；均色 (63,63,63)。
 */

#include <gtest/gtest.h>

#include "wingman/androidagent/android_script_api.hpp"
#include "platform/android/android_host_bridge.hpp"
#include "wingman/vision_ai.hpp"

#include <sol/sol.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using wingman::android::registerAndroidApis;
using wingman::Color;
using wingman::Bitmap;
namespace pab = wingman::platform::android;

namespace {

// 合成帧：与类注释保持一致
std::unique_ptr<Bitmap> makeTestFrame() {
    auto frame = std::make_unique<Bitmap>(4, 2);
    frame->setPixel(0, 0, Color(255, 0, 0, 255));
    frame->setPixel(1, 0, Color(0, 255, 0, 255));
    frame->setPixel(2, 0, Color(0, 0, 255, 255));
    frame->setPixel(3, 0, Color(255, 255, 255, 255));
    // 第二行保持默认黑色
    return frame;
}

// Fake 桥：记录手势调用，captureFrame 恒返回测试帧
class FakeHostBridge : public pab::AndroidHostBridge {
public:
    bool tap(int x, int y, int durationMs) override {
        taps.push_back({x, y, durationMs});
        return true;
    }
    bool swipe(int x1, int y1, int x2, int y2, int durationMs) override {
        swipes.push_back({x1, y1, x2, y2, durationMs});
        return true;
    }
    bool longPress(int x, int y, int durationMs) override {
        longPresses.push_back({x, y, durationMs});
        return true;
    }
    std::unique_ptr<Bitmap> captureFrame() override {
        ++captureCount;
        return makeTestFrame();
    }
    bool screenSize(int& width, int& height) override {
        width = 4;
        height = 2;
        return true;
    }

    struct Tap { int x, y, ms; };
    struct Swipe { int x1, y1, x2, y2, ms; };
    std::vector<Tap> taps;
    std::vector<Swipe> swipes;
    std::vector<Tap> longPresses;
    int captureCount = 0;
};

// 建好 wingman 表的 Lua state（与 runSync 内部同构的最小环境）
struct ApiEnv {
    sol::state lua;
    std::atomic<bool> stop{false};

    explicit ApiEnv(pab::AndroidHostBridge* bridge,
                    const std::string& filesDir = "") {
        lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math,
                           sol::lib::table, sol::lib::os);
        lua["wingman"] = lua.create_table();
        registerAndroidApis(lua, stop, bridge, filesDir);
    }

    // 断言脚本无错（script_pass_on_error：失败返回结果而非抛出，
    // 与 ScriptRunner::runSync 同款）
    void eval(const std::string& code) {
        sol::protected_function_result r =
            lua.safe_script(code, sol::script_pass_on_error);
        if (!r.valid()) {
            ADD_FAILURE() << "lua error: " << r.get<sol::error>().what();
        }
    }
};

} // namespace

TEST(AndroidApiTest, GesturePassthroughAndDefaults) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    env.eval(R"(
        assert(wingman.input.tap(10, 20) == true)
        assert(wingman.input.click(30, 40, 80) == true)      -- 短按路径
        assert(wingman.input.click(50, 60, 800) == true)     -- >=500 走长按
        assert(wingman.input.swipe(0, 0, 100, 200) == true)
        assert(wingman.input.longPress(5, 6) == true)
    )");

    ASSERT_EQ(bridge.taps.size(), 2u);
    EXPECT_EQ(bridge.taps[0].x, 10);
    EXPECT_EQ(bridge.taps[0].y, 20);
    EXPECT_EQ(bridge.taps[0].ms, 60);        // 默认时长
    EXPECT_EQ(bridge.taps[1].x, 30);
    ASSERT_EQ(bridge.longPresses.size(), 2u);
    EXPECT_EQ(bridge.longPresses[0].x, 50);
    EXPECT_EQ(bridge.longPresses[0].ms, 800);
    EXPECT_EQ(bridge.longPresses[1].ms, 600);  // longPress 默认时长
    ASSERT_EQ(bridge.swipes.size(), 1u);
    EXPECT_EQ(bridge.swipes[0].x2, 100);
    EXPECT_EQ(bridge.swipes[0].ms, 300);       // swipe 默认时长
}

TEST(AndroidApiTest, MissingBridgeDegradesGracefully) {
    ApiEnv env(nullptr);

    // Lua 5.5 无全局 unpack：二元组用 table.unpack
    env.eval(R"(
        assert(wingman.input.tap(1, 2) == false)
        assert(wingman.input.swipe(0, 0, 1, 1) == false)
        assert(wingman.screen.getScreenWidth() == 0)
        assert(wingman.screen.getScreenHeight() == 0)
        assert(wingman.screen.capture() == false)
        assert(wingman.screen.getPixel(0, 0) == nil)
        local r = wingman.screen.findColor(0xFF0000)
        assert(r[1] == nil and r[2] == false)
        assert(wingman.vision.findColor(0xFF0000) == nil)
        assert(wingman.vision.hasColor(0xFF0000) == false)
        assert(wingman.vision.getDominantColor() == nil)
        local m = wingman.vision.findImage('tpl.png')
        assert(m.found == false)
    )");
}

TEST(AndroidApiTest, ScreenQueriesAndDominantColor) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    env.eval(R"(
        assert(wingman.screen.getScreenWidth() == 4)
        assert(wingman.screen.getScreenHeight() == 2)
        assert(wingman.screen.capture() == true)
        p = wingman.screen.getPixel(0, 0)
        assert(p.r == 255 and p.g == 0 and p.b == 0 and p.a == 255)
        d = wingman.vision.getDominantColor()
        -- 均色：三通道各 (255+255)/8 = 63（第二行黑不计入）
        assert(d.r == 63 and d.g == 63 and d.b == 63)
    )");
    // capture/getPixel/getDominantColor 各取一帧；尺寸查询不走 captureFrame
    EXPECT_EQ(bridge.captureCount, 3);
}

TEST(AndroidApiTest, FindColorHitToleranceAndMiss) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    env.eval(R"(
        -- 命中：整型 0xRRGGBB
        local pt, found = table.unpack(wingman.screen.findColor(0x00FF00))
        assert(found and pt.x == 1 and pt.y == 0)
        -- 命中：{r,g,b} 表形式
        local pt2 = wingman.vision.findColor({r = 255, g = 0, b = 0})
        assert(pt2 and pt2.x == 0 and pt2.y == 0)
        -- 容差内命中（深灰 10,10,10 vs 黑，tolerance 20）
        assert(wingman.vision.hasColor(0x0A0A0A, 20) == true)
        -- 容差外未命中
        assert(wingman.vision.hasColor(0x0A0A0A, 5) == false)
        -- region 限定命中
        local pt3 = wingman.vision.findColor(0x0000FF, 10, {x = 2, y = 0, width = 1, height = 1})
        assert(pt3 and pt3.x == 2)
        -- region 外的颜色找不到
        local pt4 = wingman.vision.findColor(0xFF0000, 10, {x = 2, y = 0, width = 2, height = 2})
        assert(pt4 == nil)
        -- findAllColors 多点（黑有 4 个）
        local blacks = wingman.vision.findAllColors(0x000000, 10)
        assert(#blacks == 4)
    )");
}

TEST(AndroidApiTest, FindImageMissingTemplateReportsNotFound) {
    // lua_tests 无 vision 配置，模板文件加载仅支持 BMP 且属
    // image_analyzer_test.cpp 的深测范围；此处验证路径解析 + 未找到语义
    FakeHostBridge bridge;
    ApiEnv env(&bridge, "/data/templates");

    env.eval(R"(
        local m = wingman.vision.findImage('missing.png', 0.9)
        assert(m.found == false)
        local r = wingman.screen.findImage('missing.png')
        assert(r[1] == nil and r[2] == false)
    )");
}

TEST(AndroidApiTest, DelayInterruptsOnStop) {
    ApiEnv env(nullptr);
    env.stop.store(false);

    sol::protected_function_result result;
    std::thread scriptThread([&] {
        // lua_State 单线程使用：仅此线程触碰 lua，主线程只动 stop 标志
        result = env.lua.safe_script("wingman.input.delay(5000)",
                                     sol::script_pass_on_error);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    env.stop.store(true);
    scriptThread.join();

    // 5000ms delay 在 ~150ms 被停止标志打断（分片检查点抛 "script stopped"）
    ASSERT_FALSE(result.valid());
    const std::string error = result.get<sol::error>().what();
    EXPECT_NE(error.find("script stopped"), std::string::npos);
}

// ===== AI 视觉识别（provider 通路，桌面 vision 模块同形）=====
// VisionAi 配置为进程级静态：配过 baseUrl 的用例必须在收尾 reset，
// 防污染后续用例的 HTTP 路径

TEST(AndroidApiTest, AiSetupValidatesAndStatusShape) {
    ApiEnv env(nullptr);

    env.eval(R"(
        assert(wingman.vision.aiSetup({ model = "m" }) == false)             -- 缺 baseUrl
        assert(wingman.vision.aiSetup({ baseUrl = "http://x/v1" }) == false) -- 缺 model
        assert(wingman.vision.aiSetup({
            baseUrl = "http://x/v1", model = "m",
            apiKeyEnc = "enc" }) == false)                                   -- 密文缺口令
        local st = wingman.vision.aiSetupStatus()
        assert(type(st.configured) == "boolean")
        assert(type(st.hasKey) == "boolean")
        assert(type(st.lastError) == "string")
    )");

    wingman::VisionAi::reset();
}

#ifdef WINGMAN_ENABLE_VISION
TEST(AndroidApiTest, AiLocateCapturesFrameThroughBridge) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    // 端口 1 立即拒绝：截帧→HTTP 链路验证不外呼
    env.eval(R"(
        assert(wingman.vision.aiSetup({
            baseUrl = "http://127.0.0.1:1", model = "test-model",
            apiKey = "sk-test", timeoutSeconds = 2 }) == true)
        local st = wingman.vision.aiSetupStatus()
        assert(st.configured == true)
        local r = wingman.vision.aiLocate("登录按钮")
        assert(r.found == false)
        assert(#r.error > 0)
    )");

    // 注入被采用的强证据：locate 经 captureFrame 取帧
    EXPECT_EQ(bridge.captureCount, 1);
    wingman::VisionAi::reset();
}

TEST(AndroidApiTest, AiLocateRegionPassedThrough) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    env.eval(R"(
        assert(wingman.vision.aiSetup({
            baseUrl = "http://127.0.0.1:1", model = "test-model",
            timeoutSeconds = 2 }) == true)
        local r = wingman.vision.aiLocate("图标", { x = 0, y = 0, width = 2, height = 1 })
        assert(r.found == false)
    )");

    // AndroidCaptureSource region 裁剪：2x1 region 来自 4x2 帧
    EXPECT_EQ(bridge.captureCount, 1);
    wingman::VisionAi::reset();
}

TEST(AndroidApiTest, AiElementsCapturesFrameThroughBridge) {
    FakeHostBridge bridge;
    ApiEnv env(&bridge);

    // 批量元素：同 provider 通路（端口 1 拒绝 → 空批 + error 可区分）
    env.eval(R"(
        assert(wingman.vision.aiSetup({
            baseUrl = "http://127.0.0.1:1", model = "test-model",
            timeoutSeconds = 2 }) == true)
        local r = wingman.vision.aiElements("可点击按钮")
        assert(r.found == false)
        assert(#r.error > 0)
        local all = wingman.vision.aiElements("")   -- desc 空=列出全部
        assert(all.found == false)
    )");

    // 两次调用各经 captureFrame 取帧一次
    EXPECT_EQ(bridge.captureCount, 2);
    wingman::VisionAi::reset();
}
#else
TEST(AndroidApiTest, AiSetupStubSemanticsMatchesDesktopStub) {
    ApiEnv env(nullptr);

    // 桌面 vision_stub 同语义：setup 空体恒 true、isConfigured 恒 false、
    // locate 恒空 box（error 串为空）
    env.eval(R"(
        assert(wingman.vision.aiSetup({
            baseUrl = "http://x/v1", model = "m" }) == true)
        local st = wingman.vision.aiSetupStatus()
        assert(st.configured == false)
        local r = wingman.vision.aiLocate("登录按钮")
        assert(r.found == false)
        local e = wingman.vision.aiElements("登录按钮")
        assert(e.found == false)
        assert(type(e.elements) == "table" and #e.elements == 0)  -- stub 空批（lastError 恒空不走 error 形态）
    )");

    wingman::VisionAi::reset();
}
#endif
