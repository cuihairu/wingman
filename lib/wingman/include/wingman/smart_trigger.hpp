#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include "wingman/vision.hpp"
#include "wingman/ocr.hpp"

namespace wingman {

// Trigger condition type
enum class TriggerConditionType {
    COLOR_FOUND,        // Color found
    COLOR_NOT_FOUND,    // Color lost
    IMAGE_FOUND,        // Image found
    IMAGE_NOT_FOUND,    // Image lost
    TEXT_FOUND,         // Text found
    TEXT_NOT_FOUND,     // Text lost
    EDGE_DETECTED,      // Edge detected
    COLOR_CHANGED,      // Color changed
    OCR_CONTAINS,       // OCR contains text
    OCR_EQUALS,         // OCR equals text
};

// Trigger action type
enum class TriggerActionType {
    CLICK,              // Click
    KEY_PRESS,          // Key press
    WAIT,               // Wait
    LUA_SCRIPT,         // Execute Lua script
    CUSTOM_CALLBACK,    // Callback function
    STOP,               // Stop trigger
    LOG                 // Log
};

// Trigger condition configuration
struct TriggerCondition {
    TriggerConditionType type;
    Color targetColor;               // For color-related
    int tolerance = 0;               // Color tolerance
    std::string templatePath;        // For image matching
    std::string targetText;          // For text recognition
    Rect searchRegion = {};          // Search region
    double threshold = 0.8;          // Match threshold
    Color previousColor;             // For color change detection
    bool hasPreviousColor = false;   // Whether there is a previous color
};

// Trigger action configuration
struct TriggerAction {
    TriggerActionType type;
    Point clickPosition = {};        // Click position
    int keyCode = 0;                 // Key code
    int waitMs = 0;                  // Wait time
    std::string luaScript;           // Lua script content
    std::string logMessage;          // Log message
    std::function<void()> callback;  // Callback function
};

// Smart trigger
class SmartTrigger {
public:
    SmartTrigger(const std::string& name);
    ~SmartTrigger();

    // Add condition
    void addCondition(const TriggerCondition& condition);

    // Add action
    void addAction(const TriggerAction& action);

    // Set check interval (milliseconds)
    void setCheckInterval(int intervalMs);

    // Set max trigger count (0 = unlimited)
    void setMaxTriggers(int maxCount);

    // Start monitoring
    bool start();

    // Stop monitoring
    void stop();

    // Whether running
    bool isRunning() const { return running_; }

    // Get trigger count
    int getTriggerCount() const { return triggerCount_; }

    // Reset trigger count
    void resetTriggerCount() { triggerCount_ = 0; }

    // Get name
    const std::string& getName() const { return name_; }

private:
    std::string name_;
    std::vector<TriggerCondition> conditions_;
    std::vector<TriggerAction> actions_;
    int checkIntervalMs_ = 100;
    int maxTriggers_ = 0;
    std::atomic<int> triggerCount_{0};
    std::atomic<bool> running_{false};
    std::thread watchThread_;

    // Check conditions
    bool checkConditions();

    // Execute actions
    void executeActions();

    // Watch thread
    void watchLoop();
};

// Smart trigger manager
class SmartTriggerManager {
public:
    static SmartTriggerManager& instance();

    // Create trigger
    std::shared_ptr<SmartTrigger> createTrigger(const std::string& name);

    // Get trigger
    std::shared_ptr<SmartTrigger> getTrigger(const std::string& name);

    // Remove trigger
    void removeTrigger(const std::string& name);

    // Start all triggers
    void startAll();

    // Stop all triggers
    void stopAll();

    // Get all triggers
    std::vector<std::shared_ptr<SmartTrigger>> getAllTriggers() const;

private:
    std::map<std::string, std::shared_ptr<SmartTrigger>> triggers_;
    mutable std::mutex mutex_;
};

} // namespace wingman
