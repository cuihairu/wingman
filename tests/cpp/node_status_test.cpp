#include <gtest/gtest.h>
#include "wingman/node_status.hpp"

using namespace wingman;

class NodeStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// NodeState Tests
// ============================================================================

TEST_F(NodeStatusTest, NodeStateValues) {
    EXPECT_EQ(static_cast<int>(NodeState::Online), 0);
    EXPECT_EQ(static_cast<int>(NodeState::Busy), 1);
    EXPECT_EQ(static_cast<int>(NodeState::Idle), 2);
    EXPECT_EQ(static_cast<int>(NodeState::Error), 3);
    EXPECT_EQ(static_cast<int>(NodeState::Offline), 4);
}

// ============================================================================
// GameWindowStatus Tests
// ============================================================================

TEST_F(NodeStatusTest, GameWindowStatusDefaults) {
    GameWindowStatus status{};  // Value initialization
    EXPECT_TRUE(status.title.empty());
    EXPECT_TRUE(status.processName.empty());
    EXPECT_EQ(status.handle, 0);
    EXPECT_EQ(status.x, 0);
    EXPECT_EQ(status.y, 0);
    EXPECT_EQ(status.width, 0);
    EXPECT_EQ(status.height, 0);
    EXPECT_FALSE(status.isForeground);
}

TEST_F(NodeStatusTest, GameWindowStatusWithValues) {
    GameWindowStatus status{};
    status.title = "Test Game";
    status.processName = "game.exe";
    status.handle = 12345;
    status.x = 100;
    status.y = 100;
    status.width = 1920;
    status.height = 1080;
    status.isForeground = true;

    EXPECT_EQ(status.title, "Test Game");
    EXPECT_EQ(status.processName, "game.exe");
    EXPECT_EQ(status.handle, 12345);
    EXPECT_EQ(status.width, 1920);
    EXPECT_TRUE(status.isForeground);
}

TEST_F(NodeStatusTest, GameWindowStatusToJson) {
    GameWindowStatus status{};
    status.title = "Test Game";
    status.processName = "game.exe";

    std::string json = status.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("Test Game"), std::string::npos);
}

TEST_F(NodeStatusTest, GameWindowStatusFromJson) {
    std::string json = R"({
        "title": "Test Game",
        "processName": "game.exe",
        "handle": 12345,
        "x": 100,
        "y": 100,
        "width": 1920,
        "height": 1080,
        "isForeground": true
    })";

    auto status = GameWindowStatus::fromJson(json);
    EXPECT_EQ(status.title, "Test Game");
    EXPECT_EQ(status.processName, "game.exe");
    EXPECT_EQ(status.handle, 12345);
}

// ============================================================================
// ScriptStatus Tests
// ============================================================================

TEST_F(NodeStatusTest, ScriptStatusDefaults) {
    ScriptStatus status{};
    EXPECT_TRUE(status.name.empty());
    EXPECT_TRUE(status.state.empty());
    EXPECT_EQ(status.uptimeSeconds, 0);
    EXPECT_TRUE(status.lastError.empty());
}

TEST_F(NodeStatusTest, ScriptStatusWithValues) {
    ScriptStatus status{};
    status.name = "test_script.lua";
    status.state = "running";
    status.uptimeSeconds = 3600;
    status.lastError = "";

    EXPECT_EQ(status.name, "test_script.lua");
    EXPECT_EQ(status.state, "running");
    EXPECT_EQ(status.uptimeSeconds, 3600);
}

TEST_F(NodeStatusTest, ScriptStatusToJson) {
    ScriptStatus status{};
    status.name = "test.lua";
    status.state = "running";

    std::string json = status.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("test.lua"), std::string::npos);
}

TEST_F(NodeStatusTest, ScriptStatusFromJson) {
    std::string json = R"({
        "name": "test.lua",
        "state": "running",
        "uptimeSeconds": 3600,
        "lastError": ""
    })";

    auto status = ScriptStatus::fromJson(json);
    EXPECT_EQ(status.name, "test.lua");
    EXPECT_EQ(status.state, "running");
    EXPECT_EQ(status.uptimeSeconds, 3600);
}

// ============================================================================
// NodeHeartbeat Tests
// ============================================================================

TEST_F(NodeStatusTest, NodeHeartbeatDefaults) {
    NodeHeartbeat heartbeat{};
    EXPECT_TRUE(heartbeat.nodeId.empty());
    EXPECT_TRUE(heartbeat.hostname.empty());
    EXPECT_EQ(heartbeat.timestamp, 0);
    EXPECT_EQ(heartbeat.cpuUsage, 0.0);
    EXPECT_EQ(heartbeat.memoryUsage, 0.0);
    EXPECT_TRUE(heartbeat.games.empty());
    EXPECT_TRUE(heartbeat.scripts.empty());
    EXPECT_TRUE(heartbeat.version.empty());
}

TEST_F(NodeStatusTest, NodeHeartbeatWithValues) {
    NodeHeartbeat heartbeat{};
    heartbeat.nodeId = "node_001";
    heartbeat.hostname = "test-pc";
    heartbeat.status = NodeState::Online;
    heartbeat.timestamp = 1234567890;
    heartbeat.cpuUsage = 50.5;
    heartbeat.memoryUsage = 2048.0;
    heartbeat.version = "0.1.0";

    EXPECT_EQ(heartbeat.nodeId, "node_001");
    EXPECT_EQ(heartbeat.hostname, "test-pc");
    EXPECT_EQ(heartbeat.status, NodeState::Online);
    EXPECT_EQ(heartbeat.cpuUsage, 50.5);
}

TEST_F(NodeStatusTest, NodeHeartbeatNow) {
    int64_t now = NodeHeartbeat::now();
    EXPECT_GT(now, 0);
}

TEST_F(NodeStatusTest, NodeHeartbeatToJson) {
    NodeHeartbeat heartbeat{};
    heartbeat.nodeId = "node_001";
    heartbeat.hostname = "test-pc";
    heartbeat.status = NodeState::Online;

    std::string json = heartbeat.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("node_001"), std::string::npos);
}

TEST_F(NodeStatusTest, NodeHeartbeatFromJson) {
    std::string json = R"({
        "nodeId": "node_001",
        "hostname": "test-pc",
        "status": 0,
        "timestamp": 1234567890,
        "cpuUsage": 50.5,
        "memoryUsage": 2048.0,
        "version": "0.1.0"
    })";

    auto heartbeat = NodeHeartbeat::fromJson(json);
    EXPECT_EQ(heartbeat.nodeId, "node_001");
    EXPECT_EQ(heartbeat.hostname, "test-pc");
    EXPECT_EQ(heartbeat.status, NodeState::Online);
}

// ============================================================================
// ServerCommand Tests
// ============================================================================

TEST_F(NodeStatusTest, ServerCommandValues) {
    EXPECT_EQ(static_cast<int>(ServerCommand::None), 0);
    EXPECT_EQ(static_cast<int>(ServerCommand::StartScript), 1);
    EXPECT_EQ(static_cast<int>(ServerCommand::StopScript), 2);
    EXPECT_EQ(static_cast<int>(ServerCommand::PauseScript), 3);
    EXPECT_EQ(static_cast<int>(ServerCommand::ResumeScript), 4);
    EXPECT_EQ(static_cast<int>(ServerCommand::Restart), 5);
    EXPECT_EQ(static_cast<int>(ServerCommand::Shutdown), 6);
    EXPECT_EQ(static_cast<int>(ServerCommand::UpdateConfig), 7);
}

// ============================================================================
// ServerCommandData Tests
// ============================================================================

TEST_F(NodeStatusTest, ServerCommandDataDefaults) {
    ServerCommandData data{};
    EXPECT_EQ(data.command, ServerCommand::None);
    EXPECT_TRUE(data.scriptPath.empty());
    EXPECT_TRUE(data.configData.empty());
    EXPECT_EQ(data.timestamp, 0);
}

TEST_F(NodeStatusTest, ServerCommandDataWithValues) {
    ServerCommandData data{};
    data.command = ServerCommand::StartScript;
    data.scriptPath = "scripts/test.lua";
    data.timestamp = 1234567890;

    EXPECT_EQ(data.command, ServerCommand::StartScript);
    EXPECT_EQ(data.scriptPath, "scripts/test.lua");
}

TEST_F(NodeStatusTest, ServerCommandDataToJson) {
    ServerCommandData data{};
    data.command = ServerCommand::StartScript;
    data.scriptPath = "test.lua";

    std::string json = data.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("test.lua"), std::string::npos);
}

TEST_F(NodeStatusTest, ServerCommandDataFromJson) {
    std::string json = R"({
        "command": "start_script",
        "scriptPath": "test.lua",
        "configData": "",
        "timestamp": 1234567890
    })";

    auto data = ServerCommandData::fromJson(json);
    EXPECT_EQ(data.command, ServerCommand::StartScript);
    EXPECT_EQ(data.scriptPath, "test.lua");
}
