#include "wingman/config.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <algorithm>

namespace wingman {

// Get config file path
static std::filesystem::path getConfigFilePath(const std::string& configDir) {
    return std::filesystem::path(configDir) / "config.json";
}

// Load full config JSON
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

// Save full config JSON
static bool saveConfigJson(const std::string& configDir, const nlohmann::json& j) {
    try {
        std::filesystem::path configPath = getConfigFilePath(configDir);

        // Ensure directory exists
        std::filesystem::create_directories(std::filesystem::path(configDir));

        std::ofstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(2);  // Indent 2 spaces
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
    j["serverControlled"] = serverControlled;
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
        if (j.contains("serverControlled")) config.serverControlled = j["serverControlled"];
    } catch (...) {
        // Parse failed, return default config
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
        // Parse failed, return default config
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
        // Parse failed, return default config
    }
    return config;
}

// ========== GameConfig Implementation ==========

std::string GameConfig::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["path"] = path;
    j["args"] = args;
    j["workingDir"] = workingDir;
    j["autoStart"] = autoStart;
    j["scriptPath"] = scriptPath;
    j["windowTitle"] = windowTitle;
    j["delaySeconds"] = delaySeconds;
    j["autoRestart"] = autoRestart;
    j["restartDelay"] = restartDelay;
    j["maxRestarts"] = maxRestarts;
    j["restartCount"] = restartCount;
    return j.dump();
}

GameConfig GameConfig::fromJson(const std::string& json) {
    GameConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("name")) config.name = j["name"];
        if (j.contains("path")) config.path = j["path"];
        if (j.contains("args")) config.args = j["args"];
        if (j.contains("workingDir")) config.workingDir = j["workingDir"];
        if (j.contains("autoStart")) config.autoStart = j["autoStart"];
        if (j.contains("scriptPath")) config.scriptPath = j["scriptPath"];
        if (j.contains("windowTitle")) config.windowTitle = j["windowTitle"];
        if (j.contains("delaySeconds")) config.delaySeconds = j["delaySeconds"];
        if (j.contains("autoRestart")) config.autoRestart = j["autoRestart"];
        if (j.contains("restartDelay")) config.restartDelay = j["restartDelay"];
        if (j.contains("maxRestarts")) config.maxRestarts = j["maxRestarts"];
        if (j.contains("restartCount")) config.restartCount = j["restartCount"];
    } catch (...) {
        // Parse failed, return default config
    }
    return config;
}

// ========== ConfigManager Implementation ==========

class ConfigManager::Impl {
public:
    std::string configDir;

    Impl(const std::string& configDir) : configDir(configDir) {
        // Ensure config directory exists
        std::filesystem::create_directories(configDir);

        bool createdDefault = false;
        nlohmann::json config = loadConfigJson(configDir);

        // If config file is empty or missing, create default config
        if (config.empty()) {
            config["server"] = {
                {"host", "localhost"},
                {"port", 9527},
                {"username", ""},
                {"password", ""},
                {"autoConnect", false},
                {"serverControlled", false}
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
            config["games"] = nlohmann::json::array();  // Empty game list
            createdDefault = true;
        }

        // Save config
        if (createdDefault) {
            saveConfigJson(configDir, config);
            std::cout << "[CONFIG] Created default config file: " << configDir << "/config.json\n";
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

std::vector<GameConfig> ConfigManager::getGameConfigList() const {
    nlohmann::json config = loadConfigJson(impl_->configDir);
    std::vector<GameConfig> result;

    if (config.contains("games") && config["games"].is_array()) {
        for (const auto& gameJson : config["games"]) {
            result.push_back(GameConfig::fromJson(gameJson.dump()));
        }
    }

    return result;
}

bool ConfigManager::writeGameConfigList(const std::vector<GameConfig>& games) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    nlohmann::json gamesArray = nlohmann::json::array();
    for (const auto& game : games) {
        gamesArray.push_back(nlohmann::json::parse(game.toJson()));
    }
    j["games"] = gamesArray;

    return saveConfigJson(impl_->configDir, j);
}

bool ConfigManager::addGameConfig(const GameConfig& game) {
    auto games = getGameConfigList();

    // Check if game with same name already exists
    for (auto& existing : games) {
        if (existing.name == game.name) {
            existing = game;  // Update existing config
            return writeGameConfigList(games);
        }
    }

    // Add new game
    games.push_back(game);
    return writeGameConfigList(games);
}

bool ConfigManager::removeGameConfig(const std::string& name) {
    auto games = getGameConfigList();

    auto it = std::remove_if(games.begin(), games.end(),
        [&name](const GameConfig& game) { return game.name == name; });

    if (it != games.end()) {
        games.erase(it, games.end());
        return writeGameConfigList(games);
    }

    return false;
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
    // Direct JSON mode always saves in real-time
    return true;
}

bool ConfigManager::load() {
    // Direct JSON mode always loads in real-time
    return true;
}

const std::string& ConfigManager::getConfigDir() const {
    return impl_->configDir;
}

} // namespace wingman
