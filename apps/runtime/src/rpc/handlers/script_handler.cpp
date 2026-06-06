#include "wingman/rpc/script_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"

namespace wingman::rpc {

void registerScriptHandlers(RpcDispatcher& dispatcher, runtime::StandaloneMode& standalone) {
    using json = nlohmann::json;

    dispatcher.registerHandler("script.list", [&standalone](const json&) -> json {
        auto scripts = standalone.listScripts();
        json arr = json::array();
        for (const auto& s : scripts) {
            arr.push_back({
                {"id", s.id},
                {"name", s.path},
                {"path", s.path},
                {"size", 0},
                {"isRunning", s.state == runtime::ScriptState::Running}
            });
        }
        return {{"scripts", arr}};
    });

    dispatcher.registerHandler("script.start", [&standalone](const json& params) -> json {
        std::string path = params.value("path", "");
        if (path.empty()) {
            return {{"success", false}, {"error", "Missing path"}};
        }
        std::string id = standalone.loadScript(path);
        if (id.empty()) {
            return {{"success", false}, {"error", "Failed to load script"}};
        }
        if (!standalone.startScript(id)) {
            return {{"success", false}, {"error", "Failed to start script"}};
        }
        return {{"scriptId", id}, {"status", "running"}};
    });

    dispatcher.registerHandler("script.stop", [&standalone](const json& params) -> json {
        std::string scriptId = params.value("scriptId", "");
        if (scriptId.empty()) {
            return {{"success", false}, {"error", "Missing scriptId"}};
        }
        if (!standalone.stopScript(scriptId)) {
            return {{"success", false}, {"error", "Failed to stop script"}};
        }
        return {{"success", true}};
    });
}

} // namespace wingman::rpc
