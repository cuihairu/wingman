#include "wingman/rpc/rpc_dispatcher.hpp"
#include <spdlog/spdlog.h>

namespace wingman::rpc {

void RpcDispatcher::registerHandler(const std::string& method, Handler handler) {
    handlers_[method] = std::move(handler);
}

std::string RpcDispatcher::dispatch(const std::string& rawMessage) {
    using json = nlohmann::json;

    json request;
    try {
        request = json::parse(rawMessage);
    } catch (const std::exception& e) {
        return json{
            {"type", "error"},
            {"error", "Invalid JSON: " + std::string(e.what())}
        }.dump();
    }

    std::string method = request.value("method", "");
    std::string id = request.value("id", "");
    json params = request.value("params", json::object());

    auto it = handlers_.find(method);
    if (it == handlers_.end()) {
        spdlog::warn("Unknown RPC method: {}", method);
        return json{
            {"type", "response"},
            {"id", id},
            {"data", {{"success", false}, {"error", "Unknown method: " + method}}}
        }.dump();
    }

    try {
        json result = it->second(params);
        // If handler returns a data object with success field, use it directly
        // Otherwise wrap in success=true for backward compatibility
        json responseData;
        if (result.is_object() && result.contains("success")) {
            responseData = result;
        } else {
            responseData = {{"success", true}, {"result", result}};
        }
        return json{
            {"type", "response"},
            {"id", id},
            {"data", responseData}
        }.dump();
    } catch (const std::exception& e) {
        spdlog::error("RPC handler error for {}: {}", method, e.what());
        return json{
            {"type", "response"},
            {"id", id},
            {"data", {{"success", false}, {"error", e.what()}}}
        }.dump();
    }
}

} // namespace wingman::rpc
