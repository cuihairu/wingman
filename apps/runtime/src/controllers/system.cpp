#include "wingman/runtime/controllers/system.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include "wingman/version.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace wingman::runtime::controllers {

RpcResponse SystemCtrl::getStatus(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        nlohmann::json status;
        status["server"] = "wingman";
        status["version"] = WINGMAN_VERSION;
        status["uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        status["runningScripts"] = getScriptManager().getRunningScripts().size();
        status["totalScripts"] = getScriptManager().getScriptNames().size();

        resp.success = true;
        resp.result = status;

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse SystemCtrl::getVersion(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        nlohmann::json version;
        version["server"] = "wingman";
        version["version"] = WINGMAN_VERSION;
        version["buildDate"] = __DATE__ " " __TIME__;

        resp.success = true;
        resp.result = version;

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RpcResponse SystemCtrl::quit(const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        // TODO: 优雅关闭
        spdlog::info("[System] Quit requested via WebSocket");

        resp.success = true;
        resp.result["message"] = "Shutting down...";

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

} // namespace wingman::runtime::controllers
