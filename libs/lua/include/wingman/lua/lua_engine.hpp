#pragma once

#include <string>
#include <memory>
#include <functional>
#include <sol/sol.hpp>

namespace wingman::lua {

// ========== Lua 引擎 ==========

class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    // 禁止拷贝
    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    // 初始化
    bool initialize();
    void shutdown();

    // 获取 Lua 状态
    sol::state& getState() { return lua_; }
    const sol::state& getState() const { return lua_; }

    // 执行脚本
    bool executeFile(const std::string& path);
    bool executeString(const std::string& code);
    bool executeBuffer(const char* buffer, size_t size, const std::string& name = "buffer");

    // 加载模块
    bool loadModule(const std::string& name, const std::string& path);

    // 注册 C 函数
    using LuaCFunction = sol::protected_function;
    void registerFunction(const std::string& name, sol::function func);

    // 设置全局变量
    void setGlobal(const std::string& name, const std::string& value);
    void setGlobal(const std::string& name, int value);
    void setGlobal(const std::string& name, double value);

    // 调用回调
    using LuaCallback = std::function<void(const std::string&)>;
    void setErrorCallback(LuaCallback callback) { errorCallback_ = std::move(callback); }

    // 模板方法：设置任意类型的全局变量
    template<typename T>
    void setGlobal(const std::string& name, T&& value) {
        if (!lua_.ready()) return;
        lua_[name] = std::forward<T>(value);
    }

    // 模板方法：注册任意类型的函数
    template<typename F>
    void registerFunction(const std::string& name, F&& func) {
        if (!lua_.ready()) return;
        lua_.set_function(name, std::forward<F>(func));
    }

private:
    sol::state lua_;
    LuaCallback errorCallback_;
};

// ========== 脚本管理器 ==========

class ScriptManager {
public:
    struct ScriptInfo {
        std::string id;
        std::string path;
        enum class Status { Unknown, Loaded, Running, Paused, Stopped, Error };
        Status status = Status::Unknown;
        std::string error;
        int64_t uptime = 0;
    };

    ScriptManager();
    ~ScriptManager();

    // 禁止拷贝
    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;

    // 加载脚本
    std::string loadScript(const std::string& path);
    bool unloadScript(const std::string& id);

    // 运行控制
    bool startScript(const std::string& id);
    bool pauseScript(const std::string& id);
    bool resumeScript(const std::string& id);
    bool stopScript(const std::string& id);

    // 查询
    std::vector<ScriptInfo> listScripts() const;
    ScriptInfo getScriptInfo(const std::string& id) const;

    // 输出回调
    using OutputCallback = std::function<void(const std::string& scriptId, const std::string& output)>;
    void setOutputCallback(OutputCallback callback) { outputCallback_ = std::move(callback); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    OutputCallback outputCallback_;
};

} // namespace wingman::lua
