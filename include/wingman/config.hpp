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
    bool serverControlled = false;  // 服务器控制模式：允许远程下发脚本

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

// 自动运行配置
struct AutoRunConfig {
    bool enabled = false;           // 是否启用自动运行
    std::string scriptPath;         // 脚本路径
    int delaySeconds = 0;           // 启动延迟（秒）
    bool repeat = false;            // 是否重复运行
    int repeatInterval = 0;         // 重复间隔（秒），0 表示脚本自己控制循环

    std::string toJson() const;
    static AutoRunConfig fromJson(const std::string& json);
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

    // 自动运行配置
    AutoRunConfig getAutoRunConfig() const;
    bool setAutoRunConfig(const AutoRunConfig& config);

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
