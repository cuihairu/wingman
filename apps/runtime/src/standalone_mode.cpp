#include "wingman/client/standalone_mode.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <random>
#include <algorithm>

namespace wingman::client {

// ========== StandaloneMode 实现 ==========

class StandaloneMode::Impl {
public:
    StandaloneModeConfig config;
    std::map<std::string, ScriptInfo> scripts;
    mutable std::mutex scriptsMutex;
};

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
    spdlog::info("StandaloneMode started");
    return true;
}

void StandaloneMode::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping StandaloneMode");

    // 停止所有运行的脚本
    std::lock_guard lock(impl_->scriptsMutex);
    for (auto& [id, info] : impl_->scripts) {
        if (info.state == ScriptState::Running) {
            info.state = ScriptState::Stopped;
            spdlog::info("Stopped script: {}", id);
        }
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

    ScriptInfo info;
    info.id = id;
    info.path = path;
    info.state = ScriptState::Loaded;
    info.uptime = 0;

    std::lock_guard lock(impl_->scriptsMutex);
    impl_->scripts[id] = info;

    spdlog::info("Script loaded: {} -> {}", path, id);
    return id;
}

bool StandaloneMode::unloadScript(const std::string& id) {
    spdlog::info("Unloading script: {}", id);

    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it == impl_->scripts.end()) {
        spdlog::warn("Script not found: {}", id);
        return false;
    }

    // 如果正在运行，先停止
    if (it->second.state == ScriptState::Running) {
        it->second.state = ScriptState::Stopped;
    }

    impl_->scripts.erase(it);
    spdlog::info("Script unloaded: {}", id);
    return true;
}

bool StandaloneMode::startScript(const std::string& id) {
    spdlog::info("Starting script: {}", id);

    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it == impl_->scripts.end()) {
        spdlog::warn("Script not found: {}", id);
        return false;
    }

    if (it->second.state == ScriptState::Running) {
        spdlog::warn("Script already running: {}", id);
        return true;
    }

    it->second.state = ScriptState::Running;
    it->second.uptime = 0;

    spdlog::info("Script started: {}", id);
    return true;
}

bool StandaloneMode::pauseScript(const std::string& id) {
    spdlog::info("Pausing script: {}", id);

    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it == impl_->scripts.end()) {
        spdlog::warn("Script not found: {}", id);
        return false;
    }

    if (it->second.state != ScriptState::Running) {
        spdlog::warn("Script is not running: {}", id);
        return false;
    }

    it->second.state = ScriptState::Paused;
    spdlog::info("Script paused: {}", id);
    return true;
}

bool StandaloneMode::resumeScript(const std::string& id) {
    spdlog::info("Resuming script: {}", id);

    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it == impl_->scripts.end()) {
        spdlog::warn("Script not found: {}", id);
        return false;
    }

    if (it->second.state != ScriptState::Paused) {
        spdlog::warn("Script is not paused: {}", id);
        return false;
    }

    it->second.state = ScriptState::Running;
    spdlog::info("Script resumed: {}", id);
    return true;
}

bool StandaloneMode::stopScript(const std::string& id) {
    spdlog::info("Stopping script: {}", id);

    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it == impl_->scripts.end()) {
        spdlog::warn("Script not found: {}", id);
        return false;
    }

    it->second.state = ScriptState::Stopped;
    it->second.uptime = 0;
    spdlog::info("Script stopped: {}", id);
    return true;
}

std::vector<ScriptInfo> StandaloneMode::listScripts() const {
    std::lock_guard lock(impl_->scriptsMutex);
    std::vector<ScriptInfo> result;
    result.reserve(impl_->scripts.size());

    for (const auto& [id, info] : impl_->scripts) {
        result.push_back(info);
    }

    return result;
}

ScriptInfo StandaloneMode::getScript(const std::string& id) const {
    std::lock_guard lock(impl_->scriptsMutex);
    auto it = impl_->scripts.find(id);
    if (it != impl_->scripts.end()) {
        return it->second;
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

} // namespace wingman::client
