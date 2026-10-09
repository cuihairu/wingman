// screenshot.capture RPC 端到端：registerScreenshotHandlers 注册 →
// dispatcher.dispatch 真请求 → 断言 JPEG data URI 信封。
// vision 构建（WINGMAN_ENABLE_VISION）走真链路：Screen::capture → BGRA→BGR →
// imencode(".jpg", 质量 82) → base64；断言 base64 解码首两字节为 JPEG SOI
// （FF D8）、width/height/region 回显。无 X 环境 GTEST_SKIP（capture 优雅
// 失败路径，同 platform_x11_test 先例）。无 vision 构建断言降级错误信封
// （screenshot.capture 不再是「注册了但必失败」的哑路径，行为受回归守卫）。
#include <gtest/gtest.h>

#include "wingman/runtime/rpc/screenshot_handler.hpp"
#include "wingman/crypt.hpp"
#include "wingman/screen.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

// 分发一条 screenshot.capture 请求（指定 region），返回 data 对象
json dispatchCapture(const std::string& rawParams) {
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerScreenshotHandlers(dispatcher);
    const auto request = json{{"method", "screenshot.capture"},
                              {"id", "vision-e2e"},
                              {"params", json::parse(rawParams)}};
    const auto response = json::parse(dispatcher.dispatch(request.dump()));
    EXPECT_EQ(response.value("type", ""), "response");
    return response.at("data");
}

} // namespace

#ifdef WINGMAN_ENABLE_VISION

TEST(ScreenshotHandlerTest, CaptureReturnsJpegDataUri) {
    if (wingman::Screen::getScreenWidth() <= 0) {
        GTEST_SKIP() << "X display unavailable (try: Xvfb :99 & DISPLAY=:99 ctest)";
    }

    const auto data = dispatchCapture(
        R"({"region":{"x":10,"y":10,"width":64,"height":48}})");

    ASSERT_TRUE(data.value("success", false)) << data.dump();
    const auto& result = data.at("result");
    const auto image = result.at("image").get<std::string>();
    EXPECT_TRUE(image.starts_with("data:image/jpeg;base64,"));

    // base64 解码后首两字节必须是 JPEG SOI 标记（真编码链路的硬证据）
    const auto bytes = wingman::crypt::base64Decode(
        image.substr(image.find(',') + 1));
    ASSERT_GT(bytes.size(), 100u);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0xD8);

    EXPECT_EQ(result.at("width").get<int>(), 64);
    EXPECT_EQ(result.at("height").get<int>(), 48);
    EXPECT_EQ(result.at("region").at("x").get<int>(), 10);
    EXPECT_EQ(result.at("region").at("width").get<int>(), 64);
}

TEST(ScreenshotHandlerTest, CaptureFullMonitorWhenRegionOmitted) {
    if (wingman::Screen::getScreenWidth() <= 0) {
        GTEST_SKIP() << "X display unavailable (try: Xvfb :99 & DISPLAY=:99 ctest)";
    }

    // 无 region → 整个主显示器
    const auto data = dispatchCapture("{}");
    ASSERT_TRUE(data.value("success", false)) << data.dump();
    EXPECT_GT(data.at("result").at("width").get<int>(), 0);
    EXPECT_GT(data.at("result").at("height").get<int>(), 0);
}

#else // !WINGMAN_ENABLE_VISION

TEST(ScreenshotHandlerTest, CaptureWithoutVisionReturnsErrorEnvelope) {
    // 无 vision 构建（Linux 默认，未启用 vcpkg vision feature）：注册的
    // screenshot.capture 必须以明确错误信封降级，而非抛异常/静默
    const auto data = dispatchCapture(
        R"({"region":{"x":0,"y":0,"width":8,"height":8}})");
    EXPECT_FALSE(data.value("success", true));
    EXPECT_NE(data.at("error").get<std::string>().find(
                  "WINGMAN_ENABLE_VISION"), std::string::npos);
}

#endif // WINGMAN_ENABLE_VISION
