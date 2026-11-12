#include "wingman/runtime/commands/mcp_command.hpp"

#include "wingman/mcp/mcp_server.hpp"
#include "wingman/version.hpp"

#include <iostream>

namespace wingman::runtime::commands {

int mcpCommand(const std::vector<std::string>& tail) {
    if (!tail.empty()) {
        std::cerr << "Error: mcp does not accept arguments\n";
        return 1;
    }

    // serverInfo.version 用纯 semver（WINGMAN_VERSION_FULL/STRING 带展示后缀，
    // 不适合机器可读字段）。
    wingman::mcp::McpServer server("wingman", WINGMAN_VERSION);
    server.run(std::cin, std::cout);
    return 0;
}

} // namespace wingman::runtime::commands
