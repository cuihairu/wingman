#include "rpc_ipc_test_common.hpp"

// ========== events.drain ==========

TEST(EventHandlerTest, DrainRespectsLimitAndReportsRemaining) {
    clearEventBuffer();
    auto& buffer = wingman::runtime::EventBuffer::instance();
    buffer.push("t.one", {{"k", 1}});
    buffer.push("t.two", {{"k", 2}});
    buffer.push("t.three", {{"k", 3}});

    RpcDispatcher dispatcher;
    wingman::rpc::registerEventHandlers(dispatcher);

    const auto response = dispatchCall(dispatcher, "events.drain", json{{"max", 2}});
    const auto& result = response.at("data").at("result");
    ASSERT_EQ(result.at("events").size(), 2u);
    EXPECT_EQ(result.at("events")[0].at("method").get<std::string>(), "t.one");
    EXPECT_EQ(result.at("events")[1].at("method").get<std::string>(), "t.two");
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 1u);
    EXPECT_TRUE(result.contains("dropped"));

    const auto rest = dispatchCall(dispatcher, "events.drain");
    const auto& restResult = rest.at("data").at("result");
    ASSERT_EQ(restResult.at("events").size(), 1u);
    EXPECT_EQ(restResult.at("events")[0].at("method").get<std::string>(), "t.three");
    EXPECT_EQ(restResult.at("remaining").get<std::size_t>(), 0u);
}

TEST(EventHandlerTest, DrainOnEmptyBufferYieldsEmptyWithCounters) {
    clearEventBuffer();
    RpcDispatcher dispatcher;
    wingman::rpc::registerEventHandlers(dispatcher);

    const auto response = dispatchCall(dispatcher, "events.drain");
    const auto& result = response.at("data").at("result");
    EXPECT_TRUE(result.at("events").empty());
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 0u);
}

// ========== config.getRemote / config.setRemote ==========

TEST(ConfigHandlerTest, GetRemoteWithoutAccessReturnsError) {
    RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, {});

    const auto response = dispatchCall(dispatcher, "config.getRemote");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "remote config not available");
}

TEST(ConfigHandlerTest, SetRemoteWithoutApplyReturnsError) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json{{"serverIp", "10.0.0.1"}}; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote", json{{}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "remote config not writable");
}

TEST(ConfigHandlerTest, GetRemoteReturnsProvidedConfig) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json{{"serverIp", "10.0.0.1"}, {"serverPort", 9000}}; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.getRemote");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["serverIp"].get<std::string>(), "10.0.0.1");
    EXPECT_EQ(result["serverPort"].get<int>(), 9000);
}

TEST(ConfigHandlerTest, SetRemoteAppliesAndReturnsFreshConfig) {
    RpcDispatcher dispatcher;
    json applied;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [&applied] { return applied; };
    access.apply = [&applied](const json& request) -> std::string {
        applied = request.value("config", json::object());
        return {};
    };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote",
        json{{"config", {{"serverIp", "9.9.9.9"}}}});
    ASSERT_TRUE(response["data"]["success"].get<bool>()) << response.dump();
    EXPECT_EQ(response["data"]["result"]["serverIp"].get<std::string>(), "9.9.9.9");
}

TEST(ConfigHandlerTest, SetRemotePropagatesApplyError) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json::object(); };
    access.apply = [](const json&) -> std::string { return "bad token"; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote", json{{}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "bad token");
}

