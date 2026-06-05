#include <gtest/gtest.h>
#include "wingman/node_status.hpp"

using namespace wingman;

// ========== GameWindowStatus ==========

TEST(GameWindowStatusTest, Roundtrip) {
    GameWindowStatus original;
    original.title = "TestWindow";
    original.processName = "test.exe";
    original.handle = 12345;
    original.x = 100;
    original.y = 200;
    original.width = 800;
    original.height = 600;
    original.isForeground = true;

    std::string json = original.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("TestWindow"), std::string::npos);

    auto restored = GameWindowStatus::fromJson(json);
    EXPECT_EQ(restored.title, "TestWindow");
    EXPECT_EQ(restored.processName, "test.exe");
    EXPECT_EQ(restored.handle, uintptr_t(12345));
    EXPECT_EQ(restored.x, 100);
    EXPECT_EQ(restored.y, 200);
    EXPECT_EQ(restored.width, 800);
    EXPECT_EQ(restored.height, 600);
    EXPECT_TRUE(restored.isForeground);
}

TEST(GameWindowStatusTest, DefaultValues) {
    GameWindowStatus status{};
    EXPECT_TRUE(status.title.empty());
    EXPECT_TRUE(status.processName.empty());
    EXPECT_EQ(status.handle, uintptr_t(0));
    EXPECT_EQ(status.x, 0);
    EXPECT_FALSE(status.isForeground);
}

// ========== ScriptStatus ==========

TEST(ScriptStatusTest, Roundtrip) {
    ScriptStatus original;
    original.name = "test_script";
    original.state = "running";
    original.uptimeSeconds = 3600;
    original.lastError = "";

    std::string json = original.toJson();
    EXPECT_NE(json.find("test_script"), std::string::npos);

    auto restored = ScriptStatus::fromJson(json);
    EXPECT_EQ(restored.name, "test_script");
    EXPECT_EQ(restored.state, "running");
    EXPECT_EQ(restored.uptimeSeconds, 3600);
    EXPECT_TRUE(restored.lastError.empty());
}

TEST(ScriptStatusTest, WithError) {
    ScriptStatus original;
    original.name = "broken";
    original.state = "error";
    original.lastError = "Something went wrong";

    std::string json = original.toJson();
    auto restored = ScriptStatus::fromJson(json);
    EXPECT_EQ(restored.state, "error");
    EXPECT_EQ(restored.lastError, "Something went wrong");
}

// ========== NodeHeartbeat ==========

TEST(NodeHeartbeatTest, Roundtrip) {
    NodeHeartbeat hb;
    hb.nodeId = "node-001";
    hb.hostname = "test-host";
    hb.status = NodeState::Online;
    hb.timestamp = 1700000000000LL;
    hb.cpuUsage = 45.5;
    hb.memoryUsage = 2048.0;
    hb.version = "1.0.0";

    GameWindowStatus win;
    win.title = "Game";
    hb.games.push_back(win);

    ScriptStatus script;
    script.name = "bot";
    script.state = "running";
    hb.scripts.push_back(script);

    std::string json = hb.toJson();
    EXPECT_NE(json.find("node-001"), std::string::npos);
    EXPECT_NE(json.find("Game"), std::string::npos);
    EXPECT_NE(json.find("bot"), std::string::npos);

    auto restored = NodeHeartbeat::fromJson(json);
    EXPECT_EQ(restored.nodeId, "node-001");
    EXPECT_EQ(restored.hostname, "test-host");
    EXPECT_EQ(restored.status, NodeState::Online);
    EXPECT_DOUBLE_EQ(restored.cpuUsage, 45.5);
    EXPECT_DOUBLE_EQ(restored.memoryUsage, 2048.0);
    EXPECT_EQ(restored.version, "1.0.0");
    EXPECT_EQ(restored.games.size(), 1u);
    EXPECT_EQ(restored.scripts.size(), 1u);
    EXPECT_EQ(restored.games[0].title, "Game");
    EXPECT_EQ(restored.scripts[0].name, "bot");
}

TEST(NodeHeartbeatTest, EmptyRoundtrip) {
    NodeHeartbeat hb;
    hb.nodeId = "empty";
    hb.status = NodeState::Idle;

    std::string json = hb.toJson();
    auto restored = NodeHeartbeat::fromJson(json);
    EXPECT_EQ(restored.nodeId, "empty");
    EXPECT_EQ(restored.status, NodeState::Idle);
    EXPECT_TRUE(restored.games.empty());
    EXPECT_TRUE(restored.scripts.empty());
}

TEST(NodeHeartbeatTest, NowReturnsReasonableTimestamp) {
    int64_t ts = NodeHeartbeat::now();
    EXPECT_GT(ts, 1000000000000LL);
    EXPECT_LT(ts, 2000000000000LL);
}

TEST(NodeHeartbeatTest, AllNodeStates) {
    for (auto state : {NodeState::Online, NodeState::Busy, NodeState::Idle,
                       NodeState::Error, NodeState::Offline}) {
        NodeHeartbeat hb;
        hb.status = state;
        std::string json = hb.toJson();
        auto restored = NodeHeartbeat::fromJson(json);
        EXPECT_EQ(restored.status, state);
    }
}

// ========== ServerCommandData ==========

TEST(ServerCommandDataTest, RoundtripStartScript) {
    ServerCommandData cmd;
    cmd.command = ServerCommand::StartScript;
    cmd.scriptPath = "/scripts/test.lua";
    cmd.timestamp = 1700000000000LL;

    std::string json = cmd.toJson();
    EXPECT_NE(json.find("start_script"), std::string::npos);
    EXPECT_NE(json.find("/scripts/test.lua"), std::string::npos);

    auto restored = ServerCommandData::fromJson(json);
    EXPECT_EQ(restored.command, ServerCommand::StartScript);
    EXPECT_EQ(restored.scriptPath, "/scripts/test.lua");
    EXPECT_EQ(restored.timestamp, 1700000000000LL);
}

TEST(ServerCommandDataTest, RoundtripUpdateConfig) {
    ServerCommandData cmd;
    cmd.command = ServerCommand::UpdateConfig;
    cmd.configData = "{\"key\":\"value\"}";
    cmd.timestamp = 1000LL;

    std::string json = cmd.toJson();
    auto restored = ServerCommandData::fromJson(json);
    EXPECT_EQ(restored.command, ServerCommand::UpdateConfig);
    EXPECT_EQ(restored.configData, "{\"key\":\"value\"}");
}

TEST(ServerCommandDataTest, AllCommands) {
    for (auto cmd : {ServerCommand::None, ServerCommand::StopScript,
                     ServerCommand::PauseScript, ServerCommand::ResumeScript,
                     ServerCommand::Restart, ServerCommand::Shutdown}) {
        ServerCommandData data;
        data.command = cmd;
        data.timestamp = 0;
        std::string json = data.toJson();
        auto restored = ServerCommandData::fromJson(json);
        EXPECT_EQ(restored.command, cmd);
    }
}

// ========== Invalid JSON Error Paths ==========

TEST(GameWindowStatusTest, FromJsonInvalidReturnsDefault) {
    auto status = GameWindowStatus::fromJson("not valid json {{{");
    EXPECT_TRUE(status.title.empty());
    EXPECT_TRUE(status.processName.empty());
    EXPECT_EQ(status.handle, uintptr_t(0));
    EXPECT_EQ(status.x, 0);
    EXPECT_EQ(status.y, 0);
    EXPECT_EQ(status.width, 0);
    EXPECT_EQ(status.height, 0);
    EXPECT_FALSE(status.isForeground);
}

TEST(GameWindowStatusTest, FromJsonEmptyObject) {
    auto status = GameWindowStatus::fromJson("{}");
    EXPECT_TRUE(status.title.empty());
    EXPECT_EQ(status.handle, uintptr_t(0));
}

TEST(ScriptStatusTest, FromJsonInvalidReturnsDefault) {
    auto status = ScriptStatus::fromJson("broken json");
    EXPECT_TRUE(status.name.empty());
    EXPECT_TRUE(status.state.empty());
    EXPECT_EQ(status.uptimeSeconds, 0u);
    EXPECT_TRUE(status.lastError.empty());
}

TEST(ScriptStatusTest, FromJsonEmptyObject) {
    auto status = ScriptStatus::fromJson("{}");
    EXPECT_TRUE(status.name.empty());
}

TEST(NodeHeartbeatTest, FromJsonInvalidReturnsDefault) {
    auto hb = NodeHeartbeat::fromJson("{invalid");
    EXPECT_TRUE(hb.nodeId.empty());
    EXPECT_TRUE(hb.hostname.empty());
    EXPECT_EQ(hb.timestamp, 0);
    EXPECT_TRUE(hb.games.empty());
    EXPECT_TRUE(hb.scripts.empty());
}

TEST(NodeHeartbeatTest, FromJsonEmptyObject) {
    auto hb = NodeHeartbeat::fromJson("{}");
    EXPECT_TRUE(hb.nodeId.empty());
}

TEST(ServerCommandDataTest, FromJsonInvalidReturnsDefault) {
    auto data = ServerCommandData::fromJson("not json");
    EXPECT_EQ(data.command, ServerCommand::None);
    EXPECT_EQ(data.timestamp, 0);
    EXPECT_TRUE(data.scriptPath.empty());
    EXPECT_TRUE(data.configData.empty());
}

TEST(ServerCommandDataTest, FromJsonEmptyObject) {
    auto data = ServerCommandData::fromJson("{}");
    EXPECT_EQ(data.command, ServerCommand::None);
}

TEST(ServerCommandDataTest, FromJsonUnknownCommand) {
    auto data = ServerCommandData::fromJson(R"({"command":"unknown_command","timestamp":100})");
    // Unknown command string should leave command as default None
    EXPECT_EQ(data.command, ServerCommand::None);
    EXPECT_EQ(data.timestamp, 100);
}

TEST(NodeHeartbeatTest, FromJsonUnknownStatus) {
    auto hb = NodeHeartbeat::fromJson(R"({"nodeId":"n1","status":"unknown_state"})");
    // Unknown status string should leave status as default Online
    EXPECT_EQ(hb.nodeId, "n1");
    EXPECT_EQ(hb.status, NodeState::Online);
}

TEST(NodeHeartbeatTest, FromJsonNonArrayGamesAndScripts) {
    // games/scripts fields that are not arrays should be ignored
    auto hb = NodeHeartbeat::fromJson(R"({"nodeId":"n2","games":"not_array","scripts":42})");
    EXPECT_EQ(hb.nodeId, "n2");
    EXPECT_TRUE(hb.games.empty());
    EXPECT_TRUE(hb.scripts.empty());
}
