#include "wingman/script_manager.hpp"
#include "wingman/lua_bindings.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

namespace wingman {

// ========== Sandbox Implementation ==========

namespace {
    // 沙箱环境变量名前缀
    const char* SANDBOX_KEY = "__wm_sandbox";

    // 沙箱元表
    int sandboxIndex(lua_State* L);
    int sandboxNewIndex(lua_State* L);

    struct SandboxData {
        std::string scriptName;
        uint64_t startTime;
        uint64_t instructionCount;
    };

    // 沙箱钩子函数 - 用于指令计数
    void sandboxHook(lua_State* L, lua_Debug* ar) {
        lua_getallocf(L, nullptr);
        // 简单实现：每次钩子触发增加计数
        lua_pushstring(L, SANDBOX_KEY);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_isuserdata(L, -1)) {
            auto* data = static_cast<SandboxData*>(lua_touserdata(L, -1));
            data->instructionCount++;
        }
        lua_pop(L, 1);
    }

    // 创建沙箱环境
    lua_State* createSandboxedState(lua_State* parentL, const std::string& name, const SandboxConfig& config) {
        lua_State* sandboxL = lua_newthread(parentL);

        // 创建沙箱数据
        auto* data = static_cast<SandboxData*>(lua_newuserdata(sandboxL, sizeof(SandboxData)));
        data->scriptName = name;
        data->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        data->instructionCount = 0;

        // 保存沙箱数据到注册表
        lua_pushstring(sandboxL, SANDBOX_KEY);
        lua_pushvalue(sandboxL, -2);
        lua_settable(sandboxL, LUA_REGISTRYINDEX);

        // 设置指令钩子
        if (config.instructionLimit > 0) {
            lua_sethook(sandboxL, sandboxHook, LUA_MASKCOUNT, 1000);
        }

        // 创建受限的全局环境
        lua_newtable(sandboxL); // 沙箱全局表

        // 复制安全的函数到沙箱
        const char* safeFuncs[] = {
            "print", "tonumber", "tostring", "type", "pairs",
            "ipairs", "select", "assert", "error", "next",
            "unpack", "pcall", "xpcall", "rawget", "rawset",
            "rawequal", "rawlen", "getmetatable", "setmetatable",
            "math", "string", "table", "coroutine", nullptr
        };

        lua_getglobal(sandboxL, "_G");
        for (int i = 0; safeFuncs[i]; ++i) {
            lua_getfield(sandboxL, -1, safeFuncs[i]);
            lua_setfield(sandboxL, -3, safeFuncs[i]);
        }
        lua_pop(sandboxL, 1); // 弹出 _G

        // 设置元表以阻止访问不存在的全局变量
        lua_newtable(sandboxL);
        lua_pushcfunction(sandboxL, sandboxIndex);
        lua_setfield(sandboxL, -2, "__index");
        lua_pushcfunction(sandboxL, sandboxNewIndex);
        lua_setfield(sandboxL, -2, "__newindex");
        lua_setmetatable(sandboxL, -2);

        // 替换全局环境 (Lua 5.2+ compatible)
        // In Lua 5.2+, use lua_setupvalue instead of LUA_GLOBALSINDEX
        lua_pushvalue(sandboxL, -1);  // Push the table again
        lua_setglobal(sandboxL, "_G");  // Set as _G global

        return sandboxL;
    }

    int sandboxIndex(lua_State* L) {
        const char* key = lua_tostring(L, 2);
        // 禁止访问危险的库
        static const char* dangerous[] = {
            "io", "os", "debug", "package", "load", "loadfile",
            "dofile", "require", "module", nullptr
        };
        for (int i = 0; dangerous[i]; ++i) {
            if (strcmp(key, dangerous[i]) == 0) {
                lua_pushnil(L);
                return 1;
            }
        }
        // 尝试从安全环境获取
        lua_getglobal(L, key);
        return 1;
    }

    int sandboxNewIndex(lua_State* L) {
        const char* key = lua_tostring(L, 2);
        // 禁止修改危险的全局变量
        static const char* protected[] = {
            "io", "os", "debug", "package", "load", "loadfile",
            "dofile", "require", "module", "_G", nullptr
        };
        for (int i = 0; protected[i]; ++i) {
            if (strcmp(key, protected[i]) == 0) {
                luaL_error(L, "cannot modify protected global '%s'", key);
                return 0;
            }
        }
        lua_rawset(L, 1);
        return 0;
    }
}

// ========== ScriptManager Implementation ==========

ScriptManager::ScriptManager() {
    initLua();
}

ScriptManager::~ScriptManager() {
    stopHotReload();

    // 清理所有脚本的 Lua 状态
    for (auto& pair : m_scripts) {
        if (pair.second->luaState) {
            pair.second->luaState.reset();
        }
    }

    // 清理主 Lua 状态
    if (m_mainL) {
        lua_close(m_mainL);
        m_mainL = nullptr;
    }
}

// ========== 脚本加载管理 ==========

bool ScriptManager::loadScript(const std::string& name, const std::string& path, const ScriptConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查文件是否存在
    if (!std::filesystem::exists(path)) {
        triggerEvent(name, ScriptEvent::error, "file not found: " + path);
        return false;
    }

    // 卸载已存在的脚本
    if (m_scripts.find(name) != m_scripts.end()) {
        unloadScript(name);
    }

    // 创建脚本信息
    auto info = std::make_unique<ScriptInfo>();
    info->config = config;
    info->config.name = name;
    info->config.path = path;
    info->lastModified = getFileModifiedTime(path);

    m_scripts[name] = std::move(info);

    triggerEvent(name, ScriptEvent::loaded, "");
    return true;
}

bool ScriptManager::unloadScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    if (it->second->state == ScriptState::running) {
        stopScript(name);
    }

    triggerEvent(name, ScriptEvent::unloaded, "");
    m_scripts.erase(it);
    return true;
}

bool ScriptManager::reloadScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    auto& info = it->second;
    bool wasRunning = info->state == ScriptState::running;

    // 重新加载
    if (wasRunning) {
        stopScript(name);
    }

    info->lastModified = getFileModifiedTime(info->config.path);
    info->state = ScriptState::loaded;
    info->lastError.clear();

    if (wasRunning) {
        runScript(name);
    }

    triggerEvent(name, ScriptEvent::reloaded, "");
    return true;
}

bool ScriptManager::checkReload(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    auto& info = it->second;
    if (!info->config.autoReload && !m_globalAutoReload) {
        return false;
    }

    uint64_t currentModified = getFileModifiedTime(info->config.path);
    if (currentModified > info->lastModified) {
        return reloadScript(name);
    }

    return false;
}

void ScriptManager::checkAllReloads() {
    std::vector<std::string> names = getScriptNames();
    for (const auto& name : names) {
        checkReload(name);
    }
}

// ========== 脚本执行 ==========

bool ScriptManager::runScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    auto& infoPtr = it->second;

    // 如果已经在运行，先停止
    if (infoPtr->state == ScriptState::running) {
        stopScript(name);
    }

    // 创建或重用 Lua 状态
    if (!infoPtr->luaState) {
        infoPtr->luaState = std::make_unique<lua::LuaState>();
    }

    // 读取脚本文件
    std::ifstream file(infoPtr->config.path);
    if (!file.is_open()) {
        infoPtr->state = ScriptState::error;
        infoPtr->lastError = "failed to open file";
        triggerEvent(name, ScriptEvent::error, infoPtr->lastError);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    file.close();

    // 设置环境变量
    std::string envSetup;
    const std::unordered_map<std::string, std::string>& env = infoPtr->config.env;
    for (std::unordered_map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
        envSetup += "_G['" + it->first + "'] = '" + it->second + "';\n";
    }

    // 执行脚本
    std::string fullCode = envSetup + code;
    if (!infoPtr->luaState->doString(fullCode)) {
        infoPtr->state = ScriptState::error;
        infoPtr->lastError = infoPtr->luaState->getLastError();
        triggerEvent(name, ScriptEvent::error, infoPtr->lastError);
        return false;
    }

    infoPtr->state = ScriptState::running;
    infoPtr->lastLoaded = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    triggerEvent(name, ScriptEvent::started, "");
    return true;
}

bool ScriptManager::stopScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    if (it->second->state != ScriptState::running && it->second->state != ScriptState::paused) {
        return false;
    }

    it->second->state = ScriptState::loaded;
    triggerEvent(name, ScriptEvent::stopped, "");
    return true;
}

bool ScriptManager::pauseScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    if (it->second->state != ScriptState::running) {
        return false;
    }

    it->second->state = ScriptState::paused;
    return true;
}

bool ScriptManager::resumeScript(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return false;
    }

    if (it->second->state != ScriptState::paused) {
        return false;
    }

    it->second->state = ScriptState::running;
    return true;
}

bool ScriptManager::callFunction(const std::string& name, const std::string& func,
                                const std::vector<std::string>& args,
                                std::string* result) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end() || it->second->state != ScriptState::running) {
        return false;
    }

    lua_State* L = getLuaState(name);
    if (!L) {
        return false;
    }

    // 获取函数
    lua_getglobal(L, func.c_str());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;
    }

    // 压入参数
    for (const auto& arg : args) {
        lua_pushstring(L, arg.c_str());
    }

    // 调用函数
    int status = lua_pcall(L, static_cast<int>(args.size()), 1, 0);
    if (status != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        if (error) {
            it->second->lastError = error;
        }
        lua_pop(L, 1);
        return false;
    }

    // 获取结果
    if (result && lua_gettop(L) > 0) {
        if (lua_isstring(L, -1)) {
            *result = lua_tostring(L, -1);
        } else if (lua_isboolean(L, -1)) {
            *result = lua_toboolean(L, -1) ? "true" : "false";
        } else if (lua_isnumber(L, -1)) {
            *result = std::to_string(lua_tonumber(L, -1));
        } else {
            *result = "";
        }
    }

    lua_pop(L, 1);
    return true;
}

// ========== 配置管理 ==========

bool ScriptManager::loadConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (path.empty()) {
        return false;
    }

    // 根据扩展名选择解析器
    if (path.size() > 5) {
        std::string ext = path.substr(path.size() - 5);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".json") {
            return loadJsonConfig(path);
        }
    }

    if (path.size() > 4) {
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".ini" || ext == ".cfg") {
            return loadIniConfig(path);
        }
    }

    return false;
}

bool ScriptManager::saveConfig(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 简单实现：保存为 INI 格式
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : m_config) {
        file << pair.first << "=" << pair.second << "\n";
    }

    return true;
}

std::string ScriptManager::getConfig(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return it->second;
    }
    return defaultValue;
}

void ScriptManager::setConfig(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value;
}

std::string ScriptManager::getEnv(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_env.find(key);
    if (it != m_env.end()) {
        return it->second;
    }

#ifdef _WIN32
    char buffer[4096];
    if (GetEnvironmentVariableA(key.c_str(), buffer, sizeof(buffer)) > 0) {
        return buffer;
    }
#else
    const char* val = std::getenv(key.c_str());
    if (val) {
        return val;
    }
#endif

    return "";
}

void ScriptManager::setEnv(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_env[key] = value;
}

// ========== 状态查询 ==========

ScriptInfo* ScriptManager::getScriptInfo(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it != m_scripts.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> ScriptManager::getScriptNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    names.reserve(m_scripts.size());
    for (const auto& pair : m_scripts) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> ScriptManager::getRunningScripts() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    for (const auto& pair : m_scripts) {
        if (pair.second->state == ScriptState::running) {
            names.push_back(pair.first);
        }
    }
    return names;
}

bool ScriptManager::hasScript(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scripts.find(name) != m_scripts.end();
}

// ========== 事件回调 ==========

void ScriptManager::setEventCallback(ScriptEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCallback = std::move(callback);
}

// ========== 沙箱管理 ==========

void ScriptManager::setSandboxConfig(const SandboxConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sandboxConfig = config;
}

const SandboxConfig& ScriptManager::getSandboxConfig() const {
    return m_sandboxConfig;
}

// ========== 热加载控制 ==========

void ScriptManager::setAutoReload(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it != m_scripts.end()) {
        it->second->config.autoReload = enabled;
    }
}

void ScriptManager::setGlobalAutoReload(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_globalAutoReload = enabled;
}

void ScriptManager::startHotReload() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_hotReloadRunning) {
        return;
    }

    m_hotReloadRunning = true;
    m_hotReloadThread = std::thread([this]() {
        while (m_hotReloadRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            checkAllReloads();
        }
    });
}

void ScriptManager::stopHotReload() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_hotReloadRunning) {
        return;
    }

    m_hotReloadRunning = false;
    if (m_hotReloadThread.joinable()) {
        m_hotReloadThread.join();
    }
}

// ========== Lua 状态访问 ==========

lua_State* ScriptManager::getLuaState(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return nullptr;
    }

    if (it->second->luaState) {
        return it->second->luaState->getState();
    }

    return nullptr;
}

// ========== 私有方法 ==========

void ScriptManager::setupSandbox(lua_State* L, const std::string& name) {
    // TODO: 实现沙箱设置
}

bool ScriptManager::checkMemoryLimit(lua_State* L) {
    // TODO: 实现内存检查
    return true;
}

bool ScriptManager::checkInstructionLimit(lua_State* L) {
    // TODO: 实现指令计数检查
    return true;
}

bool ScriptManager::checkTimeLimit(const std::string& name) {
    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return true;
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    if (it->second->config.timeoutMs > 0 &&
        now - it->second->lastLoaded > it->second->config.timeoutMs) {
        return false;
    }

    return true;
}

uint64_t ScriptManager::getFileModifiedTime(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        LARGE_INTEGER time;
        time.HighPart = data.ftLastWriteTime.dwHighDateTime;
        time.LowPart = data.ftLastWriteTime.dwLowDateTime;
        return static_cast<uint64_t>(time.QuadPart / 10000 - 11644473600000LL);
    }
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_mtime) * 1000;
    }
#endif
    return 0;
}

bool ScriptManager::loadJsonConfig(const std::string& path) {
    // TODO: 使用 nlohmann/json 解析
    return false;
}

bool ScriptManager::loadIniConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // 跳过注释和空行
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // 解析 key=value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 去除空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            m_config[key] = value;
        }
    }

    return true;
}

void ScriptManager::triggerEvent(const std::string& name, ScriptEvent event, const std::string& message) {
    if (m_eventCallback) {
        m_eventCallback(name, event, message);
    }
}

// ========== Lua 初始化和管理 ==========

void ScriptManager::initLua() {
    // 创建主 Lua 状态
    m_mainL = luaL_newstate();
    if (!m_mainL) {
        return;
    }

    // 打开标准库
    luaL_openlibs(m_mainL);

    // 注册 script 模块（将 ScriptManager 实例存储到注册表中）
    lua_pushlightuserdata(m_mainL, this);
    lua_setfield(m_mainL, LUA_REGISTRYINDEX, "__wm_script_manager");

    // 注册 script 模块函数
    lua_newtable(m_mainL);

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        // 获取 ScriptManager 实例
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* path = luaL_optstring(L, 2, "");

        // 解析配置表
        ScriptConfig config;
        config.name = name;
        config.path = path;

        if (lua_istable(L, 3)) {
            lua_getfield(L, 3, "auto_reload");
            if (lua_isboolean(L, -1)) config.autoReload = lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, 3, "sandboxed");
            if (lua_isboolean(L, -1)) config.sandboxed = lua_toboolean(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, 3, "timeout");
            if (lua_isnumber(L, -1)) config.timeoutMs = lua_tointeger(L, -1);
            lua_pop(L, 1);

            // 解析环境变量
            lua_getfield(L, 3, "env");
            if (lua_istable(L, -1)) {
                lua_pushnil(L);
                while (lua_next(L, -2) != 0) {
                    const char* key = lua_tostring(L, -2);
                    const char* value = lua_tostring(L, -1);
                    if (key && value) {
                        config.env[key] = value;
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }

        bool success = mgr->loadScript(name, path, config);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "load");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->unloadScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "unload");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->reloadScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "reload");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->runScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "run");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->stopScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "stop");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->pauseScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "pause");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        bool success = mgr->resumeScript(name);
        lua_pushboolean(L, success);
        return 1;
    });
    lua_setfield(m_mainL, -2, "resume");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        auto names = mgr->getScriptNames();
        lua_newtable(L);
        for (size_t i = 0; i < names.size(); ++i) {
            lua_pushstring(L, names[i].c_str());
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    });
    lua_setfield(m_mainL, -2, "list");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        auto* info = mgr->getScriptInfo(name);
        bool isRunning = info && info->state == ScriptState::running;
        lua_pushboolean(L, isRunning);
        return 1;
    });
    lua_setfield(m_mainL, -2, "isRunning");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        auto* info = mgr->getScriptInfo(name);
        if (!info) {
            lua_pushnil(L);
            return 1;
        }

        const char* stateStr = "unloaded";
        switch (info->state) {
            case ScriptState::unloaded: stateStr = "unloaded"; break;
            case ScriptState::loaded: stateStr = "loaded"; break;
            case ScriptState::running: stateStr = "running"; break;
            case ScriptState::paused: stateStr = "paused"; break;
            case ScriptState::error: stateStr = "error"; break;
        }
        lua_pushstring(L, stateStr);
        return 1;
    });
    lua_setfield(m_mainL, -2, "getState");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* key = luaL_checkstring(L, 2);
        const char* value = luaL_checkstring(L, 3);
        mgr->setConfig(key, value);
        return 0;
    });
    lua_setfield(m_mainL, -2, "setConfig");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* key = luaL_checkstring(L, 2);
        const char* defaultValue = lua_optstring(L, 3, "");
        std::string result = mgr->getConfig(key, defaultValue);
        lua_pushstring(L, result.c_str());
        return 1;
    });
    lua_setfield(m_mainL, -2, "getConfig");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* key = luaL_checkstring(L, 2);
        const char* value = luaL_checkstring(L, 3);
        mgr->setEnv(key, value);
        return 0;
    });
    lua_setfield(m_mainL, -2, "setEnv");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* key = luaL_checkstring(L, 2);
        std::string result = mgr->getEnv(key);
        lua_pushstring(L, result.c_str());
        return 1;
    });
    lua_setfield(m_mainL, -2, "getEnv");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        bool enabled = lua_toboolean(L, 1);
        mgr->setGlobalAutoReload(enabled);
        return 0;
    });
    lua_setfield(m_mainL, -2, "setHotReload");

    lua_pushcfunction(m_mainL, [](lua_State* L) {
        lua_getfield(L, LUA_REGISTRYINDEX, "__wm_script_manager");
        auto* mgr = static_cast<ScriptManager*>(lua_touserdata(L, -1));
        lua_pop(L, 1);

        mgr->checkAllReloads();
        return 0;
    });
    lua_setfield(m_mainL, -2, "checkReload");

    lua_setglobal(m_mainL, "script");
}

} // namespace wingman
