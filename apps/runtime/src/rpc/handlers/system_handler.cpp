#include "wingman/runtime/rpc/system_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"

#include <chrono>

namespace wingman::rpc {

namespace {

bool isPausedState(wingman::runtime::ScriptState state) {
    return state == wingman::runtime::ScriptState::Paused;
}

bool isRunningState(wingman::runtime::ScriptState state) {
    return state == wingman::runtime::ScriptState::Running;
}

} // namespace

void registerRuntimeSystemHandlers(RpcDispatcher& dispatcher,
                                   const std::string& version,
                                   runtime::StandaloneMode& standalone) {
    using json = nlohmann::json;

    const auto startTime = std::chrono::steady_clock::now();

    dispatcher.registerHandler("system.getStatus", [&standalone, version, startTime](const json&) -> json {
        const auto scripts = standalone.listScripts();

        uint64_t runningScripts = 0;
        bool paused = false;
        for (const auto& script : scripts) {
            runningScripts += isRunningState(script.state) ? 1u : 0u;
            paused = paused || isPausedState(script.state);
        }

        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();

        return {
            {"server", "wingman"},
            {"version", version},
            {"uptime", uptime},
            {"runningScripts", runningScripts},
            {"paused", paused}
        };
    });

    dispatcher.registerHandler("system.togglePause", [&standalone](const json&) -> json {
        const auto scripts = standalone.listScripts();
        bool hasPaused = false;
        for (const auto& script : scripts) {
            if (isPausedState(script.state)) {
                hasPaused = true;
                break;
            }
        }

        const size_t changed = hasPaused
            ? standalone.resumeAllScripts()
            : standalone.pauseAllScripts();

        return {
            {"paused", !hasPaused},
            {"changedScripts", changed}
        };
    });

    dispatcher.registerHandler("system.pauseAll", [&standalone](const json&) -> json {
        return {
            {"paused", true},
            {"changedScripts", standalone.pauseAllScripts()}
        };
    });

    dispatcher.registerHandler("system.resumeAll", [&standalone](const json&) -> json {
        return {
            {"paused", false},
            {"changedScripts", standalone.resumeAllScripts()}
        };
    });

    dispatcher.registerHandler("system.stopAll", [&standalone](const json&) -> json {
        return {
            {"stoppedScripts", standalone.stopAllScripts()}
        };
    });

    dispatcher.registerHandler("system.isPaused", [&standalone](const json&) -> json {
        const auto scripts = standalone.listScripts();
        bool paused = false;
        for (const auto& script : scripts) {
            if (isPausedState(script.state)) {
                paused = true;
                break;
            }
        }

        return {{"paused", paused}};
    });
}

} // namespace wingman::rpc
