#pragma once

#include <string>
#include <optional>
#include <memory>

namespace wingman {

// 服务器连接配置
struct ServerConfig {
    std::string host = "localhost";
    int port = 8080;
    std::string username;
    std::string password;  // 注意：实际使用时应该加密存储
    bool autoConnect = false;

    // 转换为 JSON
    std::string toJson() const;

    // 从 JSON 解析
    static ServerConfig fromJson(const std::string& json);

    // 验证配置
    bool isValid() const { return !host.empty() && port > 0 && port <= 65535; }
};

// 托盘配置
struct TrayConfig {
    bool minimizeToTray = true;
    bool startMinimized = false;
    bool showNotifications = true;

    std::string toJson() const;
    static TrayConfig fromJson(const std::string& json);
};

// 通用配置管理器
class ConfigManager {
public:
    explicit ConfigManager(const std::string& configDir = "config");
    ~ConfigManager();

    // 禁止拷贝
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // 服务器配置
    ServerConfig getServerConfig() const;
    bool setServerConfig(const ServerConfig& config);

    // 托盘配置
    TrayConfig getTrayConfig() const;
    bool setTrayConfig(const TrayConfig& config);

    // 通用键值对访问
    std::optional<std::string> get(const std::string& key) const;
    bool set(const std::string& key, const std::string& value);
    bool remove(const std::string& key);

    // 保存到磁盘
    bool save();

    // 从磁盘重新加载
    bool load();

    // 获取配置目录
    const std::string& getConfigDir() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
