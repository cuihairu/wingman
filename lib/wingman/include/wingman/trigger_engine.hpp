#pragma once

#include <string>
#include <vector>
#include <memory>
#include "wingman/trigger.hpp"

namespace wingman {

// Trigger engine
// Responsible for loading triggers from config files and managing lifecycle
class TriggerEngine {
public:
    TriggerEngine();
    ~TriggerEngine();

    // Load triggers from Lua config file
    bool loadFromLua(const std::string& filepath);

    // Load triggers from YAML config file
    bool loadFromYAML(const std::string& filepath);

    // Start engine
    bool start();

    // Stop engine
    void stop();

    // Whether running
    bool isRunning() const { return m_running; }

    // Get trigger manager
    TriggerManager* getManager() { return m_manager.get(); }

    // Enable/disable trigger
    bool enableTrigger(const std::string& name);
    bool disableTrigger(const std::string& name);

    // Get statistics
    struct Stats {
        size_t totalTriggers = 0;
        size_t enabledTriggers = 0;
        size_t totalTriggered = 0;
    };
    Stats getStats() const;

private:
    std::unique_ptr<TriggerManager> m_manager;
    std::vector<std::string> m_triggerNames;
    std::vector<size_t> m_triggerIds;
    bool m_running = false;
};

} // namespace wingman
