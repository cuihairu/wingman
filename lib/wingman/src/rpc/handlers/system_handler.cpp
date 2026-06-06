#include "wingman/rpc/system_handler.hpp"
#include <chrono>

namespace wingman::rpc {

void registerSystemHandlers(RpcDispatcher& dispatcher, const std::string& version) {
    using json = nlohmann::json;

    auto startTime = std::chrono::steady_clock::now();

    dispatcher.registerHandler("system.getStatus", [&startTime](const json&) -> json {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        return {
            {"server", "wingman"},
            {"version", "0.1.0"},
            {"uptime", uptime},
            {"runningScripts", 0},
            {"paused", false}
        };
    });

    dispatcher.registerHandler("system.getVersion", [&version](const json&) -> json {
        return {
            {"server", "wingman"},
            {"version", version},
            {"buildDate", __DATE__ " " __TIME__}
        };
    });
}

} // namespace wingman::rpc
