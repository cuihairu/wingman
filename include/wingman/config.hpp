#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>

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

// 托盘菜单项动作类型
enum class TrayActionType {
    none,           // 无动作
    command,        // 执行命令
    lua,            // 执行 Lua 脚本
    startGame,      // 启动游戏
    http,           // HTTP 请求
    callback        // C++ 回调（内部使用）
};

// 托盘菜单项配置
struct TrayMenuItemConfig {
    std::string id;                     // 菜单项 ID
    std::string label;                  // 显示文本
    TrayActionType actionType = TrayActionType::none;  // 动作类型
    std::string action;                 // 动作参数（命令/Lua脚本/URL等）
    bool enabled = true;                // 是否启用
    bool isSeparator = false;           // 是否为分隔符
    std::vector<TrayMenuItemConfig> subitems;  // 子菜单项

    std::string toJson() const;
    static TrayMenuItemConfig fromJson(const std::string& json);
};

// 托盘图标状态
enum class TrayIconState {
    normal,    // 正常（正在运行）
    idle,      // 空闲（无任务）
    disabled,  // 禁用（变灰）
    busy,      // 忙碌
    error      // 错误
};

// 托盘配置
struct TrayConfig {
    bool minimizeToTray = true;
    bool startMinimized = false;
    bool showNotifications = true;
    std::string iconPath;               // 默认图标路径
    std::string tooltip = "Wingman";    // 提示文本
    std::vector<TrayMenuItemConfig> menuItems;  // 预设菜单项

    // 不同状态的图标路径
    std::string iconNormal;             // 正常状态图标
    std::string iconIdle;               // 空闲状态图标（灰色）
    std::string iconBusy;               // 忙碌状态图标
    std::string iconError;              // 错误状态图标

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

// 心跳配置
struct HeartbeatConfig {
    bool enabled = true;            // 是否启用心跳
    int intervalSeconds = 30;       // 心跳间隔（秒）
    int timeoutSeconds = 90;        // 超时时间（秒），服务器超过此时间未收到心跳认为节点离线

    std::string toJson() const;
    static HeartbeatConfig fromJson(const std::string& json);
};

// 游戏配置
struct GameConfig {
    std::string name;               // 游戏名称
    std::string path;               // 游戏可执行文件路径
    std::string args;               // 启动参数
    std::string workingDir;         // 工作目录
    bool autoStart = false;         // 是否自动启动游戏
    std::string scriptPath;         // 关联的自动脚本
    std::string windowTitle;        // 窗口标题（用于检测）
    int delaySeconds = 5;           // 游戏启动后等待时间（秒）
    bool autoRestart = false;       // 游戏关闭后是否自动重启
    int restartDelay = 10;          // 重启延迟（秒）
    int maxRestarts = 3;            // 最大重启次数（0 = 无限）
    int restartCount = 0;           // 当前重启次数（内部使用）

    std::string toJson() const;
    static GameConfig fromJson(const std::string& json);

    bool isValid() const { return !path.empty(); }
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

    // 心跳配置
    HeartbeatConfig getHeartbeatConfig() const;
    bool setHeartbeatConfig(const HeartbeatConfig& config);

    // 游戏配置
    std::vector<GameConfig> getGameConfigList() const;
    bool writeGameConfigList(const std::vector<GameConfig>& games);
    bool addGameConfig(const GameConfig& game);
    bool removeGameConfig(const std::string& name);

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
