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

const char* scriptStateToString(runtime::ScriptState state) {
    switch (state) {
        case runtime::ScriptState::Loaded: return "loaded";
        case runtime::ScriptState::Running: return "running";
        case runtime::ScriptState::Paused: return "paused";
        case runtime::ScriptState::Stopped: return "stopped";
        case runtime::ScriptState::Error: return "error";
        default: return "unknown";
    }
}

nlohmann::json scriptToJson(const runtime::ScriptInfo& s) {
    return {
        {"id", s.id},
        {"name", s.path},
        {"path", s.path},
        {"size", 0},
        {"isRunning", s.state == runtime::ScriptState::Running},
        {"state", scriptStateToString(s.state)},
        {"error", s.error},
        // epoch 毫秒时间戳（脚本最近一次加载时间），GUI 据此计算运行时长
        {"loadedAt", s.uptime}
    };
}

} // namespace

void registerScriptHandlers(RpcDispatcher& dispatcher, runtime::StandaloneMode& standalone) {
    using json = nlohmann::json;

    dispatcher.registerHandler("script.list", [&standalone](const json&) -> json {
        auto scripts = standalone.listScripts();
        json arr = json::array();
        for (const auto& s : scripts) {
            arr.push_back(scriptToJson(s));
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

    dispatcher.registerHandler("script.pause", [&standalone](const json& params) -> json {
        std::string scriptId = params.value("scriptId", "");
        if (scriptId.empty()) {
            return {{"success", false}, {"error", "Missing scriptId"}};
        }
        if (!standalone.pauseScript(scriptId)) {
            return {{"success", false}, {"error", "Failed to pause script"}};
        }
        return {{"success", true}};
    });

    dispatcher.registerHandler("script.resume", [&standalone](const json& params) -> json {
        std::string scriptId = params.value("scriptId", "");
        if (scriptId.empty()) {
            return {{"success", false}, {"error", "Missing scriptId"}};
        }
        if (!standalone.resumeScript(scriptId)) {
            return {{"success", false}, {"error", "Failed to resume script"}};
        }
        return {{"success", true}};
    });

    // 重启：对已加载脚本 stop → start（路径复用，id 不变）
    dispatcher.registerHandler("script.restart", [&standalone](const json& params) -> json {
        std::string scriptId = params.value("scriptId", "");
        if (scriptId.empty()) {
            return {{"success", false}, {"error", "Missing scriptId"}};
        }

        auto info = standalone.getScript(scriptId);
        if (info.path.empty() && info.id.empty()) {
            return {{"success", false}, {"error", "Script not found"}};
        }

        if (info.state == runtime::ScriptState::Running || info.state == runtime::ScriptState::Paused) {
            if (!standalone.stopScript(scriptId)) {
                return {{"success", false}, {"error", "Failed to stop script for restart"}};
            }
        }
        if (!standalone.startScript(scriptId)) {
            return {{"success", false}, {"error", "Failed to restart script"}};
        }
        return {{"success", true}};
    });

    // 卸载：从脚本表中移除（仅对非运行中脚本有效）
    dispatcher.registerHandler("script.unload", [&standalone](const json& params) -> json {
        std::string scriptId = params.value("scriptId", "");
        if (scriptId.empty()) {
            return {{"success", false}, {"error", "Missing scriptId"}};
        }
        if (!standalone.unloadScript(scriptId)) {
            return {{"success", false}, {"error", "Failed to unload script (stop it first)"}};
        }
        return {{"success", true}};
    });
}

} // namespace wingman::rpc
