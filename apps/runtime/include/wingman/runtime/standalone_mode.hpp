#pragma once

#include "wingman/runtime/config.hpp"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace wingman::runtime {

// ========== 脚本状态 ==========

enum class ScriptState {
    Unknown,
    Loaded,
    Running,
    Paused,
    Stopped,
    Error
};

struct ScriptInfo {
    std::string id;
    std::string path;
    ScriptState state = ScriptState::Unknown;
    std::string error;
    int64_t uptime = 0;
};

// ========== 单机模式 ==========

class StandaloneMode {
    class Impl;

public:
public:
    StandaloneMode(const StandaloneModeConfig& config);
    ~StandaloneMode();

    // 启动/停止
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // 脚本管理
    std::string loadScript(const std::string& path);
    bool unloadScript(const std::string& id);
    bool startScript(const std::string& id);
    bool pauseScript(const std::string& id);
    bool resumeScript(const std::string& id);
    bool stopScript(const std::string& id);

    // 查询
    std::vector<ScriptInfo> listScripts() const;
    ScriptInfo getScript(const std::string& id) const;

    // 获取配置
    const StandaloneModeConfig& getConfig() const;

private:
    // 生成脚本 ID
    std::string generateScriptId();
    // P-Impl
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
};

} // namespace wingman::runtime
