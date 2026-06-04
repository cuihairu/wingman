#pragma once

// ========== wingman-core ==========
// Core module unified header

#include <string>

#include "wingman/screen.hpp"
#include "wingman/platform/input_factory.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/human.hpp"

#include "wingman/trigger.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/vision.hpp"

#include "wingman/system.hpp"
#include "wingman/performance.hpp"
#include "wingman/security.hpp"

#include "wingman/storage.hpp"

#include "wingman/config.hpp"
#include "wingman/recorder.hpp"

namespace wingman::core {

// Core manager
class CoreManager {
public:
    static CoreManager& instance() {
        static CoreManager manager;
        return manager;
    }

    // Initialize
    bool initialize(const std::string& configPath = {}) {
        configPath_ = configPath;
        running_ = true;
        return true;
    }

    void shutdown() {
        running_ = false;
    }

    // Running state
    bool isRunning() const { return running_; }

    const std::string& configPath() const { return configPath_; }

private:
    CoreManager() = default;
    ~CoreManager() = default;

    bool running_ = false;
    std::string configPath_;
};

} // namespace wingman::core
