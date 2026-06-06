#include "wingman/runtime/config.hpp"
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace wingman::runtime {

// ========== AgentConfig 实现 ==========

RunMode AgentConfig::getRunMode() const {
    return enableRemote ? RunMode::Remote : RunMode::Standalone;
}

AgentConfig AgentConfig::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str());
}

AgentConfig AgentConfig::loadFromString(const std::string& toml) {
    AgentConfig config;

    // 简单解析（临时实现，应该使用 toml++ 库）
    std::istringstream stream(toml);
    std::string line;

    while (std::getline(stream, line)) {
        // 跳过空行和注释
        if (line.empty() || line[0] == '#') continue;

        // 解析 key = value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 去除空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 解析布尔值
            if (value == "true") {
                if (key == "enable_remote") config.enableRemote = true;
                if (key == "wait_for_ide") config.debugger.waitForIde = true;
                if (key == "enable") config.debugger.enable = true;
                if (key == "console") config.logging.console = true;
            } else if (value == "false") {
                if (key == "enable_remote") config.enableRemote = false;
                if (key == "wait_for_ide") config.debugger.waitForIde = false;
                if (key == "enable") config.debugger.enable = false;
                if (key == "console") config.logging.console = false;
            }
            // 解析整数
            else if (value.find_first_not_of("0123456789") == std::string::npos) {
                int intValue = std::stoi(value);
                if (key == "server_port") config.remoteClient.serverPort = intValue;
                if (key == "listen_port") config.debugger.listenPort = intValue;
                if (key == "reconnect_interval") config.remoteClient.reconnectInterval = intValue;
                if (key == "heartbeat_interval") config.remoteClient.heartbeatInterval = intValue;
                if (key == "connect_timeout") config.remoteClient.connectTimeout = intValue;
                if (key == "screenshot_cache_size") config.performance.screenshotCacheSize = intValue;
                if (key == "match_thread_pool_size") config.performance.matchThreadPoolSize = intValue;
                if (key == "memory_limit_mb") config.performance.memoryLimitMb = intValue;
            }
            // 解析字符串
            else {
                if (key == "server_ip") config.remoteClient.serverIp = value;
                if (key == "script_dir") config.standalone.scriptDir = value;
                if (key == "level") config.logging.level = value;
                if (key == "file") config.logging.file = value;
            }
        }
    }

    return config;
}

bool AgentConfig::saveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << "# Wingman Agent 配置文件\n\n";

    file << "# ========== 运行模式配置 ==========\n";
    file << "enable_remote = " << (enableRemote ? "true" : "false") << "\n\n";

    file << "# ========== 远程模式配置 ==========\n";
    file << "[remote]\n";
    file << "server_ip = \"" << remoteClient.serverIp << "\"\n";
    file << "server_port = " << remoteClient.serverPort << "\n";
    file << "reconnect_interval = " << remoteClient.reconnectInterval << "\n";
    file << "heartbeat_interval = " << remoteClient.heartbeatInterval << "\n";
    file << "connect_timeout = " << remoteClient.connectTimeout << "\n\n";

    file << "# ========== 单机模式配置 ==========\n";
    file << "[standalone]\n";
    file << "script_dir = \"" << standalone.scriptDir << "\"\n\n";

    file << "# ========== 调试器配置 ==========\n";
    file << "[debugger]\n";
    file << "enable = " << (debugger.enable ? "true" : "false") << "\n";
    file << "listen_port = " << debugger.listenPort << "\n";
    file << "wait_for_ide = " << (debugger.waitForIde ? "true" : "false") << "\n\n";

    file << "# ========== 日志配置 ==========\n";
    file << "[logging]\n";
    file << "level = \"" << logging.level << "\"\n";
    file << "file = \"" << logging.file << "\"\n";
    file << "console = " << (logging.console ? "true" : "false") << "\n\n";

    return file.good();
}

} // namespace wingman::runtime
