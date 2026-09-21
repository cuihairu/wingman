#include "wingman/runtime/rpc/config_handler.hpp"

namespace wingman::rpc {

void registerRuntimeConfigHandlers(RpcDispatcher& dispatcher,
                                   const RemoteConfigAccess& access) {
    dispatcher.registerHandler("config.getRemote", [&access](const nlohmann::json&) -> nlohmann::json {
        if (!access.get) {
            return {{"success", false}, {"error", "remote config not available"}};
        }
        return {{"success", true}, {"result", access.get()}};
    });

    dispatcher.registerHandler("config.setRemote", [&access](const nlohmann::json& req) -> nlohmann::json {
        if (!access.apply) {
            return {{"success", false}, {"error", "remote config not writable"}};
        }
        auto error = access.apply(req);
        if (error.empty()) {
            return {{"success", true}, {"result", access.get()}};
        }
        return {{"success", false}, {"error", error}};
    });
}

} // namespace wingman::rpc
