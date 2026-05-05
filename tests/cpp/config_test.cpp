#include <gtest/gtest.h>
#include "wingman/config.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace wingman;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试使用独立的配置目录
        testDir = "test_config_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void TearDown() override {
        // 清理测试目录
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }

    std::string testDir;
};


// ========== ServerConfig Tests ==========

TEST(ServerConfigTest, DefaultValues) {
    ServerConfig config;

    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 8080);
    EXPECT_TRUE(config.username.empty());
    EXPECT_TRUE(config.password.empty());
    EXPECT_FALSE(config.autoConnect);
}

TEST(ServerConfigTest, ToJson_FromJson) {
    ServerConfig original;
    original.host = "192.168.1.100";
    original.port = 9000;
    original.username = "admin";
    original.password = "secret123";
    original.autoConnect = true;

    // 转换为 JSON
    std::string json = original.toJson();
    ASSERT_FALSE(json.empty());

    // 从 JSON 恢复
    ServerConfig restored = ServerConfig::fromJson(json);

    EXPECT_EQ(restored.host, "192.168.1.100");
    EXPECT_EQ(restored.port, 9000);
    EXPECT_EQ(restored.username, "admin");
    EXPECT_EQ(restored.password, "secret123");
    EXPECT_TRUE(restored.autoConnect);
}

TEST(ServerConfigTest, IsValid) {
    ServerConfig config;

    // 默认配置应该有效
    EXPECT_TRUE(config.isValid());

    // 空主机名无效
    config.host = "";
    EXPECT_FALSE(config.isValid());

    // 恢复主机名
    config.host = "localhost";

    // 端口超出范围
    config.port = 0;
    EXPECT_FALSE(config.isValid());

    config.port = 70000;
    EXPECT_FALSE(config.isValid());

    // 有效端口
    config.port = 8080;
    EXPECT_TRUE(config.isValid());
}


// ========== TrayConfig Tests ==========

TEST(TrayConfigTest, DefaultValues) {
    TrayConfig config;

    EXPECT_TRUE(config.minimizeToTray);
    EXPECT_FALSE(config.startMinimized);
    EXPECT_TRUE(config.showNotifications);
}

TEST(TrayConfigTest, ToJson_FromJson) {
    TrayConfig original;
    original.minimizeToTray = false;
    original.startMinimized = true;
    original.showNotifications = false;

    std::string json = original.toJson();
    ASSERT_FALSE(json.empty());

    TrayConfig restored = TrayConfig::fromJson(json);

    EXPECT_FALSE(restored.minimizeToTray);
    EXPECT_TRUE(restored.startMinimized);
    EXPECT_FALSE(restored.showNotifications);
}


// ========== ConfigManager Tests ==========

TEST_F(ConfigTest, ConfigManager_CreateDefaultConfig) {
    ConfigManager manager(testDir);

    // 检查配置文件是否创建
    std::filesystem::path configPath = std::filesystem::path(testDir) / "config.json";
    EXPECT_TRUE(std::filesystem::exists(configPath));

    // 检查文件内容
    std::ifstream file(configPath);
    ASSERT_TRUE(file.is_open());

    nlohmann::json j;
    file >> j;

    EXPECT_TRUE(j.contains("server"));
    EXPECT_TRUE(j.contains("tray"));
}

TEST_F(ConfigTest, ConfigManager_GetDefaultServerConfig) {
    ConfigManager manager(testDir);
    ServerConfig config = manager.getServerConfig();

    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 8080);
    EXPECT_FALSE(config.autoConnect);
}

TEST_F(ConfigTest, ConfigManager_GetDefaultTrayConfig) {
    ConfigManager manager(testDir);
    TrayConfig config = manager.getTrayConfig();

    EXPECT_TRUE(config.minimizeToTray);
    EXPECT_FALSE(config.startMinimized);
    EXPECT_TRUE(config.showNotifications);
}

TEST_F(ConfigTest, ConfigManager_SetServerConfig) {
    ConfigManager manager(testDir);

    ServerConfig newConfig;
    newConfig.host = "example.com";
    newConfig.port = 9999;
    newConfig.username = "testuser";
    newConfig.password = "testpass";
    newConfig.autoConnect = true;

    EXPECT_TRUE(manager.setServerConfig(newConfig));

    // 重新创建 ConfigManager，验证持久化
    ConfigManager manager2(testDir);
    ServerConfig restored = manager2.getServerConfig();

    EXPECT_EQ(restored.host, "example.com");
    EXPECT_EQ(restored.port, 9999);
    EXPECT_EQ(restored.username, "testuser");
    EXPECT_EQ(restored.password, "testpass");
    EXPECT_TRUE(restored.autoConnect);
}

TEST_F(ConfigTest, ConfigManager_SetTrayConfig) {
    ConfigManager manager(testDir);

    TrayConfig newConfig;
    newConfig.minimizeToTray = false;
    newConfig.startMinimized = true;
    newConfig.showNotifications = false;

    EXPECT_TRUE(manager.setTrayConfig(newConfig));

    // 重新创建 ConfigManager，验证持久化
    ConfigManager manager2(testDir);
    TrayConfig restored = manager2.getTrayConfig();

    EXPECT_FALSE(restored.minimizeToTray);
    EXPECT_TRUE(restored.startMinimized);
    EXPECT_FALSE(restored.showNotifications);
}

TEST_F(ConfigTest, ConfigManager_GenericGetSet) {
    ConfigManager manager(testDir);

    // 设置自定义值
    EXPECT_TRUE(manager.set("custom_key", "custom_value"));

    // 获取值
    auto value = manager.get("custom_key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "\"custom_value\"");  // JSON 字符串格式

    // 设置 JSON 值
    EXPECT_TRUE(manager.set("number", "123"));
    auto numValue = manager.get("number");
    ASSERT_TRUE(numValue.has_value());

    // 解析 JSON
    nlohmann::json j = nlohmann::json::parse(numValue.value());
    EXPECT_EQ(j.get<int>(), 123);
}

TEST_F(ConfigTest, ConfigManager_Remove) {
    ConfigManager manager(testDir);

    manager.set("temp_key", "temp_value");
    EXPECT_TRUE(manager.get("temp_key").has_value());

    // 删除
    EXPECT_TRUE(manager.remove("temp_key"));
    EXPECT_FALSE(manager.get("temp_key").has_value());

    // 删除不存在的键
    EXPECT_FALSE(manager.remove("nonexistent"));
}

TEST_F(ConfigTest, ConfigManager_Persistence) {
    {
        ConfigManager manager(testDir);
        ServerConfig config;
        config.host = "persistent.example.com";
        config.port = 7777;
        manager.setServerConfig(config);
        manager.set("custom", "value");
    }

    {
        // 新实例应该能读取之前保存的数据
        ConfigManager manager(testDir);
        ServerConfig config = manager.getServerConfig();

        EXPECT_EQ(config.host, "persistent.example.com");
        EXPECT_EQ(config.port, 7777);

        auto custom = manager.get("custom");
        ASSERT_TRUE(custom.has_value());
    }
}

TEST_F(ConfigTest, ConfigManager_ConfigFileFormat) {
    ConfigManager manager(testDir);

    ServerConfig serverConfig;
    serverConfig.host = "test.example.com";
    serverConfig.port = 8888;
    serverConfig.username = "user";
    serverConfig.autoConnect = true;
    manager.setServerConfig(serverConfig);

    TrayConfig trayConfig;
    trayConfig.minimizeToTray = false;
    trayConfig.startMinimized = true;
    manager.setTrayConfig(trayConfig);

    // 读取文件并验证格式
    std::filesystem::path configPath = std::filesystem::path(testDir) / "config.json";
    std::ifstream file(configPath);
    ASSERT_TRUE(file.is_open());

    nlohmann::json j;
    file >> j;

    // 验证结构
    EXPECT_TRUE(j.is_object());
    EXPECT_TRUE(j.contains("server"));
    EXPECT_TRUE(j.contains("tray"));

    // 验证 server 对象
    EXPECT_TRUE(j["server"].is_object());
    EXPECT_EQ(j["server"]["host"].get<std::string>(), "test.example.com");
    EXPECT_EQ(j["server"]["port"].get<int>(), 8888);
    EXPECT_EQ(j["server"]["username"].get<std::string>(), "user");
    EXPECT_TRUE(j["server"]["autoConnect"].get<bool>());

    // 验证 tray 对象
    EXPECT_TRUE(j["tray"].is_object());
    EXPECT_FALSE(j["tray"]["minimizeToTray"].get<bool>());
    EXPECT_TRUE(j["tray"]["startMinimized"].get<bool>());
}
