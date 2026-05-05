#pragma once

// Lua 绑定层 - 将 C++ API 暴露给 Lua 脚本

#include <lua.hpp>
#include <string>
#include <vector>

namespace wingman::lua {

// Lua 状态管理类
class LuaState {
public:
    LuaState();
    ~LuaState();

    // 获取原始 Lua 状态
    lua_State* getState() { return L; }

    // 加载并执行脚本文件
    bool doFile(const std::string& filepath);

    // 加载并执行脚本字符串
    bool doString(const std::string& code);

    // 注册所有 API
    void registerAPIs();

    // 获取最后的错误信息
    std::string getLastError() const { return m_lastError; }

private:
    lua_State* L;
    std::string m_lastError;

    // 报告错误
    void reportError(int status);

    // 注册各个模块
    void registerScreenModule();
    void registerInputModule();
    void registerWindowModule();
    void registerProcessModule();
    void registerSystemModule();
    void registerUtilModule();
    void registerPerformanceModule();
    void registerScriptModule();
    void registerSecurityModule();
    void registerGameProfileModule();
    void registerVisionModule();
    void registerSmartTriggerModule();
    void registerBehaviorTreeModule();
    void registerOcrModule();
    void registerTrayModule();
    void registerConfigModule();
    void registerNodeModule();
    void registerVerificationModule();
};

// === Lua C API 绑定函数 ===

// Screen 模块
namespace screen {
    int capture(lua_State* L);
    int captureRegion(lua_State* L);
    int getPixel(lua_State* L);
    int findColor(lua_State* L);
    int findColors(lua_State* L);
    int getScreenWidth(lua_State* L);
    int getScreenHeight(lua_State* L);
    int findImage(lua_State* L);
}

// Input 模块
namespace input {
    int click(lua_State* L);
    int move(lua_State* L);
    int scroll(lua_State* L);
    int keyDown(lua_State* L);
    int keyUp(lua_State* L);
    int key(lua_State* L);
    int type(lua_State* L);
    int delay(lua_State* L);
    int randomDelay(lua_State* L);
}

// Window 模块
namespace window {
    int find(lua_State* L);
    int activate(lua_State* L);
    int getForeground(lua_State* L);
    int getTitle(lua_State* L);
    int getBounds(lua_State* L);
    int waitFor(lua_State* L);
}

// Process 模块
namespace process {
    int find(lua_State* L);
    int start(lua_State* L);
    int wait(lua_State* L);
    int terminate(lua_State* L);
    int exists(lua_State* L);
    int waitFor(lua_State* L);
}

// Util 模块
namespace util {
    int sleep(lua_State* L);
    int getTime(lua_State* L);
    int log(lua_State* L);
}

// Tray 模块
namespace tray {
    int create(lua_State* L);
    int get(lua_State* L);
    int remove(lua_State* L);
}

// TrayIcon 对象方法
namespace tray_icon {
    int setIcon(lua_State* L);
    int setTooltip(lua_State* L);
    int addItem(lua_State* L);
    int addSeparator(lua_State* L);
    int addSubmenu(lua_State* L);
    int removeItem(lua_State* L);
    int clearItems(lua_State* L);
    int show(lua_State* L);
    int hide(lua_State* L);
    int updateMenu(lua_State* L);
    int isVisible(lua_State* L);
    int destroy(lua_State* L);
}

} // namespace wingman::lua
