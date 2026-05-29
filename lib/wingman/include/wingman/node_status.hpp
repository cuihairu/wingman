#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace wingman {

// Node state
enum class NodeState {
    Online,      // Online
    Busy,        // Busy (executing script)
    Idle,        // Idle
    Error,       // Error
    Offline      // Offline
};

// Game window status
struct GameWindowStatus {
    std::string title;           // Window title
    std::string processName;     // Process name
    uintptr_t handle;            // Window handle
    int x, y, width, height;    // Window position and size
    bool isForeground;           // Whether in foreground

    std::string toJson() const;
    static GameWindowStatus fromJson(const std::string& json);
};

// Script execution status
struct ScriptStatus {
    std::string name;            // Script name
    std::string state;           // State: running, paused, stopped, error
    int64_t uptimeSeconds;       // Uptime (seconds)
    std::string lastError;       // Last error message

    std::string toJson() const;
    static ScriptStatus fromJson(const std::string& json);
};

// Node heartbeat data
struct NodeHeartbeat {
    std::string nodeId;          // Node ID (auto-generated or configured)
    std::string hostname;        // Hostname
    NodeState status;            // Node state
    int64_t timestamp;           // Timestamp (milliseconds)

    // System information
    double cpuUsage;             // CPU usage (0-100)
    double memoryUsage;          // Memory usage (MB)

    // Game state
    std::vector<GameWindowStatus> games;      // Game window list
    std::vector<ScriptStatus> scripts;        // Running scripts

    // Version information
    std::string version;         // Wingman version

    std::string toJson() const;
    static NodeHeartbeat fromJson(const std::string& json);

    // Get current timestamp (milliseconds)
    static int64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

// Server commands
enum class ServerCommand {
    None,
    StartScript,     // Start script
    StopScript,      // Stop script
    PauseScript,     // Pause script
    ResumeScript,    // Resume script
    Restart,         // Restart node
    Shutdown,        // Shutdown node
    UpdateConfig     // Update configuration
};

// Server command data
struct ServerCommandData {
    ServerCommand command;
    std::string scriptPath;      // Script path (used when StartScript)
    std::string configData;      // Configuration data (used when UpdateConfig)
    int64_t timestamp;           // Command timestamp

    std::string toJson() const;
    static ServerCommandData fromJson(const std::string& json);
};

} // namespace wingman
