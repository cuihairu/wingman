// system.* RPC handlers（runtime 版）——原 lib/wingman stub 版已删除，
// system.getStatus/getVersion 由 registerRuntimeSystemHandlers 统一提供。
#include "wingman/runtime/rpc/system_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/rpc/rpc_dispatcher.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

class SystemHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        standalone_ = std::make_unique<wingman::runtime::StandaloneMode>(
            wingman::runtime::StandaloneModeConfig{});
        wingman::rpc::registerRuntimeSystemHandlers(dispatcher_, "test-version",
                                                    *standalone_);
    }

    json call(const std::string& method) {
        const json request = {{"method", method}, {"id", "1"}, {"params", json::object()}};
        return json::parse(dispatcher_.dispatch(request.dump()));
    }

    wingman::rpc::RpcDispatcher dispatcher_;
    std::unique_ptr<wingman::runtime::StandaloneMode> standalone_;
};

TEST_F(SystemHandlerTest, GetStatusReportsIdleRuntime) {
    const auto response = call("system.getStatus");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["server"], "wingman");
    EXPECT_EQ(result["version"], "test-version");
    EXPECT_TRUE(result.contains("uptime"));
    EXPECT_EQ(result["runningScripts"], 0u);
    EXPECT_FALSE(result["paused"].get<bool>());
    // 未注入 providers 时的回退值（禁用/未知）
    EXPECT_EQ(result["remoteState"], "disabled");
    EXPECT_FALSE(result["remoteConnected"].get<bool>());
    EXPECT_EQ(result["mode"], 0);
}

TEST_F(SystemHandlerTest, GetVersionReturnsRegisteredVersion) {
    const auto response = call("system.getVersion");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["server"], "wingman");
    EXPECT_EQ(result["version"], "test-version");
    EXPECT_TRUE(result.contains("buildDate"));
}

TEST_F(SystemHandlerTest, IsPausedDefaultsToFalse) {
    const auto response = call("system.isPaused");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_FALSE(response["data"]["result"]["paused"].get<bool>());
}

} // namespace
