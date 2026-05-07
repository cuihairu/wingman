#pragma once

#include <string>
#include <memory>

namespace wingman::agent {

// ========== 运行模式 ==========

enum class RunMode {
    Unknown = 0,
    Active,      // 主动连接 Server
    Passive,     // 被动监听端口
    Standalone,  // 单机模式（仅脚本）
    Both         // 同时主动+被动（双保险）
};

// ========== 配置结构 ==========

struct ActiveModeConfig {
    std::string serverIp = "127.0.0.1";
    int serverPort = 9527;
    int reconnectInterval = 5;      // 秒
    int heartbeatInterval = 30;     // 秒
    int connectTimeout = 10;        // 秒
};

struct PassiveModeConfig {
    std::string listenIp = "0.0.0.0";
    int listenPort = 9528;
    int maxConnections = 10;
};

struct StandaloneModeConfig {
    std::string scriptDir = "./scripts";
    std::vector<std::string> autoStart;
};

struct DebuggerConfig {
    bool enable = true;
    int listenPort = 9966;
    bool waitForIde = false;
};

struct LoggingConfig {
    std::string level = "info";
    std::string file = "wingman-agent.log";
    bool console = true;
};

struct PerformanceConfig {
    int screenshotCacheSize = 10;
    int matchThreadPoolSize = 4;
    int memoryLimitMb = 512;
};

struct AgentConfig {
    // 模式配置
    bool enableActive = true;
    bool enablePassive = true;
    std::string connectionStrategy = "parallel";  // fallback | parallel | primary

    ActiveModeConfig active;
    PassiveModeConfig passive;
    StandaloneModeConfig standalone;
    DebuggerConfig debugger;
    LoggingConfig logging;
    PerformanceConfig performance;

    // 加载配置文件
    static AgentConfig loadFromFile(const std::string& path);

    // 加载配置字符串（TOML）
    static AgentConfig loadFromString(const std::string& toml);

    // 保存到文件
    bool saveToFile(const std::string& path) const;

    // 获取实际运行模式
    RunMode getRunMode() const;
};

} // namespace wingman::agent
