#pragma once

#include <string>
#include <memory>
#include <vector>

namespace wingman::runtime {

// ========== 运行能力位标志 ==========
// 支持能力组合而非互斥模式
enum class RunCapability : uint32_t {
    None = 0,
    RemoteOutbound = 0x01,   // 主动连接 Go Orchestrator
    LocalIpc = 0x02,         // 本地 IPC 服务（供 UI 连接）
    StandaloneScript = 0x04, // 单机脚本执行
};

// 能力位运算（inline 避免 ODR 违规）
inline RunCapability operator|(RunCapability a, RunCapability b) {
    return static_cast<RunCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline RunCapability operator&(RunCapability a, RunCapability b) {
    return static_cast<RunCapability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasCapability(RunCapability capabilities, RunCapability flag) {
    return (static_cast<uint32_t>(capabilities) & static_cast<uint32_t>(flag)) != 0;
}

// ========== 运行模式（向后兼容） ==========
// 遗留用途，内部从能力派生
enum class RunMode {
    Unknown = 0,
    Remote,      // 主动连接 Go Orchestrator（遗留）
    Standalone,  // 单机模式（仅脚本，遗留）
    Hybrid,      // 混合模式：远端 + 本地 IPC
};

// ========== 配置结构 ==========

struct RemoteClientConfig {
    std::string serverIp = "127.0.0.1";
    int serverPort = 8888;
    int reconnectInterval = 5;      // 秒（退避基数）
    int maxReconnectInterval = 60;  // 秒（退避上限）
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
    // 能力配置（可组合）
    bool enableRemote = true;      // 启用远端 outbound 连接
    bool enableLocalIpc = true;    // 启用本地 IPC 服务器（供 UI 连接）
    bool enableStandaloneScript = false;  // 启用单机脚本执行

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

    // 获取能力位标志
    RunCapability getCapabilities() const;

    // 获取实际运行模式（向后兼容，从能力派生）
    RunMode getRunMode() const;
};

} // namespace wingman::runtime
