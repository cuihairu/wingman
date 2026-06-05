#include <gtest/gtest.h>
#include "wingman/config.hpp"
#include <filesystem>
#include <fstream>

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

// ========== Extended Struct Serialization Tests ==========

TEST(ServerConfigTest, FromJsonInvalidInput) {
    auto cfg = ServerConfig::fromJson("not valid json {{{");
    // Should return defaults, not crash
    EXPECT_EQ(cfg.host, "localhost");
    EXPECT_EQ(cfg.port, 9527);
}

TEST(ServerConfigTest, FromJsonEmptyObject) {
    auto cfg = ServerConfig::fromJson("{}");
    EXPECT_EQ(cfg.host, "localhost");
    EXPECT_EQ(cfg.port, 9527);
}

TEST(ServerConfigTest, ServerControlledField) {
    ServerConfig original;
    original.host = "10.0.0.1";
    original.port = 443;
    original.password = "secret";
    original.serverControlled = true;

    std::string json = original.toJson();
    auto restored = ServerConfig::fromJson(json);
    EXPECT_TRUE(restored.serverControlled);
    EXPECT_EQ(restored.password, "secret");
}

TEST(ServerConfigTest, IsValidPortBoundary) {
    ServerConfig cfg;
    cfg.host = "localhost";
    cfg.port = 1;
    EXPECT_TRUE(cfg.isValid());

    cfg.port = 65535;
    EXPECT_TRUE(cfg.isValid());

    cfg.port = -1;
    EXPECT_FALSE(cfg.isValid());
}

TEST(AutoRunConfigTest, FromJsonInvalidInput) {
    auto cfg = AutoRunConfig::fromJson("}}broken");
    EXPECT_FALSE(cfg.enabled);
    EXPECT_EQ(cfg.delaySeconds, 0);
}

TEST(HeartbeatConfigTest, FromJsonInvalidInput) {
    auto cfg = HeartbeatConfig::fromJson("not json at all");
    EXPECT_TRUE(cfg.enabled);
    EXPECT_EQ(cfg.intervalSeconds, 30);
}

TEST(GameConfigTest, FullFieldRoundtrip) {
    GameConfig original;
    original.name = "FullGame";
    original.path = "/games/full";
    original.args = "-debug -log";
    original.workingDir = "/games";
    original.autoStart = true;
    original.scriptPath = "scripts/full.lua";
    original.windowTitle = "Full Window";
    original.delaySeconds = 10;
    original.autoRestart = true;
    original.restartDelay = 5;
    original.maxRestarts = 10;
    original.restartCount = 2;

    std::string json = original.toJson();
    auto restored = GameConfig::fromJson(json);

    EXPECT_EQ(restored.name, "FullGame");
    EXPECT_EQ(restored.path, "/games/full");
    EXPECT_EQ(restored.args, "-debug -log");
    EXPECT_EQ(restored.workingDir, "/games");
    EXPECT_TRUE(restored.autoStart);
    EXPECT_EQ(restored.scriptPath, "scripts/full.lua");
    EXPECT_EQ(restored.windowTitle, "Full Window");
    EXPECT_EQ(restored.delaySeconds, 10);
    EXPECT_TRUE(restored.autoRestart);
    EXPECT_EQ(restored.restartDelay, 5);
    EXPECT_EQ(restored.maxRestarts, 10);
    EXPECT_EQ(restored.restartCount, 2);
}

TEST(GameConfigTest, FromJsonInvalidInput) {
    auto cfg = GameConfig::fromJson("broken json {{{");
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_EQ(cfg.delaySeconds, 5);
}

// ========== Extended ConfigManager Tests ==========

TEST(ConfigManagerTest, SetJsonValueParsed) {
    ConfigManager mgr("test_config_dir_json");
    bool ok = mgr.set("json_key", R"({"nested":true,"count":42})");
    EXPECT_TRUE(ok);

    auto val = mgr.get("json_key");
    EXPECT_TRUE(val.has_value());
    // The stored value should be a parsed JSON object
    EXPECT_NE(val->find("nested"), std::string::npos);
    EXPECT_NE(val->find("42"), std::string::npos);
}

TEST(ConfigManagerTest, RemoveMissingKeyReturnsFalse) {
    ConfigManager mgr("test_config_dir_rm");
    bool result = mgr.remove("absolutely_nonexistent_key_xyz_999");
    EXPECT_FALSE(result);
}

TEST(ConfigManagerTest, RemoveGameConfigNonexistent) {
    ConfigManager mgr("test_config_dir_games");
    bool result = mgr.removeGameConfig("nonexistent_game_xyz");
    EXPECT_FALSE(result);
}

TEST(ConfigManagerTest, AddGameConfigUpdatesExisting) {
    ConfigManager mgr("test_config_dir_update");

    GameConfig game1;
    game1.name = "UpdateGame";
    game1.path = "/original";
    EXPECT_TRUE(mgr.addGameConfig(game1));

    GameConfig game2;
    game2.name = "UpdateGame";
    game2.path = "/updated";
    EXPECT_TRUE(mgr.addGameConfig(game2));

    auto games = mgr.getGameConfigList();
    int count = 0;
    for (const auto& g : games) {
        if (g.name == "UpdateGame") {
            EXPECT_EQ(g.path, "/updated");
            ++count;
        }
    }
    EXPECT_EQ(count, 1);

    mgr.removeGameConfig("UpdateGame");
}

TEST(ConfigManagerTest, SaveAndLoadAlwaysReturnTrue) {
    ConfigManager mgr("test_config_dir_sl");
    EXPECT_TRUE(mgr.save());
    EXPECT_TRUE(mgr.load());
}

TEST(ConfigManagerTest, FileRoundtripWithTempDir) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_roundtrip";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    {
        ConfigManager mgr(tempDir.string());
        ServerConfig srv;
        srv.host = "roundtrip.test";
        srv.port = 7777;
        srv.username = "rtuser";
        srv.autoConnect = true;
        EXPECT_TRUE(mgr.setServerConfig(srv));

        AutoRunConfig ar;
        ar.enabled = true;
        ar.scriptPath = "rt_script.lua";
        ar.delaySeconds = 3;
        EXPECT_TRUE(mgr.setAutoRunConfig(ar));
    }

    // Re-create manager from the same directory
    {
        ConfigManager mgr(tempDir.string());
        auto srv = mgr.getServerConfig();
        EXPECT_EQ(srv.host, "roundtrip.test");
        EXPECT_EQ(srv.port, 7777);
        EXPECT_EQ(srv.username, "rtuser");
        EXPECT_TRUE(srv.autoConnect);

        auto ar = mgr.getAutoRunConfig();
        EXPECT_TRUE(ar.enabled);
        EXPECT_EQ(ar.scriptPath, "rt_script.lua");
        EXPECT_EQ(ar.delaySeconds, 3);
    }

    std::filesystem::remove_all(tempDir);
}

TEST(ConfigManagerTest, SetNonJsonValueStoredAsString) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_setstr";
    std::filesystem::remove_all(tempDir);
    ConfigManager mgr(tempDir.string());

    // Plain string (not valid JSON) should be stored as-is
    bool ok = mgr.set("plain_key", "just a plain string");
    EXPECT_TRUE(ok);

    auto val = mgr.get("plain_key");
    ASSERT_TRUE(val.has_value());
    // The value is stored as a JSON string, so it will be quoted
    EXPECT_NE(val->find("just a plain string"), std::string::npos);

    std::filesystem::remove_all(tempDir);
}

TEST(ConfigManagerTest, GetGameConfigListEmpty) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_emptygames";
    std::filesystem::remove_all(tempDir);
    ConfigManager mgr(tempDir.string());

    auto games = mgr.getGameConfigList();
    // Default config has empty games array
    EXPECT_TRUE(games.empty());

    std::filesystem::remove_all(tempDir);
}

TEST(ConfigManagerTest, WriteGameConfigList) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_writegames";
    std::filesystem::remove_all(tempDir);
    ConfigManager mgr(tempDir.string());

    std::vector<GameConfig> games;
    GameConfig g1;
    g1.name = "Game1";
    g1.path = "/g1";
    games.push_back(g1);
    GameConfig g2;
    g2.name = "Game2";
    g2.path = "/g2";
    games.push_back(g2);

    EXPECT_TRUE(mgr.writeGameConfigList(games));

    auto loaded = mgr.getGameConfigList();
    EXPECT_EQ(loaded.size(), 2u);

    std::filesystem::remove_all(tempDir);
}

TEST(ConfigManagerTest, ExistingConfigFileIsPreserved) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_existing";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // Write a config file manually
    std::ofstream f((tempDir / "config.json").string());
    f << R"({"server":{"host":"custom.host","port":1234},"games":[]})";
    f.close();

    // Create ConfigManager — should load existing config, not overwrite
    ConfigManager mgr(tempDir.string());
    auto srv = mgr.getServerConfig();
    EXPECT_EQ(srv.host, "custom.host");
    EXPECT_EQ(srv.port, 1234);

    std::filesystem::remove_all(tempDir);
}

TEST(ConfigManagerTest, CorruptedConfigFileReturnsDefaults) {
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_config_test_corrupted";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);

    // Write invalid JSON to trigger catch block in loadConfigJson
    {
        std::ofstream f((tempDir / "config.json").string());
        f << "{{{{not valid json";
    }

    ConfigManager mgr(tempDir.string());
    // Should return defaults, not crash
    auto srv = mgr.getServerConfig();
    EXPECT_EQ(srv.host, "localhost");
    EXPECT_EQ(srv.port, 9527);

    std::filesystem::remove_all(tempDir);
}
