#include "rpc_ipc_test_common.hpp"

// ========== script.*（真实 StandaloneMode + 真实 Lua） ==========

class ScriptHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        clearEventBuffer();
        // 幂等：static 注册器只在首次调用时把 Lua 引擎注册进 Factory
        wingman::lua::registerLuaEngine();
        config_.scriptDir = tempPath("scripts");
        standalone_ = std::make_unique<StandaloneMode>(config_);
        wingman::rpc::registerScriptHandlers(dispatcher_, *standalone_);
    }

    void TearDown() override {
        standalone_.reset();
        std::error_code ec;
        fs::remove_all(config_.scriptDir, ec);
    }

    std::string writeScript(const std::string& name, const std::string& body) {
        fs::create_directories(config_.scriptDir);
        const auto path = fs::path(config_.scriptDir) / name;
        std::ofstream file(path);
        file << body;
        return path.string();
    }

    StandaloneModeConfig config_;
    std::unique_ptr<StandaloneMode> standalone_;
    RpcDispatcher dispatcher_;
};

TEST_F(ScriptHandlerTest, ListEmptyInitially) {
    const auto response = dispatchCall(dispatcher_, "script.list");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_TRUE(response["data"]["result"]["scripts"].empty());
}

TEST_F(ScriptHandlerTest, ListReflectsLoadedScript) {
    const auto path = writeScript("simple.lua", "return 1\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(dispatcher_, "script.list");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& scripts = response["data"]["result"]["scripts"];
    ASSERT_EQ(scripts.size(), 1u);
    EXPECT_EQ(scripts[0]["id"].get<std::string>(), id);
    EXPECT_EQ(scripts[0]["path"].get<std::string>(), path);
    // 产品语义：ScriptManager 加载后的初始状态为 unloaded，runtime 层映射为
    // "stopped"（script_handler 的 scriptStateToString）
    EXPECT_EQ(scripts[0]["state"].get<std::string>(), "stopped");
    EXPECT_FALSE(scripts[0]["isRunning"].get<bool>());
    EXPECT_TRUE(scripts[0].contains("loadedAt"));
}

TEST_F(ScriptHandlerTest, StartWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "script.start");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(ScriptHandlerTest, StartWithNonexistentFileFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.start", json{{"path", tempPath("missing") + ".lua"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load script");
}

TEST_F(ScriptHandlerTest, StartBlocksThenStopReportsNotRunning) {
    // 产品契约：ScriptManager 为同步执行模型，script.start 阻塞至脚本执行
    // 完成才返回。顺序 RPC 因此永远无法命中 running 窗口——对已完成脚本
    // stop 返回失败信封是确定性契约（「GUI 停止运行中脚本」在该模型下不可
    // 达，属真机观察范畴，见 todo.md）。
    const auto path = writeScript("life.lua", "return 7\n");
    const auto started = dispatchCall(
        dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(started["data"]["success"].get<bool>()) << started.dump();
    const auto scriptId = started["data"]["result"]["scriptId"].get<std::string>();
    EXPECT_FALSE(scriptId.empty());
    EXPECT_FALSE(started["data"]["result"]["reused"].get<bool>());
    EXPECT_EQ(started["data"]["result"]["status"].get<std::string>(), "running");

    const auto stopped = dispatchCall(
        dispatcher_, "script.stop", json{{"scriptId", scriptId}});
    EXPECT_FALSE(stopped["data"]["success"].get<bool>());
    EXPECT_EQ(stopped["data"]["error"].get<std::string>(), "Failed to stop script");

    const auto unloaded = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", scriptId}});
    EXPECT_TRUE(unloaded["data"]["success"].get<bool>()) << unloaded.dump();
}

TEST_F(ScriptHandlerTest, StartReusesLoadedScriptById) {
    const auto path = writeScript("reuse.lua", "return 2\n");
    const auto first = dispatchCall(dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(first["data"]["success"].get<bool>()) << first.dump();
    const auto firstId = first["data"]["result"]["scriptId"].get<std::string>();

    const auto second = dispatchCall(dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(second["data"]["success"].get<bool>()) << second.dump();
    EXPECT_TRUE(second["data"]["result"]["reused"].get<bool>());
    EXPECT_EQ(second["data"]["result"]["scriptId"].get<std::string>(), firstId);
}

TEST_F(ScriptHandlerTest, StopWithoutScriptIdFails) {
    const auto response = dispatchCall(dispatcher_, "script.stop");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing scriptId");
}

TEST_F(ScriptHandlerTest, StopUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.stop", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to stop script");
}

TEST_F(ScriptHandlerTest, PauseAndResumeUnknownScriptFail) {
    const auto paused = dispatchCall(
        dispatcher_, "script.pause", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(paused["data"]["success"].get<bool>());
    EXPECT_EQ(paused["data"]["error"].get<std::string>(), "Failed to pause script");

    const auto resumed = dispatchCall(
        dispatcher_, "script.resume", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(resumed["data"]["success"].get<bool>());
    EXPECT_EQ(resumed["data"]["error"].get<std::string>(), "Failed to resume script");
}

TEST_F(ScriptHandlerTest, PauseLoadedButNotRunningScriptFails) {
    // 运行中脚本的协作暂停属真机观察范畴；此处锁定确定性路径：
    // loaded（未运行）脚本 pause 必须 false 而非伪成功
    const auto path = writeScript("idle.lua", "return 3\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.pause", json{{"scriptId", id}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to pause script");
}

TEST_F(ScriptHandlerTest, RestartUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.restart", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Script not found");
}

TEST_F(ScriptHandlerTest, RestartLoadedScriptSucceeds) {
    const auto path = writeScript("restart.lua", "return 4\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.restart", json{{"scriptId", id}});
    EXPECT_TRUE(response["data"]["success"].get<bool>()) << response.dump();
}

TEST_F(ScriptHandlerTest, UnloadUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "Failed to unload script (stop it first)");
}

TEST_F(ScriptHandlerTest, UnloadLoadedScriptSucceeds) {
    const auto path = writeScript("unload.lua", "return 5\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", id}});
    EXPECT_TRUE(response["data"]["success"].get<bool>()) << response.dump();

    const auto listed = dispatchCall(dispatcher_, "script.list");
    EXPECT_TRUE(listed["data"]["result"]["scripts"].empty());
}

// 缺陷回归守卫（本轮修复）：仅 LocalIpc 能力的 runtime 不会 start()
// StandaloneMode，GUI 仍可经 script.start 加载脚本；析构时 stop() 原实现的
// running_ 早退会跳过清理，把脚本连同引擎永久留在进程级全局 ScriptManager。
TEST_F(ScriptHandlerTest, DestructorReleasesNeverStartedLoadedScripts) {
    const auto path = writeScript("leak.lua", "return 6\n");
    std::string id;
    {
        StandaloneMode transient(config_);
        id = transient.loadScript(path);
        ASSERT_FALSE(id.empty());
        ASSERT_NE(wingman::runtime::getScriptManager().getScriptInfo(id), nullptr);
    } // 析构：未 start() 也必须清理登记
    EXPECT_EQ(wingman::runtime::getScriptManager().getScriptInfo(id), nullptr);
}
