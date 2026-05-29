#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>
#include <mutex>
#include <thread>

#include <spdlog/logger.h>

#include "wingman/screen.hpp"  // For Rect type

namespace wingman {

// Trigger types
enum class TriggerType {
    ColorFound,      // Color found
    ColorLost,       // Color lost
    ImageFound,      // Image found
    ImageLost,       // Image lost
    WindowOpened,    // Window opened
    WindowClosed,    // Window closed
    ProcessStarted,  // Process started
    ProcessStopped,  // Process stopped
    TimeElapsed,     // Time elapsed
    HotkeyPressed,   // Hotkey pressed
    PixelChanged,    // Pixel changed
};

// Trigger condition
struct BasicTriggerCondition {
    TriggerType type;
    std::string value;      // Specific value (e.g. color, window title, etc.)
    Rect region;           // Search region
    int tolerance;         // Tolerance
    int interval;          // Check interval (ms)
    bool enabled;
};

// Trigger action
enum class BasicTriggerAction {
    RunScript,       // Run script
    Click,           // Click
    KeyPress,        // Key press
    Type,            // Type text
    StopScript,      // Stop script
    PauseScript,     // Pause script
    ShowMessage,     // Show message
    PlayAudio,       // Play audio
    Log,             // Log
};

struct TriggerActionData {
    BasicTriggerAction type;
    std::string value;
    int x, y;           // Coordinates
    int delay;          // Delay
};

// Trigger configuration
struct TriggerConfig {
    std::string name;
    BasicTriggerCondition condition;
    std::vector<TriggerActionData> actions;
    bool oneShot;        // Trigger only once
    int cooldown;        // Cooldown (ms)
    bool enabled;
};

// Trigger instance (runtime data)
struct TriggerInstance {
    size_t id;
    TriggerConfig config;
    uint64_t startTime;       // Trigger start time
    uint64_t lastTriggerTime;
    bool triggered;
};

// Trigger manager
class TriggerManager {
public:
    TriggerManager();
    explicit TriggerManager(std::shared_ptr<spdlog::logger> logger);
    ~TriggerManager();

    // Add trigger
    size_t add(const TriggerConfig& config);

    // Update trigger configuration
    bool update(size_t id, const TriggerConfig& config);

    // Remove trigger
    void remove(size_t id);

    // Enable/disable trigger
    void enable(size_t id);
    void disable(size_t id);

    // Start/stop all triggers
    void start();
    void stop();

    // Get trigger state
    bool isRunning(size_t id) const;

    // Get all trigger configurations
    std::vector<TriggerConfig> getAllTriggerConfigs() const;

    // Get all trigger instances (with state)
    std::vector<TriggerInstance> getAllTriggerInstances() const;

    // Get configuration by ID
    std::optional<TriggerConfig> getTriggerConfig(size_t id) const;

    // Check if trigger exists
    bool hasTrigger(size_t id) const;

    // Get trigger count
    size_t getTriggerCount() const;

    // Set script engine factory (for executing script actions)
    void setScriptManager(class ScriptManager* mgr);

    // Set logger
    void setLogger(std::shared_ptr<spdlog::logger> logger);

private:
    std::vector<TriggerInstance> m_triggers;
    bool m_running;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    class ScriptManager* m_scriptManager;
    std::shared_ptr<spdlog::logger> m_logger;

    // Trigger check thread
    void checkThread();

    // Check single trigger
    bool checkTrigger(TriggerInstance& trigger);

    // Execute actions
    void executeActions(const std::vector<TriggerActionData>& actions);

    // Platform-specific message box
    static void showMessage(const std::string& message);

    // Platform-specific audio playback
    static void playAudio(const std::string& filepath);
};

} // namespace wingman
