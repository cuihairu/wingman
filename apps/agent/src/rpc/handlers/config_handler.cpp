#include "wingman/runtime/rpc/config_handler.hpp"

namespace wingman::rpc {

// access 按值拷贝进 handler：形参引用在注册返回后即失效（调用方传临时对象
// 或短生命周期通道时，按引用捕获会在 dispatch 时 use-after-free——实测段
// 错误）。结构体仅含两个 std::function，按值持有零成本，与 system_handler
// 对 RuntimeStatusProviders 的按值捕获约定一致。
void registerRuntimeConfigHandlers(RpcDispatcher& dispatcher,
                                   const RemoteConfigAccess& access) {
    const RemoteConfigAccess owned = access;
    dispatcher.registerHandler("config.getRemote", [owned](const nlohmann::json&) -> nlohmann::json {
        if (!owned.get) {
            return {{"success", false}, {"error", "remote config not available"}};
        }
        return {{"success", true}, {"result", owned.get()}};
    });

    dispatcher.registerHandler("config.setRemote", [owned](const nlohmann::json& req) -> nlohmann::json {
        if (!owned.apply) {
            return {{"success", false}, {"error", "remote config not writable"}};
        }
        auto error = owned.apply(req);
        if (error.empty()) {
            return {{"success", true}, {"result", owned.get()}};
        }
        return {{"success", false}, {"error", error}};
    });
}

} // namespace wingman::rpc
