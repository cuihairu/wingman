#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <functional>

namespace wingman {

// Server connection configuration
struct ServerConfig {
    std::string host = "localhost";
    int port = 9527;
    std::string username;
    std::string password;  // Note: should be encrypted in production
    bool autoConnect = false;
    bool serverControlled = false;  // Server control mode: allows remote script deployment

    // Convert to JSON
    std::string toJson() const;

    // Parse from JSON
    static ServerConfig fromJson(const std::string& json);

    // Validate configuration
    bool isValid() const { return !host.empty() && port > 0 && port <= 65535; }
};

// Auto-run configuration
struct AutoRunConfig {
    bool enabled = false;           // Whether to enable auto-run
    std::string scriptPath;         // Script path
    int delaySeconds = 0;           // Startup delay (seconds)
    bool repeat = false;            // Whether to repeat
    int repeatInterval = 0;         // Repeat interval (seconds), 0 means script controls its own loop

    std::string toJson() const;
    static AutoRunConfig fromJson(const std::string& json);
};

// Heartbeat configuration
struct HeartbeatConfig {
    bool enabled = true;            // Whether to enable heartbeat
    int intervalSeconds = 30;       // Heartbeat interval (seconds)
    int timeoutSeconds = 90;        // Timeout (seconds), server considers node offline if no heartbeat received within this time

    std::string toJson() const;
    static HeartbeatConfig fromJson(const std::string& json);
};

// Game configuration
struct GameConfig {
    std::string name;               // Game name
    std::string path;               // Game executable path
    std::string args;               // Launch arguments
    std::string workingDir;         // Working directory
    bool autoStart = false;         // Whether to auto-start the game
    std::string scriptPath;         // Associated auto script
    std::string windowTitle;        // Window title (for detection)
    int delaySeconds = 5;           // Wait time after game launch (seconds)
    bool autoRestart = false;       // Whether to auto-restart after game closes
    int restartDelay = 10;          // Restart delay (seconds)
    int maxRestarts = 3;            // Max restart count (0 = unlimited)
    int restartCount = 0;           // Current restart count (internal use)

    std::string toJson() const;
    static GameConfig fromJson(const std::string& json);

    bool isValid() const { return !path.empty(); }
};

// General configuration manager
class ConfigManager {
public:
    explicit ConfigManager(const std::string& configDir = "config");
    ~ConfigManager();

    // Non-copyable
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Server configuration
    ServerConfig getServerConfig() const;
    bool setServerConfig(const ServerConfig& config);

    // Auto-run configuration
    AutoRunConfig getAutoRunConfig() const;
    bool setAutoRunConfig(const AutoRunConfig& config);

    // Heartbeat configuration
    HeartbeatConfig getHeartbeatConfig() const;
    bool setHeartbeatConfig(const HeartbeatConfig& config);

    // Game configuration
    std::vector<GameConfig> getGameConfigList() const;
    bool writeGameConfigList(const std::vector<GameConfig>& games);
    bool addGameConfig(const GameConfig& game);
    bool removeGameConfig(const std::string& name);

    // Generic key-value access
    std::optional<std::string> get(const std::string& key) const;
    bool set(const std::string& key, const std::string& value);
    bool remove(const std::string& key);

    // Save to disk
    bool save();

    // Reload from disk
    bool load();

    // Get config directory
    const std::string& getConfigDir() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
