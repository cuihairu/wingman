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

// ========== TrayMenuItemConfig Implementation ==========

std::string TrayMenuItemConfig::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["label"] = label;
    j["actionType"] = static_cast<int>(actionType);
    j["action"] = action;
    j["enabled"] = enabled;
    j["isSeparator"] = isSeparator;
    j["checked"] = checked;

    if (!subitems.empty()) {
        nlohmann::json subArray = nlohmann::json::array();
        for (const auto& item : subitems) {
            subArray.push_back(nlohmann::json::parse(item.toJson()));
        }
        j["subitems"] = subArray;
    }

    return j.dump();
}

TrayMenuItemConfig TrayMenuItemConfig::fromJson(const std::string& json) {
    TrayMenuItemConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("id")) config.id = j["id"];
        if (j.contains("label")) config.label = j["label"];
        if (j.contains("actionType")) config.actionType = static_cast<TrayActionType>(j["actionType"]);
        if (j.contains("action")) config.action = j["action"];
        if (j.contains("enabled")) config.enabled = j["enabled"];
        if (j.contains("isSeparator")) config.isSeparator = j["isSeparator"];
        if (j.contains("checked")) config.checked = j["checked"];

        if (j.contains("subitems") && j["subitems"].is_array()) {
            for (const auto& subJson : j["subitems"]) {
                config.subitems.push_back(fromJson(subJson.dump()));
            }
        }
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
    j["iconPath"] = iconPath;
    j["tooltip"] = tooltip;
    j["iconNormal"] = iconNormal;
    j["iconIdle"] = iconIdle;
    j["iconBusy"] = iconBusy;
    j["iconError"] = iconError;

    if (!menuItems.empty()) {
        nlohmann::json menuArray = nlohmann::json::array();
        for (const auto& item : menuItems) {
            menuArray.push_back(nlohmann::json::parse(item.toJson()));
        }
        j["menuItems"] = menuArray;
    }

    return j.dump();
}

TrayConfig TrayConfig::fromJson(const std::string& json) {
    TrayConfig config;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("minimizeToTray")) config.minimizeToTray = j["minimizeToTray"];
        if (j.contains("startMinimized")) config.startMinimized = j["startMinimized"];
        if (j.contains("showNotifications")) config.showNotifications = j["showNotifications"];
        if (j.contains("iconPath")) config.iconPath = j["iconPath"];
        if (j.contains("tooltip")) config.tooltip = j["tooltip"];
        if (j.contains("iconNormal")) config.iconNormal = j["iconNormal"];
        if (j.contains("iconIdle")) config.iconIdle = j["iconIdle"];
        if (j.contains("iconBusy")) config.iconBusy = j["iconBusy"];
        if (j.contains("iconError")) config.iconError = j["iconError"];

        if (j.contains("menuItems") && j["menuItems"].is_array()) {
            for (const auto& itemJson : j["menuItems"]) {
                config.menuItems.push_back(TrayMenuItemConfig::fromJson(itemJson.dump()));
            }
        }
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
            // 默认托盘配置
            nlohmann::json defaultTray = {
                {"minimizeToTray", true},
                {"startMinimized", false},
                {"showNotifications", true},
                {"iconPath", ""},
                {"tooltip", "Wingman"}
            };

            // 默认菜单项
            nlohmann::json defaultMenu = nlohmann::json::array();

            // 服务器开关菜单项
            defaultMenu.push_back({
                {"id", "server_toggle"},
                {"label", "☐ 启动服务器"},
                {"actionType", static_cast<int>(TrayActionType::callback)}
            });

            // 服务器状态信息
            defaultMenu.push_back({
                {"id", "server_info"},
                {"label", "  服务器: 未连接"},
                {"actionType", static_cast<int>(TrayActionType::none)},
                {"enabled", false}
            });

            defaultMenu.push_back({
                {"id", "local_ip"},
                {"label", "  本地 IP: 获取中..."},
                {"actionType", static_cast<int>(TrayActionType::none)},
                {"enabled", false}
            });

            defaultMenu.push_back({
                {"id", "sep0"},
                {"isSeparator", true}
            });

            defaultMenu.push_back({
                {"id", "help"},
                {"label", "帮助 / 用法"},
                {"actionType", static_cast<int>(TrayActionType::none)}
            });

            defaultMenu.push_back({
                {"id", "sep1"},
                {"isSeparator", true}
            });

            defaultMenu.push_back({
                {"id", "config_view"},
                {"label", "查看配置"},
                {"actionType", static_cast<int>(TrayActionType::none)}
            });

            defaultMenu.push_back({
                {"id", "sep2"},
                {"isSeparator", true}
            });

            defaultMenu.push_back({
                {"id", "exit"},
                {"label", "退出"},
                {"actionType", static_cast<int>(TrayActionType::callback)}
            });
            defaultTray["menuItems"] = defaultMenu;
            config["tray"] = defaultTray;
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
            config["games"] = nlohmann::json::array();  // 空游戏列表
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
        if (tray.contains("iconPath")) result.iconPath = tray["iconPath"];
        if (tray.contains("tooltip")) result.tooltip = tray["tooltip"];

        if (tray.contains("menuItems") && tray["menuItems"].is_array()) {
            for (const auto& itemJson : tray["menuItems"]) {
                result.menuItems.push_back(TrayMenuItemConfig::fromJson(itemJson.dump()));
            }
        }
    }

    return result;
}

bool ConfigManager::setTrayConfig(const TrayConfig& config) {
    nlohmann::json j = loadConfigJson(impl_->configDir);

    nlohmann::json trayJson;
    trayJson["minimizeToTray"] = config.minimizeToTray;
    trayJson["startMinimized"] = config.startMinimized;
    trayJson["showNotifications"] = config.showNotifications;
    trayJson["iconPath"] = config.iconPath;
    trayJson["tooltip"] = config.tooltip;
    trayJson["iconNormal"] = config.iconNormal;
    trayJson["iconIdle"] = config.iconIdle;
    trayJson["iconBusy"] = config.iconBusy;
    trayJson["iconError"] = config.iconError;

    if (!config.menuItems.empty()) {
        nlohmann::json menuArray = nlohmann::json::array();
        for (const auto& item : config.menuItems) {
            menuArray.push_back(nlohmann::json::parse(item.toJson()));
        }
        trayJson["menuItems"] = menuArray;
    }

    j["tray"] = trayJson;
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

    // 检查是否已存在同名游戏
    for (auto& existing : games) {
        if (existing.name == game.name) {
            existing = game;  // 更新现有配置
            return writeGameConfigList(games);
        }
    }

    // 添加新游戏
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
