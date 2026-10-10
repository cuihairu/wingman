#include "wingman/runtime/commands/stop_command.hpp"
#include "platform/agent_platform.hpp"
#include "wingman/process.hpp"
#include <spdlog/spdlog.h>

namespace wingman::runtime::commands {

namespace {

std::vector<ProcessId> findAgentProcesses() {
    std::vector<ProcessId> results;
    const ProcessId currentPid = Process::getCurrentId();

    for (const auto& name : platform::agentProcessNames()) {
        for (ProcessId pid : Process::findAll(name)) {
            if (pid != 0 && pid != currentPid) {
                results.push_back(pid);
            }
        }
    }

    return results;
}

} // namespace

int stopCommand() {
    bool stoppedAny = false;

    for (ProcessId pid : findAgentProcesses()) {
        if (Process::terminate(pid, true)) {
            stoppedAny = true;
        }
    }

    if (stoppedAny) {
        spdlog::info("Wingman agent stopped");
    } else {
        spdlog::warn("Wingman agent is not running");
    }

    return 0;
}

int statusCommand() {
    const bool running = !findAgentProcesses().empty();
    spdlog::info("Wingman agent status: {}", running ? "RUNNING" : "STOPPED");
    return running ? 0 : 1;
}

} // namespace wingman::runtime::commands
