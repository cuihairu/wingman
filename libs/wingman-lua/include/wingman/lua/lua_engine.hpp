#pragma once

#include <string>
#include <memory>
#include <functional>

struct lua_State;

namespace wingman::lua {

// ========== Lua 引擎 ==========

class LuaEngine {
public:
    LuaEngine();
    ~LuaEngine();

    // 初始化
    bool initialize();
    void shutdown();

    // 获取 Lua 状态
    lua_State* getState() const { return L_; }

    // 执行脚本
    bool executeFile(const std::string& path);
    bool executeString(const std::string& code);
    bool executeBuffer(const char* buffer, size_t size, const std::string& name = "buffer");

    // 加载模块
    bool loadModule(const std::string& name, const std::string& path);

    // 注册 C 函数
    void registerFunction(const std::string& name, lua_CFunction func);

    // 设置全局变量
    void setGlobal(const std::string& name, const std::string& value);
    void setGlobal(const std::string& name, int value);
    void setGlobal(const std::string& name, double value);

    // 调用回调
    using LuaCallback = std::function<void(const std::string&)>;
    void setErrorCallback(LuaCallback callback) { errorCallback_ = std::move(callback); }

private:
    lua_State* L_ = nullptr;
    LuaCallback errorCallback_;

    // 初始化标准库
    void openLibs();

    // 错误处理
    static int luaErrorCallback(lua_State* L);
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
