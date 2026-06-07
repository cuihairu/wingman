#include "wingman/rpc/script_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"

namespace wingman::rpc {

namespace {

runtime::ScriptInfo* findScriptByPath(std::vector<runtime::ScriptInfo>& scripts, const std::string& path) {
    for (auto& script : scripts) {
        if (script.path == path) {
            return &script;
        }
    }
    return nullptr;
}

} // namespace

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

        auto scripts = standalone.listScripts();
        if (auto* existing = findScriptByPath(scripts, path)) {
            if (!standalone.startScript(existing->id)) {
                return {{"success", false}, {"error", "Failed to start loaded script"}};
            }
            return {{"scriptId", existing->id}, {"status", "running"}, {"reused", true}};
        }

        std::string id = standalone.loadScript(path);
        if (id.empty()) {
            return {{"success", false}, {"error", "Failed to load script"}};
        }
        if (!standalone.startScript(id)) {
            return {{"success", false}, {"error", "Failed to start script"}};
        }
        return {{"scriptId", id}, {"status", "running"}, {"reused", false}};
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
