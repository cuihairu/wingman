#include "wingman/runtime/runtime_context.hpp"
#include <filesystem>

namespace wingman::runtime {

namespace {

void preloadScripts(ScriptManager& manager) {
    const std::filesystem::path scriptsDir("scripts");
    if (!std::filesystem::exists(scriptsDir) || !std::filesystem::is_directory(scriptsDir)) {
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto path = entry.path();
        const auto scriptName = path.stem().string();
        manager.loadScript(scriptName, path.string());
    }
}

} // namespace

ScriptManager& getScriptManager() {
    static ScriptManager manager;
    static const bool initialized = [] {
        preloadScripts(manager);
        return true;
    }();
    (void)initialized;
    return manager;
}

} // namespace wingman::runtime
