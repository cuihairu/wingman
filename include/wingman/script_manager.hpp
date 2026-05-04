#pragma once

#include <lua.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <filesystem>
#include <thread>

namespace wingman {

// 脚本配置
struct ScriptConfig {
    std::string name;
    std::string path;
    bool autoReload = false;
    bool sandboxed = true;
    int timeoutMs = 30000;
    std::unordered_map<std::string, std::string> env;
};

// 脚本状态
enum class ScriptState {
    unloaded,
    loaded,
    running,
    paused,
    error
};

// 脚本信息
struct ScriptInfo {
    ScriptConfig config;
    ScriptState state = ScriptState::unloaded;
    std::string lastError;
    uint64_t lastModified = 0;
    uint64_t lastLoaded = 0;
};

// 沙箱配置
struct SandboxConfig {
    bool disableIO = true;
    bool disableOS = true;
    bool disableDebug = true;
    bool disablePackage = true;
    bool disableCoroutine = false;
    uint64_t memoryLimit = 100 * 1024 * 1024; // 100MB
    uint64_t instructionLimit = 1000000; // 1M instructions
    uint64_t timeLimitMs = 30000;
};

// 脚本事件
enum class ScriptEvent {
    loaded,
    unloaded,
    started,
    stopped,
    error,
    reloaded
};

using ScriptEventCallback = std::function<void(const std::string& scriptName, ScriptEvent event, const std::string& message)>;

class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();

    // ========== 脚本加载管理 ==========

    // 加载脚本
    bool loadScript(const std::string& name, const std::string& path, const ScriptConfig& config = {});

    // 卸载脚本
    bool unloadScript(const std::string& name);

    // 重新加载脚本
    bool reloadScript(const std::string& name);

    // 检查脚本是否需要重新加载
    bool checkReload(const std::string& name);

    // 全局热加载检查
    void checkAllReloads();

    // ========== 脚本执行 ==========

    // 运行脚本
    bool runScript(const std::string& name);

    // 停止脚本
    bool stopScript(const std::string& name);

    // 暂停脚本
    bool pauseScript(const std::string& name);

    // 恢复脚本
    bool resumeScript(const std::string& name);

    // 调用脚本函数
    bool callFunction(const std::string& name, const std::string& func,
                     const std::vector<std::string>& args = {},
                     std::string* result = nullptr);

    // ========== 配置管理 ==========

    // 加载配置文件 (JSON/TOML/INI)
    bool loadConfig(const std::string& path);

    // 保存配置文件
    bool saveConfig(const std::string& path);

    // 获取配置值
    std::string getConfig(const std::string& key, const std::string& defaultValue = "");

    // 设置配置值
    void setConfig(const std::string& key, const std::string& value);

    // 获取环境变量
    std::string getEnv(const std::string& key) const;

    // 设置环境变量
    void setEnv(const std::string& key, const std::string& value);

    // ========== 状态查询 ==========

    // 获取脚本信息
    ScriptInfo* getScriptInfo(const std::string& name);

    // 获取所有脚本名称
    std::vector<std::string> getScriptNames() const;

    // 获取运行中的脚本
    std::vector<std::string> getRunningScripts() const;

    // 是否存在脚本
    bool hasScript(const std::string& name) const;

    // ========== 事件回调 ==========

    // 设置事件回调
    void setEventCallback(ScriptEventCallback callback);

    // ========== 沙箱管理 ==========

    // 设置全局沙箱配置
    void setSandboxConfig(const SandboxConfig& config);

    // 获取沙箱配置
    const SandboxConfig& getSandboxConfig() const;

    // ========== 热加载控制 ==========

    // 启用/禁用热加载
    void setAutoReload(const std::string& name, bool enabled);

    // 启用全局热加载
    void setGlobalAutoReload(bool enabled);

    // 开始热加载监听线程
    void startHotReload();

    // 停止热加载监听线程
    void stopHotReload();

    // ========== Lua 状态访问 ==========

    // 获取脚本的 Lua 状态
    lua_State* getLuaState(const std::string& name);

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<ScriptInfo>> m_scripts;
    std::unordered_map<std::string, std::string> m_config;
    std::unordered_map<std::string, std::string> m_env;
    SandboxConfig m_sandboxConfig;
    ScriptEventCallback m_eventCallback;
    bool m_globalAutoReload = false;
    bool m_hotReloadRunning = false;
    std::thread m_hotReloadThread;

    // 沙箱设置
    void setupSandbox(lua_State* L, const std::string& name);

    // 安全检查
    bool checkMemoryLimit(lua_State* L);
    bool checkInstructionLimit(lua_State* L);
    bool checkTimeLimit(const std::string& name);

    // 获取文件修改时间
    uint64_t getFileModifiedTime(const std::string& path);

    // 加载配置文件
    bool loadJsonConfig(const std::string& path);
    bool loadIniConfig(const std::string& path);

    // 触发事件
    void triggerEvent(const std::string& name, ScriptEvent event, const std::string& message = "");
};

} // namespace wingman
