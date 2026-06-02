#pragma once

#include "wingman/screen.hpp"
#include "wingman/trigger.hpp"
#include "wingman/recorder.hpp"
#include "wingman/platform/iinput.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <mutex>

#ifdef _WIN32
// Windows socket type
typedef unsigned long long SOCKET;
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
// POSIX socket type
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

namespace wingman {

// ========== Remote control request/response ==========

struct RemoteRequest {
    std::string action;      // Action type
    nlohmann::json params;   // Parameters

    // Parse from JSON
    static RemoteRequest fromJson(const std::string& jsonStr);
    static RemoteRequest fromJson(const nlohmann::json& j);

    // Convert to JSON
    nlohmann::json toJson() const;
};

struct RemoteResponse {
    bool success = false;
    nlohmann::json data;
    std::string error;

    // Convert to JSON string
    std::string toJsonString() const;
    static RemoteResponse fromJson(const nlohmann::json& j);
};

// ========== Remote control server ==========

class RemoteControlServer {
public:
    RemoteControlServer();
    explicit RemoteControlServer(std::shared_ptr<platform::IInput> input);
    ~RemoteControlServer();

    // Start server
    bool start(int port = 9999);

    // Stop server
    void stop();

    // Check running state
    bool isRunning() const { return running_; }

    // Get port
    int getPort() const { return port_; }

    // Get client connection count
    size_t getConnectionCount() const { return connectionCount_; }

private:
    // Handle client connection
    void handleClient(SOCKET clientSocket);

    // Handle request
    RemoteResponse handleRequest(const RemoteRequest& req);

    // ========== Action handlers ==========

    // Screen operations
#ifdef WINGMAN_ENABLE_VISION
    RemoteResponse handleCaptureScreen(const nlohmann::json& params);
    RemoteResponse handleFindImage(const nlohmann::json& params);
#endif
    RemoteResponse handleGetPixel(const nlohmann::json& params);
    RemoteResponse handleFindColor(const nlohmann::json& params);

    // Input simulation
    RemoteResponse handleClick(const nlohmann::json& params);
    RemoteResponse handleMove(const nlohmann::json& params);
    RemoteResponse handleKey(const nlohmann::json& params);
    RemoteResponse handleTypeText(const nlohmann::json& params);

    // Trigger management
    RemoteResponse handleAddTrigger(const nlohmann::json& params);
    RemoteResponse handleRemoveTrigger(const nlohmann::json& params);
    RemoteResponse handleEnableTrigger(const nlohmann::json& params);
    RemoteResponse handleDisableTrigger(const nlohmann::json& params);
    RemoteResponse handleListTriggers(const nlohmann::json& params);

    // Macro operations
    RemoteResponse handleRecordMacro(const nlohmann::json& params);
    RemoteResponse handleStopMacroRecording(const nlohmann::json& params);
    RemoteResponse handlePlayMacro(const nlohmann::json& params);

    // System information
    RemoteResponse handlePing(const nlohmann::json& params);
    RemoteResponse handleGetVersion(const nlohmann::json& params);

    // ========== Server state ==========
    bool running_ = false;
    int port_ = 9999;
    size_t connectionCount_ = 0;

    // ========== Core components ==========
    std::unique_ptr<TriggerManager> triggerManager_;
    std::unique_ptr<MacroRecorder> macroRecorder_;
    std::shared_ptr<platform::IInput> input_;
    std::mutex mutex_;

    // ========== Implementation details ==========
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== Remote control client ==========

class RemoteControlClient {
public:
    RemoteControlClient();
    ~RemoteControlClient();

    // Connect to server
    bool connect(const std::string& host, int port = 9999);

    // Disconnect
    void disconnect();

    // Check connection state
    bool isConnected() const { return connected_; }

    // Send request
    RemoteResponse send(const RemoteRequest& req);
    RemoteResponse send(const std::string& action, const nlohmann::json& params = {});

    // ========== Convenience methods ==========

    // Screen operations
    RemoteResponse captureScreen(int x = 0, int y = 0, int width = 0, int height = 0);
    RemoteResponse getPixel(int x, int y);
    RemoteResponse findColor(uint32_t color, int x, int y, int width, int height, int tolerance = 10);

    // Input simulation
    RemoteResponse click(int x, int y, const std::string& button = "left");
    RemoteResponse move(int x, int y, int durationMs = 100);
    RemoteResponse key(int keyCode);
    RemoteResponse typeText(const std::string& text, int delayMs = 50);

    // Triggers
    RemoteResponse addTrigger(const nlohmann::json& config);
    RemoteResponse listTriggers();
    RemoteResponse enableTrigger(const std::string& id);
    RemoteResponse disableTrigger(const std::string& id);

    // System information
    RemoteResponse ping();

private:
    bool connected_ = false;
    std::string host_;
    int port_ = 9999;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
