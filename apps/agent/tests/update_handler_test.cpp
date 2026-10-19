// update.* RPC（本地 IPC only）契约测试：enabled=false 的禁用分支、异步触发
// 的 started 信封、以及 check/download 经轮询器 worker 线程落地后的状态。
// 轮询器用注入的假传输驱动，不触网。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/runtime/rpc/update_handler.hpp"
#include "wingman/update_poller.hpp"

#include <chrono>
#include <string>
#include <thread>

using json = nlohmann::json;
using wingman::UpdateManifest;
using wingman::UpdatePollConfig;
using wingman::UpdatePoller;

namespace {

json dispatchCall(wingman::rpc::RpcDispatcher& dispatcher, const std::string& method,
                  const json& params = json::object()) {
    const json request = {
        {"type", "call"}, {"id", "t"}, {"method", method}, {"params", params}};
    return json::parse(dispatcher.dispatch(request.dump()));
}

json resultOf(const json& response) {
    return response["data"]["result"];
}

bool waitForCondition(const std::function<bool()>& pred) {
    for (int i = 0; i < 200; ++i) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

UpdateManifest newerManifest() {
    UpdateManifest m;
    m.version = "1.1.0";
    m.url = "/api/v1/update/download/9";
    m.sha256 = "abcd";
    m.size = 42;
    m.notes = "notes";
    m.fileName = "wingman-1.1.0.exe";
    return m;
}

// 构造一个注入了假传输、能立即返回 1.1.0 的轮询器（pollOnce 直接用）。
UpdatePoller makePoller(bool withDownload = true) {
    UpdatePollConfig cfg;
    cfg.baseUrl = "http://srv:9527";
    cfg.channel = "stable";
    cfg.platform = "linux";
    cfg.arch = "amd64";
    cfg.localVersion = "1.0.0";
    cfg.downloadDir = "/tmp/wingman-update-test";
    cfg.pollIntervalSeconds = 60;
    // 按值捕获：makePoller 返回后局部 remote 已析构，引用捕获会悬垂
    UpdateManifest remote = newerManifest();
    cfg.checkFn = [remote](const std::string&, const std::string&, const std::string&,
                           const std::string&, UpdateManifest& out) {
        out = remote;
        return true;
    };
    if (withDownload) {
        cfg.downloadFn = [](const std::string&, const std::string&, const std::string&) {
            return true;
        };
    }
    return UpdatePoller(cfg);
}

} // namespace

TEST(UpdateRpcHandler, DisabledWithoutPoller) {
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, nullptr);

    const auto status = resultOf(dispatchCall(dispatcher, "update.status"));
    EXPECT_FALSE(status["enabled"].get<bool>());
    EXPECT_FALSE(status["available"].get<bool>());
    EXPECT_EQ(status["lastError"].get<std::string>(), "update not configured");

    const auto check = resultOf(dispatchCall(dispatcher, "update.check"));
    EXPECT_FALSE(check["started"].get<bool>());
    EXPECT_FALSE(check["status"]["enabled"].get<bool>());

    const auto download = resultOf(dispatchCall(dispatcher, "update.download"));
    EXPECT_FALSE(download["started"].get<bool>());
}

TEST(UpdateRpcHandler, StatusEnvelopeShapeMatchesEnabled) {
    auto poller = makePoller();
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, &poller);

    const auto status = resultOf(dispatchCall(dispatcher, "update.status"));
    EXPECT_TRUE(status["enabled"].get<bool>());
    EXPECT_FALSE(status["available"].get<bool>());
    EXPECT_FALSE(status["downloading"].get<bool>());
    EXPECT_FALSE(status["downloaded"].get<bool>());
    EXPECT_EQ(status["localVersion"].get<std::string>(), "1.0.0");
    EXPECT_EQ(status["candidate"]["version"].get<std::string>(), "");
    EXPECT_EQ(status["downloadPath"].get<std::string>(), "");
    EXPECT_EQ(status["lastCheckMs"].get<int64_t>(), 0);
}

TEST(UpdateRpcHandler, CheckTriggersAsyncAndSurfacesCandidate) {
    auto poller = makePoller();
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, &poller);
    ASSERT_TRUE(poller.start());

    const auto started = resultOf(dispatchCall(dispatcher, "update.check"));
    EXPECT_TRUE(started["started"].get<bool>());
    EXPECT_FALSE(started["status"]["available"].get<bool>());  // 尚未执行完

    ASSERT_TRUE(waitForCondition([&poller] { return poller.status().available; }));
    poller.stop();

    const auto status = resultOf(dispatchCall(dispatcher, "update.status"));
    EXPECT_TRUE(status["available"].get<bool>());
    EXPECT_EQ(status["candidate"]["version"].get<std::string>(), "1.1.0");
    EXPECT_EQ(status["candidate"]["url"].get<std::string>(), "/api/v1/update/download/9");
    EXPECT_EQ(status["candidate"]["sha256"].get<std::string>(), "abcd");
    EXPECT_EQ(status["candidate"]["fileName"].get<std::string>(), "wingman-1.1.0.exe");
    EXPECT_GT(status["lastCheckMs"].get<int64_t>(), 0);
    EXPECT_TRUE(status["lastError"].get<std::string>().empty());
}

TEST(UpdateRpcHandler, DownloadTriggersAsyncAndReportsArtifactPath) {
    auto poller = makePoller();
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, &poller);
    ASSERT_TRUE(poller.start());
    poller.requestCheck();
    ASSERT_TRUE(waitForCondition([&poller] { return poller.status().available; }));

    const auto started = resultOf(dispatchCall(dispatcher, "update.download"));
    EXPECT_TRUE(started["started"].get<bool>());

    ASSERT_TRUE(waitForCondition([&poller] { return poller.status().downloaded; }));
    poller.stop();

    const auto status = resultOf(dispatchCall(dispatcher, "update.status"));
    EXPECT_TRUE(status["downloaded"].get<bool>());
    EXPECT_FALSE(status["available"].get<bool>());
    EXPECT_FALSE(status["downloading"].get<bool>());
    EXPECT_EQ(status["downloadPath"].get<std::string>(), "/tmp/wingman-update-test/wingman-1.1.0.exe");
}

TEST(UpdateRpcHandler, DisabledPollerStillAnswersWithEnabledFalse) {
    UpdatePollConfig cfg;  // baseUrl 为空 = 未启用
    cfg.localVersion = "1.0.0";
    cfg.downloadDir = "/tmp/wingman-update-test";
    UpdatePoller poller(cfg);
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, &poller);

    const auto status = resultOf(dispatchCall(dispatcher, "update.status"));
    EXPECT_FALSE(status["enabled"].get<bool>());
    EXPECT_EQ(status["lastError"].get<std::string>(), "update endpoint not configured");

    // 未启动的轮询器：check/download 触发不了，仍返回 started=true（请求已受理）
    const auto check = resultOf(dispatchCall(dispatcher, "update.check"));
    EXPECT_TRUE(check["started"].get<bool>());
    EXPECT_FALSE(check["status"]["available"].get<bool>());
}

TEST(UpdateRpcHandler, UnknownMethodYieldsErrorEnvelope) {
    wingman::rpc::RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeUpdateHandlers(dispatcher, nullptr);
    const auto response = dispatchCall(dispatcher, "update.nonexistent");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_NE(response["data"]["error"].get<std::string>().find("Unknown method"),
              std::string::npos);
}
