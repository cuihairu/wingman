#pragma once

#include <string>
#include <memory>
#include <vector>

namespace wingman::runtime {

// ========== 运行模式 ==========

enum class RunMode {
    Unknown = 0,
    Remote,      // 主动连接 Go Orchestrator
    Standalone,  // 单机模式（仅脚本）
};

// ========== 配置结构 ==========

struct RemoteClientConfig {
    std::string serverIp = "127.0.0.1";
    int serverPort = 9527;
    int reconnectInterval = 5;      // 秒
    int heartbeatInterval = 30;     // 秒
    int connectTimeout = 10;        // 秒
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
    bool enableRemote = true;

    RemoteClientConfig remoteClient;
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

} // namespace wingman::runtime
