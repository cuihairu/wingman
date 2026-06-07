#include "wingman/runtime/commands/start_command.hpp"

namespace wingman::runtime::commands {

std::string g_lastStartConfigPath;
bool g_lastStartForceStandalone = false;

int startCommand(const std::string& configPath) {
    StartOptions options;
    options.configPath = configPath;
    return startCommand(options);
}

int startCommand(const StartOptions& options) {
    g_lastStartConfigPath = options.configPath;
    g_lastStartForceStandalone = options.forceStandalone;
    return 42;
}

} // namespace wingman::runtime::commands
