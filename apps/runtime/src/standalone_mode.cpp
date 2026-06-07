#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <random>
#include <algorithm>

namespace wingman::runtime {

// ========== StandaloneMode 实现 ==========

class StandaloneMode::Impl {
public:
    StandaloneModeConfig config;
    std::map<std::string, std::string> scripts;
    mutable std::mutex scriptsMutex;
};

namespace {

ScriptState toRuntimeState(wingman::ScriptState state) {
    switch (state) {
    case wingman::ScriptState::loaded:
        return ScriptState::Loaded;
    case wingman::ScriptState::running:
        return ScriptState::Running;
    case wingman::ScriptState::paused:
        return ScriptState::Paused;
    case wingman::ScriptState::error:
        return ScriptState::Error;
    case wingman::ScriptState::unloaded:
        return ScriptState::Stopped;
    }
    return ScriptState::Unknown;
}

ScriptInfo toRuntimeInfo(const std::string& id, const wingman::ScriptInfo& info) {
    ScriptInfo result;
    result.id = id;
    result.path = info.config.path;
    result.state = toRuntimeState(info.state);
    result.error = info.lastError;
    result.uptime = static_cast<int64_t>(info.lastLoaded);
    return result;
}

} // namespace

StandaloneMode::StandaloneMode(const StandaloneModeConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
}

StandaloneMode::~StandaloneMode() {
    stop();
}

bool StandaloneMode::start() {
    if (running_.load()) {
        spdlog::warn("StandaloneMode already running");
        return true;
    }

    spdlog::info("Starting StandaloneMode - script dir: {}", impl_->config.scriptDir);

    // 创建脚本目录
    try {
        std::filesystem::create_directories(impl_->config.scriptDir);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create script directory: {}", e.what());
        return false;
    }

    running_.store(true);

    for (const auto& scriptPath : impl_->config.autoStart) {
        const auto id = loadScript(scriptPath);
        if (!id.empty()) {
            startScript(id);
        }
    }

    spdlog::info("StandaloneMode started");
    return true;
}

void StandaloneMode::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping StandaloneMode");

    std::vector<std::string> ids;
    {
        std::lock_guard lock(impl_->scriptsMutex);
        for (const auto& [id, _] : impl_->scripts) {
            ids.push_back(id);
        }
    }

    auto& manager = getScriptManager();
    for (const auto& id : ids) {
        if (!manager.stopScript(id)) {
            spdlog::debug("Script was not running or could not be stopped: {}", id);
        }
        if (manager.unloadScript(id)) {
            spdlog::info("Stopped script: {}", id);
        } else {
            spdlog::warn("Failed to unload script while stopping standalone mode: {}", id);
        }
    }

    {
        std::lock_guard lock(impl_->scriptsMutex);
        impl_->scripts.clear();
    }

    running_.store(false);
    spdlog::info("StandaloneMode stopped");
}

std::string StandaloneMode::loadScript(const std::string& path) {
    spdlog::info("Loading script: {}", path);

    // 检查文件是否存在
    if (!std::filesystem::exists(path)) {
        spdlog::error("Script file not found: {}", path);
        return "";
    }

    // 生成唯一 ID
    std::string id = generateScriptId();

    wingman::ScriptConfig scriptConfig;
    scriptConfig.name = id;
    scriptConfig.path = path;
    // Default to sandboxed mode for security (trusted mode requires explicit configuration)
    scriptConfig.sandboxed = true;

    if (!getScriptManager().loadScript(id, path, scriptConfig)) {
        spdlog::error("Failed to load script through ScriptManager: {}", path);
        return "";
    }

    std::lock_guard lock(impl_->scriptsMutex);
    impl_->scripts[id] = path;

    spdlog::info("Script loaded: {} -> {}", path, id);
    return id;
}

bool StandaloneMode::unloadScript(const std::string& id) {
    spdlog::info("Unloading script: {}", id);

    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            spdlog::warn("Script not found: {}", id);
            return false;
        }
    }

    auto& manager = getScriptManager();
    if (!manager.stopScript(id)) {
        spdlog::debug("Script was not running or could not be stopped: {}", id);
    }
    if (!manager.unloadScript(id)) {
        spdlog::warn("Failed to unload script: {}", id);
        return false;
    }

    {
        std::lock_guard lock(impl_->scriptsMutex);
        impl_->scripts.erase(id);
    }
    spdlog::info("Script unloaded: {}", id);
    return true;
}

bool StandaloneMode::startScript(const std::string& id) {
    spdlog::info("Starting script: {}", id);

    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            spdlog::warn("Script not found: {}", id);
            return false;
        }
    }

    bool started = getScriptManager().runScript(id);
    if (started) {
        spdlog::info("Script started: {}", id);
    }
    return started;
}

bool StandaloneMode::pauseScript(const std::string& id) {
    spdlog::info("Pausing script: {}", id);

    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            spdlog::warn("Script not found: {}", id);
            return false;
        }
    }

    bool paused = getScriptManager().pauseScript(id);
    if (paused) {
        spdlog::info("Script paused: {}", id);
    }
    return paused;
}

bool StandaloneMode::resumeScript(const std::string& id) {
    spdlog::info("Resuming script: {}", id);

    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            spdlog::warn("Script not found: {}", id);
            return false;
        }
    }

    bool resumed = getScriptManager().resumeScript(id);
    if (resumed) {
        spdlog::info("Script resumed: {}", id);
    }
    return resumed;
}

bool StandaloneMode::stopScript(const std::string& id) {
    spdlog::info("Stopping script: {}", id);

    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            spdlog::warn("Script not found: {}", id);
            return false;
        }
    }

    bool stopped = getScriptManager().stopScript(id);
    if (stopped) {
        spdlog::info("Script stopped: {}", id);
    }
    return stopped;
}

size_t StandaloneMode::pauseAllScripts() {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(impl_->scriptsMutex);
        ids.reserve(impl_->scripts.size());
        for (const auto& [id, _] : impl_->scripts) {
            ids.push_back(id);
        }
    }

    size_t pausedCount = 0;
    for (const auto& id : ids) {
        if (pauseScript(id)) {
            ++pausedCount;
        }
    }
    return pausedCount;
}

size_t StandaloneMode::resumeAllScripts() {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(impl_->scriptsMutex);
        ids.reserve(impl_->scripts.size());
        for (const auto& [id, _] : impl_->scripts) {
            ids.push_back(id);
        }
    }

    size_t resumedCount = 0;
    for (const auto& id : ids) {
        if (resumeScript(id)) {
            ++resumedCount;
        }
    }
    return resumedCount;
}

size_t StandaloneMode::stopAllScripts() {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(impl_->scriptsMutex);
        ids.reserve(impl_->scripts.size());
        for (const auto& [id, _] : impl_->scripts) {
            ids.push_back(id);
        }
    }

    size_t stoppedCount = 0;
    for (const auto& id : ids) {
        if (stopScript(id)) {
            ++stoppedCount;
        }
    }
    return stoppedCount;
}

std::vector<ScriptInfo> StandaloneMode::listScripts() const {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(impl_->scriptsMutex);
        ids.reserve(impl_->scripts.size());
        for (const auto& [id, _] : impl_->scripts) {
            ids.push_back(id);
        }
    }

    std::vector<ScriptInfo> result;
    result.reserve(ids.size());

    auto& manager = getScriptManager();
    for (const auto& id : ids) {
        auto info = manager.getScriptInfo(id);
        if (info) {
            result.push_back(toRuntimeInfo(id, *info));
        }
    }

    return result;
}

ScriptInfo StandaloneMode::getScript(const std::string& id) const {
    {
        std::lock_guard lock(impl_->scriptsMutex);
        if (impl_->scripts.find(id) == impl_->scripts.end()) {
            return ScriptInfo{};
        }
    }

    auto info = getScriptManager().getScriptInfo(id);
    if (info) {
        return toRuntimeInfo(id, *info);
    }
    return ScriptInfo{};
}

std::string StandaloneMode::generateScriptId() {
    // 生成随机 ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    return "script_" + std::to_string(dis(gen));
}

const StandaloneModeConfig& StandaloneMode::getConfig() const {
    return impl_->config;
}

} // namespace wingman::runtime
