#include "rpc_ipc_test_common.hpp"

// ========== system.*（providers 注入与批量操作分支） ==========

class SystemProvidersHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        standalone_ = std::make_unique<StandaloneMode>(StandaloneModeConfig{});
        providers_.remoteConnected = [] { return true; };
        providers_.remoteStateName = [] { return std::string("connected"); };
        providers_.ipcClientConnected = [] { return true; };
        providers_.runMode = [] { return 3; };
        wingman::rpc::registerRuntimeSystemHandlers(
            dispatcher_, "prov-version", *standalone_, providers_);
    }

    std::unique_ptr<StandaloneMode> standalone_;
    wingman::rpc::RuntimeStatusProviders providers_;
    RpcDispatcher dispatcher_;
};

TEST_F(SystemProvidersHandlerTest, GetStatusReflectsInjectedProviders) {
    const auto response = dispatchCall(dispatcher_, "system.getStatus");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["version"].get<std::string>(), "prov-version");
    EXPECT_TRUE(result["remoteConnected"].get<bool>());
    EXPECT_EQ(result["remoteState"].get<std::string>(), "connected");
    EXPECT_TRUE(result["ipcClientConnected"].get<bool>());
    EXPECT_EQ(result["mode"].get<int>(), 3);
}

TEST_F(SystemProvidersHandlerTest, TogglePauseWithoutScriptsFlipsToPaused) {
    const auto response = dispatchCall(dispatcher_, "system.togglePause");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_TRUE(result["paused"].get<bool>());
    EXPECT_EQ(result["changedScripts"].get<std::size_t>(), 0u);
}

TEST_F(SystemProvidersHandlerTest, BatchPauseResumeStopWithoutScriptsAreNoops) {
    const auto paused = dispatchCall(dispatcher_, "system.pauseAll");
    ASSERT_TRUE(paused["data"]["success"].get<bool>());
    EXPECT_EQ(paused["data"]["result"]["changedScripts"].get<std::size_t>(), 0u);

    const auto resumed = dispatchCall(dispatcher_, "system.resumeAll");
    ASSERT_TRUE(resumed["data"]["success"].get<bool>());
    EXPECT_EQ(resumed["data"]["result"]["changedScripts"].get<std::size_t>(), 0u);

    const auto stopped = dispatchCall(dispatcher_, "system.stopAll");
    ASSERT_TRUE(stopped["data"]["success"].get<bool>());
    EXPECT_EQ(stopped["data"]["result"]["stoppedScripts"].get<std::size_t>(), 0u);
}

// ========== EventLogSink ==========

TEST(EventLogSinkTest, ForwardsInfoWarnErrorAndFiltersDebug) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>(
        "wm-rpcipc-evlog", wingman::runtime::createEventLogSink());
    logger->debug("wm-rpcipc-debug-filtered");
    logger->info("wm-rpcipc-info-{}", 42);
    logger->warn("wm-rpcipc-warn-passed");
    logger->error("wm-rpcipc-error-passed");
    logger.reset();

    // token 扫描断言（进程内可能存在其他异步生产者，不做事件总量断言）
    bool sawInfo = false, sawWarn = false, sawError = false, sawDebug = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto& payload = event.payload;
        const auto message = payload.at("message").get<std::string>();
        const auto level = payload.at("level").get<std::string>();
        if (message == "wm-rpcipc-info-42") {
            sawInfo = (level == "info");
        } else if (message == "wm-rpcipc-warn-passed") {
            // spdlog 的 warn 级别名在不同小版本间为 warning/warn
            sawWarn = (level == "warning" || level == "warn");
        } else if (message == "wm-rpcipc-error-passed") {
            sawError = (level == "error");
        } else if (message == "wm-rpcipc-debug-filtered") {
            sawDebug = true; // debug 低于 info 必须被过滤
        }
    }
    EXPECT_TRUE(sawInfo);
    EXPECT_TRUE(sawWarn);
    EXPECT_TRUE(sawError);
    EXPECT_FALSE(sawDebug);
}

TEST(EventLogSinkTest, TruncatesOversizedLines) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>(
        "wm-rpcipc-evlog-trunc", wingman::runtime::createEventLogSink());
    logger->error(std::string(5000, 'x'));
    logger.reset();

    bool found = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto message = event.payload.at("message").get<std::string>();
        if (message.size() > 4000) {
            found = true;
            EXPECT_EQ(message.size(), 4096u + std::string("...[truncated]").size());
            EXPECT_TRUE(message.ends_with("...[truncated]"));
        }
    }
    EXPECT_TRUE(found);
}

TEST(EventLogSinkTest, ConstructorArgRaisesFilterFloor) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>("wm-rpcipc-evlog-floor",
        wingman::runtime::createEventLogSink(
            static_cast<std::size_t>(spdlog::level::err)));
    logger->warn("wm-rpcipc-warn-filtered");
    logger->error("wm-rpcipc-error-passed");
    logger.reset();

    bool sawError = false, sawWarn = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto message = event.payload.at("message").get<std::string>();
        if (message == "wm-rpcipc-error-passed") {
            sawError = true;
        } else if (message == "wm-rpcipc-warn-filtered") {
            sawWarn = true; // err 下限之上 warn 必须被过滤
        }
    }
    EXPECT_TRUE(sawError);
    EXPECT_FALSE(sawWarn);
}
