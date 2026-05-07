#include "wingman/debugger/protocol.hpp"
#include <spdlog/spdlog.h>

namespace wingman {

std::string debugStateToString(DebugState state) {
    switch (state) {
        case DebugState::Stopped: return "stopped";
        case DebugState::Running: return "running";
        case DebugState::Paused: return "paused";
        case DebugState::Terminated: return "terminated";
        default: return "unknown";
    }
}

} // namespace wingman
