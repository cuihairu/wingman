#pragma once

// ========== wingman-core ==========
// Core module unified header

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/human.hpp"

#include "wingman/trigger.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/vision.hpp"

#include "wingman/system.hpp"
#include "wingman/performance.hpp"
#include "wingman/security.hpp"

#include "wingman/accounts.hpp"
#include "wingman/storage.hpp"
#include "wingman/auth.hpp"

#include "wingman/config.hpp"
#include "wingman/recorder.hpp"

namespace wingman::core {

// Core manager
class CoreManager {
public:
    static CoreManager& instance();

    // Initialize
    bool initialize(const std::string& configPath);
    void shutdown();

    // Get modules
    class ScreenManager& screen();
    class InputManager& input();
    class WindowFinder& window();
    class TriggerManager& trigger();
    class AccountManager& accounts();

    // Running state
    bool isRunning() const { return running_; }

private:
    CoreManager() = default;
    ~CoreManager() = default;

    bool running_ = false;
};

} // namespace wingman::core
