#include <gtest/gtest.h>
#include "wingman/config.hpp"

using namespace wingman;

// ========== ServerConfig ==========

TEST(ServerConfigTest, DefaultValues) {
    ServerConfig cfg;
    EXPECT_EQ(cfg.host, "localhost");
    EXPECT_EQ(cfg.port, 9527);
    EXPECT_TRUE(cfg.username.empty());
    EXPECT_TRUE(cfg.password.empty());
    EXPECT_FALSE(cfg.autoConnect);
    EXPECT_FALSE(cfg.serverControlled);
}

TEST(ServerConfigTest, IsValid) {
    ServerConfig cfg;
    EXPECT_TRUE(cfg.isValid());

    cfg.host = "";
    EXPECT_FALSE(cfg.isValid());

    cfg.host = "example.com";
    cfg.port = 0;
    EXPECT_FALSE(cfg.isValid());

    cfg.port = 70000;
    EXPECT_FALSE(cfg.isValid());
}

TEST(ServerConfigTest, Roundtrip) {
    ServerConfig original;
    original.host = "192.168.1.1";
    original.port = 8080;
    original.username = "admin";
    original.autoConnect = true;

    std::string json = original.toJson();
    EXPECT_NE(json.find("192.168.1.1"), std::string::npos);

    auto restored = ServerConfig::fromJson(json);
    EXPECT_EQ(restored.host, "192.168.1.1");
    EXPECT_EQ(restored.port, 8080);
    EXPECT_EQ(restored.username, "admin");
    EXPECT_TRUE(restored.autoConnect);
}

// ========== AutoRunConfig ==========

TEST(AutoRunConfigTest, DefaultValues) {
    AutoRunConfig cfg;
    EXPECT_FALSE(cfg.enabled);
    EXPECT_TRUE(cfg.scriptPath.empty());
    EXPECT_EQ(cfg.delaySeconds, 0);
    EXPECT_FALSE(cfg.repeat);
}

TEST(AutoRunConfigTest, Roundtrip) {
    AutoRunConfig original;
    original.enabled = true;
    original.scriptPath = "/scripts/auto.lua";
    original.delaySeconds = 5;
    original.repeat = true;
    original.repeatInterval = 60;

    std::string json = original.toJson();
    auto restored = AutoRunConfig::fromJson(json);
    EXPECT_TRUE(restored.enabled);
    EXPECT_EQ(restored.scriptPath, "/scripts/auto.lua");
    EXPECT_EQ(restored.delaySeconds, 5);
    EXPECT_TRUE(restored.repeat);
    EXPECT_EQ(restored.repeatInterval, 60);
}

// ========== HeartbeatConfig ==========

TEST(HeartbeatConfigTest, DefaultValues) {
    HeartbeatConfig cfg;
    EXPECT_TRUE(cfg.enabled);
    EXPECT_EQ(cfg.intervalSeconds, 30);
    EXPECT_EQ(cfg.timeoutSeconds, 90);
}

TEST(HeartbeatConfigTest, Roundtrip) {
    HeartbeatConfig original;
    original.enabled = false;
    original.intervalSeconds = 15;
    original.timeoutSeconds = 45;

    std::string json = original.toJson();
    auto restored = HeartbeatConfig::fromJson(json);
    EXPECT_FALSE(restored.enabled);
    EXPECT_EQ(restored.intervalSeconds, 15);
    EXPECT_EQ(restored.timeoutSeconds, 45);
}

// ========== GameConfig ==========

TEST(GameConfigTest, DefaultValues) {
    GameConfig cfg;
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_TRUE(cfg.path.empty());
    EXPECT_FALSE(cfg.autoStart);
    EXPECT_EQ(cfg.delaySeconds, 5);
    EXPECT_EQ(cfg.maxRestarts, 3);
}

TEST(GameConfigTest, IsValid) {
    GameConfig cfg;
    EXPECT_FALSE(cfg.isValid());

    cfg.path = "C:\\game.exe";
    EXPECT_TRUE(cfg.isValid());
}

TEST(GameConfigTest, Roundtrip) {
    GameConfig original;
    original.name = "TestGame";
    original.path = "/games/test";
    original.args = "-windowed";
    original.autoStart = true;
    original.windowTitle = "Test Window";

    std::string json = original.toJson();
    EXPECT_NE(json.find("TestGame"), std::string::npos);

    auto restored = GameConfig::fromJson(json);
    EXPECT_EQ(restored.name, "TestGame");
    EXPECT_EQ(restored.path, "/games/test");
    EXPECT_EQ(restored.args, "-windowed");
    EXPECT_TRUE(restored.autoStart);
    EXPECT_EQ(restored.windowTitle, "Test Window");
}

// ========== ConfigManager ==========

TEST(ConfigManagerTest, LoadNonexistent) {
    ConfigManager mgr("nonexistent_config_dir_xyz");
    // Should not crash, returns defaults
    auto server = mgr.getServerConfig();
    EXPECT_EQ(server.host, "localhost");
}

TEST(ConfigManagerTest, GetSetKeyValue) {
    ConfigManager mgr("test_config_dir_kv");
    bool ok = mgr.set("test_key_kv", "test_value");
    EXPECT_TRUE(ok);
    auto val = mgr.get("test_key_kv");
    EXPECT_TRUE(val.has_value());
}

TEST(ConfigManagerTest, GetMissingKey) {
    ConfigManager mgr("test_config_dir");
    auto val = mgr.get("nonexistent_key_xyz");
    EXPECT_FALSE(val.has_value());
}

TEST(ConfigManagerTest, RemoveKey) {
    ConfigManager mgr("test_config_dir");
    mgr.set("to_remove", "value");
    EXPECT_TRUE(mgr.remove("to_remove"));
    EXPECT_FALSE(mgr.get("to_remove").has_value());
}

TEST(ConfigManagerTest, GetConfigDir) {
    ConfigManager mgr("my_config_dir");
    EXPECT_EQ(mgr.getConfigDir(), "my_config_dir");
}

TEST(ConfigManagerTest, ServerConfigRoundtrip) {
    ConfigManager mgr("test_config_dir");

    ServerConfig cfg;
    cfg.host = "example.com";
    cfg.port = 9999;
    EXPECT_TRUE(mgr.setServerConfig(cfg));

    auto restored = mgr.getServerConfig();
    EXPECT_EQ(restored.host, "example.com");
    EXPECT_EQ(restored.port, 9999);
}

TEST(ConfigManagerTest, AutoRunConfigRoundtrip) {
    ConfigManager mgr("test_config_dir");

    AutoRunConfig cfg;
    cfg.enabled = true;
    cfg.scriptPath = "test.lua";
    EXPECT_TRUE(mgr.setAutoRunConfig(cfg));

    auto restored = mgr.getAutoRunConfig();
    EXPECT_TRUE(restored.enabled);
    EXPECT_EQ(restored.scriptPath, "test.lua");
}

TEST(ConfigManagerTest, HeartbeatConfigRoundtrip) {
    ConfigManager mgr("test_config_dir");

    HeartbeatConfig cfg;
    cfg.enabled = false;
    cfg.intervalSeconds = 10;
    EXPECT_TRUE(mgr.setHeartbeatConfig(cfg));

    auto restored = mgr.getHeartbeatConfig();
    EXPECT_FALSE(restored.enabled);
    EXPECT_EQ(restored.intervalSeconds, 10);
}

TEST(ConfigManagerTest, GameConfigList) {
    ConfigManager mgr("test_config_dir");

    GameConfig game;
    game.name = "Game1";
    game.path = "/game1";

    EXPECT_TRUE(mgr.addGameConfig(game));

    auto games = mgr.getGameConfigList();
    EXPECT_GE(games.size(), 1u);

    EXPECT_TRUE(mgr.removeGameConfig("Game1"));
}
