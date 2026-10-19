#include "wingman/runtime/rpc/update_handler.hpp"

#include "wingman/update_poller.hpp"

#include <nlohmann/json.hpp>

namespace wingman::rpc {

namespace {

// 更新功能未启用时的统一返回：与 UpdateStatus::toJson() 形状一致，
// GUI 无需区分「未启用」与「已启用」两种载荷结构。
nlohmann::json disabledStatus() {
    wingman::UpdateStatus status;
    status.lastError = "update not configured";
    return status.toJson();
}

} // namespace

void registerRuntimeUpdateHandlers(RpcDispatcher& dispatcher, UpdatePoller* poller) {
    using json = nlohmann::json;

    dispatcher.registerHandler("update.status", [poller](const json&) -> json {
        if (!poller) {
            return disabledStatus();
        }
        return poller->status().toJson();
    });

    dispatcher.registerHandler("update.check", [poller](const json&) -> json {
        if (!poller) {
            return {{"started", false}, {"status", disabledStatus()}};
        }
        poller->requestCheck();
        return {{"started", true}, {"status", poller->status().toJson()}};
    });

    dispatcher.registerHandler("update.download", [poller](const json&) -> json {
        if (!poller) {
            return {{"started", false}, {"status", disabledStatus()}};
        }
        poller->requestDownload();
        return {{"started", true}, {"status", poller->status().toJson()}};
    });
}

} // namespace wingman::rpc
