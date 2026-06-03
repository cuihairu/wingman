#include <string>

namespace wingman::runtime::commands {

std::string g_lastStartConfigPath;

int startCommand(const std::string& configPath) {
    g_lastStartConfigPath = configPath;
    return 42;
}

} // namespace wingman::runtime::commands
