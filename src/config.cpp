#include "wingman/config.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>

namespace wingman {

// 获取配置文件路径
static std::filesystem::path getConfigFilePath(const std::string& configDir) {
    return std::filesystem::path(configDir) / "config.json";
}

// 加载完整的配置 JSON
static nlohmann::json loadConfigJson(const std::string& configDir) {
    std::filesystem::path configPath = getConfigFilePath(configDir);

    if (!std::filesystem::exists(configPath)) {
        return nlohmann::json::object();
    }

    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return nlohmann::json::object();
        }

        nlohmann::json j;
        file >> j;
        return j;
    } catch (...) {
        return nlohmann::json::object();
    }
}

// 保存完整的配置 JSON
static bool saveConfigJson(const std::string& configDir, const nlohmann::json& j) {
    try {
        std::filesystem::path configPath = getConfigFilePath(configDir);

        // 确保目录存在
        std::filesystem::create_directories(std::filesystem::path(configDir));

        std::ofstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(2);  // 缩进 2 格
        return true;
    } catch (...) {
        return false;
    }
}

// ========== ServerConfig Implementation ==========

std::string ServerConfig::toJson() const {
    nlohmann::json j;
    j["host"] = host;
    j["port"] = port;
    j["username"] = username;
    j["password"] = password;
    j["autoConnect"] = autoConnect;
    return j.dump();
}

ServerConfig ServerConfig::fromJson(const std::string& json) {
    ServerConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("host")) config.host = j["host"];
        if (j.contains("port")) config.port = j["port"];
        if (j.contains("username")) config.username = j["username"];
        if (j.contains("password")) config.password = j["password"];
        if (j.contains("autoConnect")) config.autoConnect = j["autoConnect"];
    } catch (...) {
        // 解析失败，返回默认配置
    }
    return config;
}

// ========== TrayConfig Implementation ==========

std::string TrayConfig::toJson() const {
    nlohmann::json j;
    j["minimizeToTray"] = minimizeToTray;
    j["startMinimized"] = startMinimized;
    j["showNotifications"] = showNotifications;
    return j.dump();
}

TrayConfig TrayConfig::fromJson(const std::string& json) {
    TrayConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("minimizeToTray")) config.minimizeToTray = j["minimizeToTray"];
        if (j.contains("startMinimized")) config.startMinimized = j["startMinimized"];
        if (j.contains("showNotifications")) config.showNotifications = j["showNotifications"];
    } catch (...) {
        // 解析失败，返回默认配置
    }
    return config;
}

// ========== AutoRunConfig Implementation ==========

std::string AutoRunConfig::toJson() const {
    nlohmann::json j;
    j["enabled"] = enabled;
    j["scriptPath"] = scriptPath;
    j["delaySeconds"] = delaySeconds;
    j["repeat"] = repeat;
    j["repeatInterval"] = repeatInterval;
    return j.dump();
}

AutoRunConfig AutoRunConfig::fromJson(const std::string& json) {
    AutoRunConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("enabled")) config.enabled = j["enabled"];
        if (j.contains("scriptPath")) config.scriptPath = j["scriptPath"];
        if (j.contains("delaySeconds")) config.delaySeconds = j["delaySeconds"];
        if (j.contains("repeat")) config.repeat = j["repeat"];
        if (j.contains("repeatInterval")) config.repeatInterval = j["repeatInterval"];
    } catch (...) {
        // 解析失败，返回默认配置
    }
    return config;
}

// ========== HeartbeatConfig Implementation ==========

std::string HeartbeatConfig::toJson() const {
    nlohmann::json j;
    j["enabled"] = enabled;
    j["intervalSeconds"] = intervalSeconds;
    j["timeoutSeconds"] = timeoutSeconds;
    return j.dump();
}

HeartbeatConfig HeartbeatConfig::fromJson(const std::string& json) {
    HeartbeatConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("enabled")) config.enabled = j["enabled"];
        if (j.contains("intervalSeconds")) config.intervalSeconds = j["intervalSeconds"];
        if (j.contains("timeoutSeconds")) config.timeoutSeconds = j["timeoutSeconds"];
    } catch (...) {
        // 解析失败，返回默认配置
    }
    return config;
}

// ========== ConfigManager Implementation ==========

class ConfigManager::Impl {
public:
    std::string configDir;

    Impl(const std::string& configDir) : configDir(configDir) {
        // 确保配置目录存在
        std::filesystem::create_directories(configDir);

        bool createdDefault = false;
        nlohmann::json config = loadConfigJson(configDir);

        // 如果配置文件为空或不存在，创建默认配置
        if (config.empty()) {
            config["server"] = {
                {"host", "localhost"},
                {"port", 8080},
                {"username", ""},
                {"password", ""},
                {"autoConnect", false},
                {"serverControlled", false}
            };
            config["tray"] = {
                {"minimizeToTray", true},
                {"startMinimized", false},
                {"showNotifications", true}
            };
            config["autoRun"] = {
                {"enabled", false},
                {"scriptPath", ""},
                {"delaySeconds", 0},
                {"repeat", false},
                {"repeatInterval", 0}
            };
            config["heartbeat"] = {
                {"enabled", true},
                {"intervalSeconds", 30},
                {"timeoutSeconds", 90}
            };
            createdDefault = true;
        }

        // 保存配置
        if (createdDefault) {
            saveConfigJson(configDir, config);
            std::cout << "[CONFIG] 已创建默认配置文件: " << configDir << "/config.json\n";
            std::cout.flush();
        }
    }
};

ConfigManager::ConfigManager(const std::string& configDir)
    : impl_(std::make_unique<Impl>(configDir)) {}

ConfigManager::~ConfigManager() = default;

ServerConfig ConfigManager::getServerConfig() const {
    nlohmann::json config = loadConfigJson(impl_->configDir);
    ServerConfig result;

    if (config.contains("server")) {
        auto& server = config["server"];
        if (server.contains("host")) result.host = server["host"];
        if (server.contains("port")) result.port = server["port"];
        if (server.contains("username")) result.username = server["username"];
        if (server.contains("password")) result.password = server["password"];
        if (server.contains("autoConnect")) result.autoConnect = server["autoConnect"];
        if (server.contains("serverControlled")) result.serverControlled = server["serverControlled"];
    }

    return result;
}

bool ConfigManager::setServerConfig(const ServerConfig& config) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    j["server"] = {
        {"host", config.host},
        {"port", config.port},
        {"username", config.username},
        {"password", config.password},
        {"autoConnect", config.autoConnect},
        {"serverControlled", config.serverControlled}
    };

    return saveConfigJson(impl_->configDir, j);
}

TrayConfig ConfigManager::getTrayConfig() const {
    nlohmann::json config = loadConfigJson(impl_->configDir);
    TrayConfig result;

    if (config.contains("tray")) {
        auto& tray = config["tray"];
        if (tray.contains("minimizeToTray")) result.minimizeToTray = tray["minimizeToTray"];
        if (tray.contains("startMinimized")) result.startMinimized = tray["startMinimized"];
        if (tray.contains("showNotifications")) result.showNotifications = tray["showNotifications"];
    }

    return result;
}

bool ConfigManager::setTrayConfig(const TrayConfig& config) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    j["tray"] = {
        {"minimizeToTray", config.minimizeToTray},
        {"startMinimized", config.startMinimized},
        {"showNotifications", config.showNotifications}
    };

    return saveConfigJson(impl_->configDir, j);
}

AutoRunConfig ConfigManager::getAutoRunConfig() const {
    nlohmann::json config = loadConfigJson(impl_->configDir);
    AutoRunConfig result;

    if (config.contains("autoRun")) {
        auto& autoRun = config["autoRun"];
        if (autoRun.contains("enabled")) result.enabled = autoRun["enabled"];
        if (autoRun.contains("scriptPath")) result.scriptPath = autoRun["scriptPath"];
        if (autoRun.contains("delaySeconds")) result.delaySeconds = autoRun["delaySeconds"];
        if (autoRun.contains("repeat")) result.repeat = autoRun["repeat"];
        if (autoRun.contains("repeatInterval")) result.repeatInterval = autoRun["repeatInterval"];
    }

    return result;
}

bool ConfigManager::setAutoRunConfig(const AutoRunConfig& config) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    j["autoRun"] = {
        {"enabled", config.enabled},
        {"scriptPath", config.scriptPath},
        {"delaySeconds", config.delaySeconds},
        {"repeat", config.repeat},
        {"repeatInterval", config.repeatInterval}
    };

    return saveConfigJson(impl_->configDir, j);
}

HeartbeatConfig ConfigManager::getHeartbeatConfig() const {
    nlohmann::json config = loadConfigJson(impl_->configDir);
    HeartbeatConfig result;

    if (config.contains("heartbeat")) {
        auto& heartbeat = config["heartbeat"];
        if (heartbeat.contains("enabled")) result.enabled = heartbeat["enabled"];
        if (heartbeat.contains("intervalSeconds")) result.intervalSeconds = heartbeat["intervalSeconds"];
        if (heartbeat.contains("timeoutSeconds")) result.timeoutSeconds = heartbeat["timeoutSeconds"];
    }

    return result;
}

bool ConfigManager::setHeartbeatConfig(const HeartbeatConfig& config) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    j["heartbeat"] = {
        {"enabled", config.enabled},
        {"intervalSeconds", config.intervalSeconds},
        {"timeoutSeconds", config.timeoutSeconds}
    };

    return saveConfigJson(impl_->configDir, j);
}

std::optional<std::string> ConfigManager::get(const std::string& key) const {
    nlohmann::json config = loadConfigJson(impl_->configDir);

    if (config.contains(key)) {
        return config[key].dump();
    }

    return std::nullopt;
}

bool ConfigManager::set(const std::string& key, const std::string& value) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    try {
        j[key] = nlohmann::json::parse(value);
    } catch (...) {
        j[key] = value;
    }

    return saveConfigJson(impl_->configDir, j);
}

bool ConfigManager::remove(const std::string& key) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    if (j.contains(key)) {
        j.erase(key);
        return saveConfigJson(impl_->configDir, j);
    }

    return false;
}

bool ConfigManager::save() {
    // 直接 JSON 模式下总是实时保存
    return true;
}

bool ConfigManager::load() {
    // 直接 JSON 模式下总是实时加载
    return true;
}

const std::string& ConfigManager::getConfigDir() const {
    return impl_->configDir;
}

} // namespace wingman
