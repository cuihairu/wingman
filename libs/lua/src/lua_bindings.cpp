#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "lua_bindings.hpp"
#include "lua_extensions.hpp"

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"
#include "wingman/system.hpp"
#include "wingman/security.hpp"
#include "wingman/game_profile.hpp"
#include "wingman/performance.hpp"
#include "wingman/trigger.hpp"
#include "wingman/vision.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/ocr.hpp"
#include "wingman/config.hpp"
#include "wingman/node_status.hpp"
#include "wingman/verification.hpp"
#include "wingman/qrcode.hpp"
#include "wingman/ui_automation.hpp"
#include "wingman/version.hpp"
#include "wingman/script_manager.hpp"
#include "wingman/human.hpp"
#include "wingman/debugger/debugger.hpp"

#include <cstring>
#include <ctime>
#include <iostream>
#include <unordered_map>
#include <atomic>
#include <windows.h>

namespace wingman::lua {

// 全局 ScriptManager 实例（用于 script 模块）
static wingman::ScriptManager* g_scriptManager = nullptr;

// 设置 ScriptManager 实例
void LuaState::setScriptManager(wingman::ScriptManager* mgr) {
    g_scriptManager = mgr;
}

// 获取 ScriptManager 实例
static wingman::ScriptManager* getScriptManager() {
    return g_scriptManager;
}

// ============================================================================
// LuaState 实现
// ============================================================================

LuaState::LuaState() : L(nullptr) {
    L = luaL_newstate();
    if (L) {
        luaL_openlibs(L);  // 加载标准库
        registerAPIs();    // 注册 Wingman API
    }
}

LuaState::~LuaState() {
    if (L) {
        lua_close(L);
    }
}

bool LuaState::doFile(const std::string& filepath) {
    int status = luaL_loadfile(L, filepath.c_str());
    if (status != LUA_OK) {
        reportError(status);
        return false;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        reportError(status);
        return false;
    }

    return true;
}

bool LuaState::doString(const std::string& code) {
    int status = luaL_loadstring(L, code.c_str());
    if (status != LUA_OK) {
        reportError(status);
        return false;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        reportError(status);
        return false;
    }

    return true;
}

void LuaState::reportError(int status) {
    if (status == LUA_OK) return;

    const char* msg = lua_tostring(L, -1);
    if (msg) {
        m_lastError = msg;
        std::cerr << "Lua error: " << msg << "\n";
    }
    lua_pop(L, 1);
}

void LuaState::registerAPIs() {
    registerScreenModule();
    registerInputModule();
    registerWindowModule();
    registerProcessModule();
    registerSystemModule();
    registerUtilModule();
    registerPerformanceModule();
    registerScriptModule();
    registerSecurityModule();
    registerGameProfileModule();
    registerVisionModule();
    registerSmartTriggerModule();
    registerBehaviorTreeModule();
    registerOcrModule();
    registerConfigModule();
    registerNodeModule();
    registerVerificationModule();
    registerQRCodeModule();
    registerUIAutomationModule();
    registerHumanModule();
    registerDebuggerModule();

    // 注册扩展模块
    wingman::lua::registerHttpModule(L);
    wingman::lua::registerJsonModule(L);
    wingman::lua::registerKvModule(L);
    wingman::lua::registerOrchestrationModule(L);
    wingman::lua::registerTeamModule(L);
}

// ============================================================================
// 辅助函数
// ============================================================================

// 从栈获取 Rect
Rect getRect(lua_State* L, int index) {
    Rect rect;
    if (lua_istable(L, index)) {
        lua_getfield(L, index, "x");
        rect.x = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "y");
        rect.y = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "width");
        rect.width = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "height");
        rect.height = lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    return rect;
}

// 从栈获取 Color
Color getColor(lua_State* L, int index) {
    Color color;

    // 如果是整数，视为 0xRRGGBB
    if (lua_isinteger(L, index)) {
        color = Color::fromRGB(static_cast<uint32_t>(lua_tointeger(L, index)));
    }
    // 如果是表，获取 {r, g, b}
    else if (lua_istable(L, index)) {
        lua_getfield(L, index, "r");
        color.r = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, index, "g");
        color.g = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, index, "b");
        color.b = static_cast<uint8_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, index, "a");
        if (lua_isinteger(L, -1)) {
            color.a = static_cast<uint8_t>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);
    }

    return color;
}

// 将 Color 压入栈
void pushColor(lua_State* L, const Color& color) {
    lua_newtable(L);
    lua_pushinteger(L, color.r);
    lua_setfield(L, -2, "r");
    lua_pushinteger(L, color.g);
    lua_setfield(L, -2, "g");
    lua_pushinteger(L, color.b);
    lua_setfield(L, -2, "b");
    lua_pushinteger(L, color.a);
    lua_setfield(L, -2, "a");
}

// 将 Point 压入栈
void pushPoint(lua_State* L, const Point& point) {
    lua_newtable(L);
    lua_pushinteger(L, point.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, point.y);
    lua_setfield(L, -2, "y");
}

// ============================================================================
// Screen 模块
// ============================================================================

namespace screen {

int capture(lua_State* L) {
    auto bitmap = Screen::capture();
    if (!bitmap) {
        lua_pushnil(L);
        return 1;
    }

    // TODO: 返回 Bitmap 对象 (需要 userdata)
    lua_pushboolean(L, true);
    return 1;
}

int captureRegion(lua_State* L) {
    Rect region = getRect(L, 1);
    auto bitmap = Screen::capture(region);
    if (!bitmap) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushboolean(L, true);
    return 1;
}

int getPixel(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);

    Color color = Screen::getPixel(x, y);
    pushColor(L, color);
    return 1;
}

int findColor(lua_State* L) {
    Color color = getColor(L, 1);
    Rect region = getRect(L, 2);
    int tolerance = luaL_optinteger(L, 3, 10);

    Point result;
    if (Screen::findColor(color, region, tolerance, result)) {
        pushPoint(L, result);
        lua_pushboolean(L, true);
        return 2;
    }

    lua_pushnil(L);
    lua_pushboolean(L, false);
    return 2;
}

int findColors(lua_State* L) {
    Color color = getColor(L, 1);
    Rect region = getRect(L, 2);
    int tolerance = luaL_optinteger(L, 3, 10);
    int maxCount = luaL_optinteger(L, 4, 0);

    auto points = Screen::findColors(color, region, tolerance, maxCount);

    lua_newtable(L);
    for (size_t i = 0; i < points.size(); ++i) {
        pushPoint(L, points[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

int getScreenWidth(lua_State* L) {
    lua_pushinteger(L, Screen::getScreenWidth());
    return 1;
}

int getScreenHeight(lua_State* L) {
    lua_pushinteger(L, Screen::getScreenHeight());
    return 1;
}

int findImage(lua_State* L) {
    const char* imagePath = luaL_checkstring(L, 1);
    Rect region = getRect(L, 2);
    double threshold = luaL_optnumber(L, 3, 0.9);

    Point result;
    if (Screen::findImage(imagePath, region, threshold, result)) {
        pushPoint(L, result);
        lua_pushboolean(L, true);
        return 2;
    }

    lua_pushnil(L);
    lua_pushboolean(L, false);
    return 2;
}

} // namespace screen

void LuaState::registerScreenModule() {
    lua_newtable(L);

    lua_pushcfunction(L, screen::capture);
    lua_setfield(L, -2, "capture");

    lua_pushcfunction(L, screen::captureRegion);
    lua_setfield(L, -2, "captureRegion");

    lua_pushcfunction(L, screen::getPixel);
    lua_setfield(L, -2, "getPixel");

    lua_pushcfunction(L, screen::findColor);
    lua_setfield(L, -2, "findColor");

    lua_pushcfunction(L, screen::findColors);
    lua_setfield(L, -2, "findColors");

    lua_pushcfunction(L, screen::getScreenWidth);
    lua_setfield(L, -2, "getScreenWidth");

    lua_pushcfunction(L, screen::getScreenHeight);
    lua_setfield(L, -2, "getScreenHeight");

    lua_pushcfunction(L, screen::findImage);
    lua_setfield(L, -2, "findImage");

    lua_setglobal(L, "screen");
}

// ============================================================================
// Input 模块
// ============================================================================

namespace input {

int click(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int button = luaL_optinteger(L, 3, 0);  // 0=Left, 1=Middle, 2=Right

    MouseButton btn = MouseButton::Left;
    if (button == 1) btn = MouseButton::Middle;
    else if (button == 2) btn = MouseButton::Right;

    Input::click(x, y, btn);
    return 0;
}

int move(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int duration = luaL_optinteger(L, 3, 0);

    if (duration > 0) {
        Input::move(x, y, duration);
    } else {
        Input::move(x, y);
    }
    return 0;
}

int scroll(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int delta = luaL_checkinteger(L, 3);

    Input::scroll(x, y, delta);
    return 0;
}

int keyDown(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Input::keyDown(vkCode);
    return 0;
}

int keyUp(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Input::keyUp(vkCode);
    return 0;
}

int key(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Input::key(vkCode);
    return 0;
}

int type(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    int delay = luaL_optinteger(L, 2, 10);

    Input::type(text, delay);
    return 0;
}

int delay(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    Input::delay(ms);
    return 0;
}

int randomDelay(lua_State* L) {
    int min = luaL_checkinteger(L, 1);
    int max = luaL_checkinteger(L, 2);
    Input::randomDelay(min, max);
    return 0;
}

} // namespace input

void LuaState::registerInputModule() {
    lua_newtable(L);

    lua_pushcfunction(L, input::click);
    lua_setfield(L, -2, "click");

    lua_pushcfunction(L, input::move);
    lua_setfield(L, -2, "move");

    lua_pushcfunction(L, input::scroll);
    lua_setfield(L, -2, "scroll");

    lua_pushcfunction(L, input::keyDown);
    lua_setfield(L, -2, "keyDown");

    lua_pushcfunction(L, input::keyUp);
    lua_setfield(L, -2, "keyUp");

    lua_pushcfunction(L, input::key);
    lua_setfield(L, -2, "key");

    lua_pushcfunction(L, input::type);
    lua_setfield(L, -2, "type");

    lua_pushcfunction(L, input::delay);
    lua_setfield(L, -2, "delay");

    lua_pushcfunction(L, input::randomDelay);
    lua_setfield(L, -2, "randomDelay");

    lua_setglobal(L, "input");
}

// ============================================================================
// Window 模块
// ============================================================================

namespace window {

int find(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);

    WindowHandle hwnd = Window::find(title);
    if (hwnd) {
        lua_pushinteger(L, reinterpret_cast<lua_Integer>(hwnd));
        lua_pushboolean(L, true);
        return 2;
    }

    lua_pushnil(L);
    lua_pushboolean(L, false);
    return 2;
}

int activate(lua_State* L) {
    WindowHandle hwnd = reinterpret_cast<WindowHandle>(lua_tointeger(L, 1));
    bool result = Window::activate(hwnd);
    lua_pushboolean(L, result);
    return 1;
}

int getForeground(lua_State* L) {
    WindowHandle hwnd = Window::getForeground();
    lua_pushinteger(L, reinterpret_cast<lua_Integer>(hwnd));
    return 1;
}

int getTitle(lua_State* L) {
    WindowHandle hwnd = reinterpret_cast<WindowHandle>(lua_tointeger(L, 1));
    std::string title = Window::getTitle(hwnd);
    lua_pushstring(L, title.c_str());
    return 1;
}

int getBounds(lua_State* L) {
    WindowHandle hwnd = reinterpret_cast<WindowHandle>(lua_tointeger(L, 1));
    Rect bounds = Window::getBounds(hwnd);

    lua_newtable(L);
    lua_pushinteger(L, bounds.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, bounds.y);
    lua_setfield(L, -2, "y");
    lua_pushinteger(L, bounds.width);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, bounds.height);
    lua_setfield(L, -2, "height");

    return 1;
}

int waitFor(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    int timeout = luaL_optinteger(L, 2, 5000);

    bool result = Window::waitFor(title, timeout);
    lua_pushboolean(L, result);
    return 1;
}

} // namespace window

void LuaState::registerWindowModule() {
    lua_newtable(L);

    lua_pushcfunction(L, window::find);
    lua_setfield(L, -2, "find");

    lua_pushcfunction(L, window::activate);
    lua_setfield(L, -2, "activate");

    lua_pushcfunction(L, window::getForeground);
    lua_setfield(L, -2, "getForeground");

    lua_pushcfunction(L, window::getTitle);
    lua_setfield(L, -2, "getTitle");

    lua_pushcfunction(L, window::getBounds);
    lua_setfield(L, -2, "getBounds");

    lua_pushcfunction(L, window::waitFor);
    lua_setfield(L, -2, "waitFor");

    lua_setglobal(L, "window");
}

// ============================================================================
// Process 模块
// ============================================================================

namespace process {

int find(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    ProcessId pid = Process::find(name);
    if (pid) {
        lua_pushinteger(L, pid);
        lua_pushboolean(L, true);
        return 2;
    }

    lua_pushnil(L);
    lua_pushboolean(L, false);
    return 2;
}

int start(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* args = luaL_optstring(L, 2, "");
    const char* workingDir = luaL_optstring(L, 3, "");

    ProcessId pid = Process::start(path, args, workingDir);
    lua_pushinteger(L, pid);
    return 1;
}

int wait(lua_State* L) {
    ProcessId pid = luaL_checkinteger(L, 1);
    int timeout = luaL_optinteger(L, 2, 0);

    bool result = Process::wait(pid, timeout);
    lua_pushboolean(L, result);
    return 1;
}

int terminate(lua_State* L) {
    ProcessId pid = luaL_checkinteger(L, 1);
    bool force = lua_toboolean(L, 2);

    bool result = Process::terminate(pid, force);
    lua_pushboolean(L, result);
    return 1;
}

int exists(lua_State* L) {
    ProcessId pid = luaL_checkinteger(L, 1);

    bool result = Process::exists(pid);
    lua_pushboolean(L, result);
    return 1;
}

int waitFor(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int timeout = luaL_optinteger(L, 2, 5000);

    bool result = Process::waitFor(name, timeout);
    lua_pushboolean(L, result);
    return 1;
}

} // namespace process

void LuaState::registerProcessModule() {
    lua_newtable(L);

    lua_pushcfunction(L, process::find);
    lua_setfield(L, -2, "find");

    lua_pushcfunction(L, process::start);
    lua_setfield(L, -2, "start");

    lua_pushcfunction(L, process::wait);
    lua_setfield(L, -2, "wait");

    lua_pushcfunction(L, process::terminate);
    lua_setfield(L, -2, "terminate");

    lua_pushcfunction(L, process::exists);
    lua_setfield(L, -2, "exists");

    lua_pushcfunction(L, process::waitFor);
    lua_setfield(L, -2, "waitFor");

    lua_setglobal(L, "process");
}

// ============================================================================
// Util 模块
// ============================================================================

namespace util {

int sleep(lua_State* L) {
    int ms = luaL_checkinteger(L, 1);
    Input::delay(ms);
    return 0;
}

int getTime(lua_State* L) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    lua_pushinteger(L, ms);
    return 1;
}

int log(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    std::cout << "[Wingman] " << message << "\n";
    return 0;
}

} // namespace util

void LuaState::registerUtilModule() {
    lua_newtable(L);

    lua_pushcfunction(L, util::sleep);
    lua_setfield(L, -2, "sleep");

    lua_pushcfunction(L, util::getTime);
    lua_setfield(L, -2, "getTime");

    lua_pushcfunction(L, util::log);
    lua_setfield(L, -2, "log");

    lua_setglobal(L, "util");
}

// ============================================================================
// System 模块
// ============================================================================

namespace system {

int getCpuInfo(lua_State* L) {
    CpuInfo info = System::getCpuInfo();

    lua_newtable(L);
    lua_pushstring(L, info.vendor.c_str());
    lua_setfield(L, -2, "vendor");
    lua_pushstring(L, info.brand.c_str());
    lua_setfield(L, -2, "brand");
    lua_pushinteger(L, info.cores);
    lua_setfield(L, -2, "cores");
    lua_pushinteger(L, info.threads);
    lua_setfield(L, -2, "threads");
    lua_pushinteger(L, info.maxClock);
    lua_setfield(L, -2, "maxClock");
    lua_pushinteger(L, info.currentClock);
    lua_setfield(L, -2, "currentClock");
    lua_pushnumber(L, info.usage);
    lua_setfield(L, -2, "usage");
    lua_pushinteger(L, info.temperature);
    lua_setfield(L, -2, "temperature");

    return 1;
}

int getCpuUsage(lua_State* L) {
    lua_pushnumber(L, System::getCpuUsage());
    return 1;
}

int getMemoryInfo(lua_State* L) {
    MemoryInfo info = System::getMemoryInfo();

    lua_newtable(L);
    lua_pushinteger(L, info.total);
    lua_setfield(L, -2, "total");
    lua_pushinteger(L, info.available);
    lua_setfield(L, -2, "available");
    lua_pushinteger(L, info.used);
    lua_setfield(L, -2, "used");
    lua_pushnumber(L, info.usage);
    lua_setfield(L, -2, "usage");

    return 1;
}

int getDiskInfo(lua_State* L) {
    const char* drive = luaL_optstring(L, 1, "");

    if (drive && drive[0]) {
        // 单个驱动器
        DiskInfo info = System::getDiskInfo(drive);

        lua_newtable(L);
        lua_pushstring(L, info.drive.c_str());
        lua_setfield(L, -2, "drive");
        lua_pushinteger(L, info.total);
        lua_setfield(L, -2, "total");
        lua_pushinteger(L, info.free);
        lua_setfield(L, -2, "free");
        lua_pushinteger(L, info.used);
        lua_setfield(L, -2, "used");
        lua_pushnumber(L, info.usage);
        lua_setfield(L, -2, "usage");
        lua_pushstring(L, info.fileSystem.c_str());
        lua_setfield(L, -2, "fileSystem");
    } else {
        // 所有驱动器
        auto disks = System::getDiskInfo();

        lua_newtable(L);
        for (size_t i = 0; i < disks.size(); ++i) {
            lua_newtable(L);

            lua_pushstring(L, disks[i].drive.c_str());
            lua_setfield(L, -2, "drive");
            lua_pushinteger(L, disks[i].total);
            lua_setfield(L, -2, "total");
            lua_pushinteger(L, disks[i].free);
            lua_setfield(L, -2, "free");
            lua_pushinteger(L, disks[i].used);
            lua_setfield(L, -2, "used");
            lua_pushnumber(L, disks[i].usage);
            lua_setfield(L, -2, "usage");

            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
    }

    return 1;
}

int getGpuInfo(lua_State* L) {
    auto gpus = System::getGpuInfo();

    lua_newtable(L);
    for (size_t i = 0; i < gpus.size(); ++i) {
        lua_newtable(L);

        lua_pushstring(L, gpus[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, gpus[i].dedicatedMemory);
        lua_setfield(L, -2, "dedicatedMemory");
        lua_pushinteger(L, gpus[i].sharedMemory);
        lua_setfield(L, -2, "sharedMemory");
        lua_pushnumber(L, gpus[i].usage);
        lua_setfield(L, -2, "usage");
        lua_pushinteger(L, gpus[i].temperature);
        lua_setfield(L, -2, "temperature");

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

int getOsInfo(lua_State* L) {
    OsInfo info = System::getOsInfo();

    lua_newtable(L);
    lua_pushstring(L, info.platform.c_str());
    lua_setfield(L, -2, "platform");
    lua_pushstring(L, info.version.c_str());
    lua_setfield(L, -2, "version");
    lua_pushstring(L, info.build.c_str());
    lua_setfield(L, -2, "build");
    lua_pushstring(L, info.architecture.c_str());
    lua_setfield(L, -2, "architecture");
    lua_pushstring(L, info.computerName.c_str());
    lua_setfield(L, -2, "computerName");
    lua_pushstring(L, info.userName.c_str());
    lua_setfield(L, -2, "userName");

    return 1;
}

int getNetworkAdapters(lua_State* L) {
    auto adapters = System::getNetworkAdapters();

    lua_newtable(L);
    for (size_t i = 0; i < adapters.size(); ++i) {
        lua_newtable(L);

        lua_pushstring(L, adapters[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, adapters[i].description.c_str());
        lua_setfield(L, -2, "description");
        lua_pushstring(L, adapters[i].macAddress.c_str());
        lua_setfield(L, -2, "macAddress");
        lua_pushstring(L, adapters[i].ipAddress.c_str());
        lua_setfield(L, -2, "ipAddress");
        lua_pushboolean(L, adapters[i].isUp);
        lua_setfield(L, -2, "isUp");

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

int getDisplayInfo(lua_State* L) {
    auto displays = System::getDisplayInfo();

    lua_newtable(L);
    for (size_t i = 0; i < displays.size(); ++i) {
        lua_newtable(L);

        lua_pushinteger(L, displays[i].index);
        lua_setfield(L, -2, "index");
        lua_pushstring(L, displays[i].name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, displays[i].width);
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, displays[i].height);
        lua_setfield(L, -2, "height");
        lua_pushinteger(L, displays[i].refreshRate);
        lua_setfield(L, -2, "refreshRate");
        lua_pushboolean(L, displays[i].isPrimary);
        lua_setfield(L, -2, "isPrimary");

        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    return 1;
}

int getUptime(lua_State* L) {
    lua_pushinteger(L, System::getUptime());
    return 1;
}

int getDateTime(lua_State* L) {
    lua_pushstring(L, System::getDateTime().c_str());
    return 1;
}

int getTimeZone(lua_State* L) {
    lua_pushstring(L, System::getTimeZone().c_str());
    return 1;
}

int getProcessCount(lua_State* L) {
    lua_pushinteger(L, System::getProcessCount());
    return 1;
}

int getThreadCount(lua_State* L) {
    lua_pushinteger(L, System::getThreadCount());
    return 1;
}

} // namespace system

void LuaState::registerSystemModule() {
    lua_newtable(L);

    lua_pushcfunction(L, system::getCpuInfo);
    lua_setfield(L, -2, "getCpuInfo");

    lua_pushcfunction(L, system::getCpuUsage);
    lua_setfield(L, -2, "getCpuUsage");

    lua_pushcfunction(L, system::getMemoryInfo);
    lua_setfield(L, -2, "getMemoryInfo");

    lua_pushcfunction(L, system::getDiskInfo);
    lua_setfield(L, -2, "getDiskInfo");

    lua_pushcfunction(L, system::getGpuInfo);
    lua_setfield(L, -2, "getGpuInfo");

    lua_pushcfunction(L, system::getOsInfo);
    lua_setfield(L, -2, "getOsInfo");

    lua_pushcfunction(L, system::getNetworkAdapters);
    lua_setfield(L, -2, "getNetworkAdapters");

    lua_pushcfunction(L, system::getDisplayInfo);
    lua_setfield(L, -2, "getDisplayInfo");

    lua_pushcfunction(L, system::getUptime);
    lua_setfield(L, -2, "getUptime");

    lua_pushcfunction(L, system::getDateTime);
    lua_setfield(L, -2, "getDateTime");

    lua_pushcfunction(L, system::getTimeZone);
    lua_setfield(L, -2, "getTimeZone");

    lua_pushcfunction(L, system::getProcessCount);
    lua_setfield(L, -2, "getProcessCount");

    lua_pushcfunction(L, system::getThreadCount);
    lua_setfield(L, -2, "getThreadCount");

    lua_setglobal(L, "system");
}

// ============================================================================
// 性能优化模块
// ============================================================================

namespace perf {

static int setConfig(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    PerformanceConfig config;

    lua_getfield(L, 1, "enableImageCache");
    if (lua_isboolean(L, -1)) config.enableImageCache = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "maxCacheSize");
    if (lua_isinteger(L, -1)) config.maxCacheSize = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enableParallelProcessing");
    if (lua_isboolean(L, -1)) config.enableParallelProcessing = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "numThreads");
    if (lua_isinteger(L, -1)) config.numThreads = lua_tointeger(L, -1);
    lua_pop(L, 1);

    mgr.setConfig(config);
    return 0;
}

static int getConfig(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    const auto& config = mgr.getConfig();

    lua_newtable(L);

    lua_pushboolean(L, config.enableImageCache);
    lua_setfield(L, -2, "enableImageCache");

    lua_pushinteger(L, config.maxCacheSize);
    lua_setfield(L, -2, "maxCacheSize");

    lua_pushboolean(L, config.enableParallelProcessing);
    lua_setfield(L, -2, "enableParallelProcessing");

    lua_pushinteger(L, config.numThreads);
    lua_setfield(L, -2, "numThreads");

    return 1;
}

static int preloadImage(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    auto& mgr = PerformanceManager::instance();
    mgr.preloadImage(path);
    return 0;
}

static int clearCache(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    mgr.clearCache();
    return 0;
}

static int getCacheSize(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    lua_pushinteger(L, mgr.getCacheSize());
    return 1;
}

static int getCacheStats(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    lua_newtable(L);

    lua_pushinteger(L, mgr.getCacheSize());
    lua_setfield(L, -2, "size");

    lua_pushinteger(L, mgr.getCacheHits());
    lua_setfield(L, -2, "hits");

    lua_pushinteger(L, mgr.getCacheMisses());
    lua_setfield(L, -2, "misses");

    double hitRate = 0.0;
    size_t total = mgr.getCacheHits() + mgr.getCacheMisses();
    if (total > 0) {
        hitRate = (double)mgr.getCacheHits() / total * 100;
    }
    lua_pushnumber(L, hitRate);
    lua_setfield(L, -2, "hitRate");

    return 1;
}

static int fastFindImage(lua_State* L) {
    const char* imagePath = luaL_checkstring(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int width = luaL_checkinteger(L, 4);
    int height = luaL_checkinteger(L, 5);
    double threshold = luaL_checknumber(L, 6);

    auto& mgr = PerformanceManager::instance();
    Point result;
    bool found = mgr.fastFindImage(imagePath, Rect(x, y, width, height), threshold, result);

    if (found) {
        lua_newtable(L);
        lua_pushinteger(L, result.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, result.y);
        lua_setfield(L, -2, "y");
        return 1;
    }

    return 0;
}

static int parallelFindColors(lua_State* L) {
    uint32_t color = luaL_checkinteger(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    int width = luaL_checkinteger(L, 4);
    int height = luaL_checkinteger(L, 5);
    int tolerance = luaL_checkinteger(L, 6);
    int maxCount = 0;
    if (lua_gettop(L) >= 7) {
        maxCount = luaL_checkinteger(L, 7);
    }

    auto& mgr = PerformanceManager::instance();
    auto points = mgr.parallelFindColors(
        Color::fromRGB(color),
        Rect(x, y, width, height),
        tolerance,
        maxCount
    );

    lua_newtable(L);
    for (size_t i = 0; i < points.size(); ++i) {
        lua_newtable(L);
        lua_pushinteger(L, points[i].x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, points[i].y);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

static int getStats(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    auto stats = mgr.getStats();

    lua_newtable(L);

    lua_pushinteger(L, stats.totalCaptures);
    lua_setfield(L, -2, "totalCaptures");

    lua_pushinteger(L, stats.totalColorSearches);
    lua_setfield(L, -2, "totalColorSearches");

    lua_pushinteger(L, stats.totalImageSearches);
    lua_setfield(L, -2, "totalImageSearches");

    lua_pushnumber(L, stats.avgCaptureTime);
    lua_setfield(L, -2, "avgCaptureTime");

    lua_pushnumber(L, stats.avgColorSearchTime);
    lua_setfield(L, -2, "avgColorSearchTime");

    lua_pushnumber(L, stats.avgImageSearchTime);
    lua_setfield(L, -2, "avgImageSearchTime");

    return 1;
}

static int resetStats(lua_State* L) {
    auto& mgr = PerformanceManager::instance();
    mgr.resetStats();
    return 0;
}

} // namespace perf

void LuaState::registerPerformanceModule() {
    lua_newtable(L);

    lua_pushcfunction(L, perf::setConfig);
    lua_setfield(L, -2, "setConfig");

    lua_pushcfunction(L, perf::getConfig);
    lua_setfield(L, -2, "getConfig");

    lua_pushcfunction(L, perf::preloadImage);
    lua_setfield(L, -2, "preloadImage");

    lua_pushcfunction(L, perf::clearCache);
    lua_setfield(L, -2, "clearCache");

    lua_pushcfunction(L, perf::getCacheSize);
    lua_setfield(L, -2, "getCacheSize");

    lua_pushcfunction(L, perf::getCacheStats);
    lua_setfield(L, -2, "getCacheStats");

    lua_pushcfunction(L, perf::fastFindImage);
    lua_setfield(L, -2, "fastFindImage");

    lua_pushcfunction(L, perf::parallelFindColors);
    lua_setfield(L, -2, "parallelFindColors");

    lua_pushcfunction(L, perf::getStats);
    lua_setfield(L, -2, "getStats");

    lua_pushcfunction(L, perf::resetStats);
    lua_setfield(L, -2, "resetStats");

    lua_setglobal(L, "perf");
}

// ============================================================================
// Script 模块
// ============================================================================

namespace script {

// script.load(name, path, config)
static int load(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);

    // 可选的配置表
    bool autoReload = false;
    bool sandboxed = true;
    int timeoutMs = 30000;

    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "autoReload");
        if (lua_isboolean(L, -1)) autoReload = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "sandboxed");
        if (lua_isboolean(L, -1)) sandboxed = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 3, "timeout");
        if (lua_isnumber(L, -1)) timeoutMs = lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.unload(name)
static int unload(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.reload(name)
static int reload(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.run(name)
static int run(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.stop(name)
static int stop(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.pause(name)
static int pause(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.resume(name)
static int resume(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, true);
    return 1;
}

// script.list()
static int list(lua_State* L) {
    lua_newtable(L);
    // TODO: 实现 ScriptManager 集成
    lua_pushstring(L, "example");
    lua_pushboolean(L, true);
    lua_settable(L, -3);
    return 1;
}

// script.isRunning(name)
static int isRunning(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushboolean(L, false);
    return 1;
}

// script.getState(name)
static int getState(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    // TODO: 实现 ScriptManager 集成
    lua_pushstring(L, "loaded");
    return 1;
}

// script.setConfig(name, key, value)
static int setConfig(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);
    // TODO: 实现 ScriptManager 集成
    return 0;
}

// script.getConfig(name, key, default)
static int getConfig(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    const char* defaultValue = lua_tostring(L, 3);
    // TODO: 实现 ScriptManager 集成
    lua_pushstring(L, defaultValue ? defaultValue : "");
    return 1;
}

// script.setEnv(name, key, value)
static int setEnv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    const char* value = luaL_checkstring(L, 3);
    // TODO: 实现 ScriptManager 集成
    return 0;
}

// script.getEnv(name, key)
static int getEnv(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    // TODO: 实现 ScriptManager 集成
    lua_pushstring(L, "");
    return 1;
}

// script.setHotReload(enabled)
static int setHotReload(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    // TODO: 实现 ScriptManager 集成
    return 0;
}

// script.checkReload()
static int checkReload(lua_State* L) {
    // TODO: 实现 ScriptManager 集成
    return 0;
}

} // namespace script

void LuaState::registerScriptModule() {
    // 如果有 ScriptManager 实例，使用它提供的 script 模块
    // 否则使用默认的存根实现
    if (g_scriptManager && g_scriptManager->getMainLuaState()) {
        // ScriptManager 已经在它自己的 Lua 状态中注册了 script 模块
        // 我们需要将其导出到当前状态
        // 注意：这里简化处理，只使用存根实现并转发到 ScriptManager

        lua_newtable(L);

        // script.load(name, path, config)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            const char* path = luaL_optstring(L, 2, "");

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
        lua_setfield(L, -2, "load");

        // script.unload(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->unloadScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "unload");

        // script.reload(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->reloadScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "reload");

        // script.run(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->runScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "run");

        // script.stop(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->stopScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "stop");

        // script.pause(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->pauseScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "pause");

        // script.resume(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            bool success = mgr->resumeScript(name);
            lua_pushboolean(L, success);
            return 1;
        });
        lua_setfield(L, -2, "resume");

        // script.list()
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_newtable(L);
                return 1;
            }
            auto names = mgr->getScriptNames();
            lua_newtable(L);
            for (size_t i = 0; i < names.size(); ++i) {
                lua_pushstring(L, names[i].c_str());
                lua_rawseti(L, -2, static_cast<int>(i + 1));
            }
            return 1;
        });
        lua_setfield(L, -2, "list");

        // script.isRunning(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushboolean(L, false);
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            auto* info = mgr->getScriptInfo(name);
            bool isRunning = info && info->state == ScriptState::running;
            lua_pushboolean(L, isRunning);
            return 1;
        });
        lua_setfield(L, -2, "isRunning");

        // script.getState(name)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushstring(L, "unloaded");
                return 1;
            }
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
        lua_setfield(L, -2, "getState");

        // script.setConfig(name, key, value)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) return 0;
            const char* name = luaL_checkstring(L, 1);
            const char* key = luaL_checkstring(L, 2);
            const char* value = luaL_checkstring(L, 3);
            mgr->setConfig(key, value);
            return 0;
        });
        lua_setfield(L, -2, "setConfig");

        // script.getConfig(name, key, default)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushstring(L, "");
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            const char* key = luaL_checkstring(L, 2);
            const char* defaultValue = lua_optstring(L, 3, "");
            std::string result = mgr->getConfig(key, defaultValue);
            lua_pushstring(L, result.c_str());
            return 1;
        });
        lua_setfield(L, -2, "getConfig");

        // script.setEnv(name, key, value)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) return 0;
            const char* name = luaL_checkstring(L, 1);
            const char* key = luaL_checkstring(L, 2);
            const char* value = luaL_checkstring(L, 3);
            mgr->setEnv(key, value);
            return 0;
        });
        lua_setfield(L, -2, "setEnv");

        // script.getEnv(name, key)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) {
                lua_pushstring(L, "");
                return 1;
            }
            const char* name = luaL_checkstring(L, 1);
            const char* key = luaL_checkstring(L, 2);
            std::string result = mgr->getEnv(key);
            lua_pushstring(L, result.c_str());
            return 1;
        });
        lua_setfield(L, -2, "getEnv");

        // script.setHotReload(enabled)
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) return 0;
            bool enabled = lua_toboolean(L, 1);
            mgr->setGlobalAutoReload(enabled);
            return 0;
        });
        lua_setfield(L, -2, "setHotReload");

        // script.checkReload()
        lua_pushcfunction(L, [](lua_State* L) {
            auto* mgr = getScriptManager();
            if (!mgr) return 0;
            mgr->checkAllReloads();
            return 0;
        });
        lua_setfield(L, -2, "checkReload");

        lua_setglobal(L, "script");
    } else {
        // 存根实现（无 ScriptManager 时）
        lua_newtable(L);

        lua_pushcfunction(L, [](lua_State* L) {
            const char* name = luaL_checkstring(L, 1);
            luaL_error(L, "ScriptManager not initialized. Cannot load script '%s'", name);
            return 0;
        });
        lua_setfield(L, -2, "load");

        lua_pushcfunction(L, [](lua_State* L) {
            lua_pushboolean(L, false);
            return 1;
        });
        lua_setfield(L, -2, "unload");
        lua_setfield(L, -2, "reload");
        lua_setfield(L, -2, "run");
        lua_setfield(L, -2, "stop");
        lua_setfield(L, -2, "pause");
        lua_setfield(L, -2, "resume");

        lua_pushcfunction(L, [](lua_State* L) {
            lua_newtable(L);
            return 1;
        });
        lua_setfield(L, -2, "list");

        lua_pushcfunction(L, [](lua_State* L) {
            lua_pushboolean(L, false);
            return 1;
        });
        lua_setfield(L, -2, "isRunning");

        lua_pushcfunction(L, [](lua_State* L) {
            lua_pushstring(L, "unloaded");
            return 1;
        });
        lua_setfield(L, -2, "getState");

        lua_pushcfunction(L, [](lua_State* L) { return 0; });
        lua_setfield(L, -2, "setConfig");
        lua_setfield(L, -2, "setEnv");
        lua_setfield(L, -2, "setHotReload");
        lua_setfield(L, -2, "checkReload");

        lua_pushcfunction(L, [](lua_State* L) {
            lua_pushstring(L, "");
            return 1;
        });
        lua_setfield(L, -2, "getConfig");
        lua_setfield(L, -2, "getEnv");

        lua_setglobal(L, "script");
    }
}

// ============================================================================
// Security 模块
// ============================================================================

namespace security {

// security.getRandomDelay()
static int getRandomDelay(lua_State* L) {
    auto& mgr = SecurityManager::instance();
    int delay = mgr.getRandomDelay();
    lua_pushinteger(L, delay);
    return 1;
}

// security.getRandomOffset()
static int getRandomOffset(lua_State* L) {
    auto& mgr = SecurityManager::instance();
    auto [x, y] = mgr.getRandomOffset();
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

// security.isDebuggerPresent()
static int isDebuggerPresent(lua_State* L) {
    auto& mgr = SecurityManager::instance();
    lua_pushboolean(L, mgr.isDebuggerPresent());
    return 1;
}

// security.isRunningInVM()
static int isRunningInVM(lua_State* L) {
    auto& mgr = SecurityManager::instance();
    lua_pushboolean(L, mgr.isRunningInVM());
    return 1;
}

// security.verifyIntegrity()
static int verifyIntegrity(lua_State* L) {
    auto& mgr = SecurityManager::instance();
    lua_pushboolean(L, mgr.verifyIntegrity());
    return 1;
}

// security.hashString(str)
static int hashString(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    std::string hash = SecurityManager::hashString(str);
    lua_pushstring(L, hash.c_str());
    return 1;
}

// security.encryptString(str, key)
static int encryptString(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    std::string encrypted = SecurityManager::encryptString(str, key);
    lua_pushstring(L, encrypted.c_str());
    return 1;
}

// security.decryptString(str, key)
static int decryptString(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    std::string decrypted = SecurityManager::decryptString(str, key);
    lua_pushstring(L, decrypted.c_str());
    return 1;
}

// security.generateRandomString(length)
static int generateRandomString(lua_State* L) {
    int length = luaL_checkinteger(L, 1);
    std::string random = SecurityManager::generateRandomString(length);
    lua_pushstring(L, random.c_str());
    return 1;
}

// security.filterSensitive(str)
static int filterSensitive(lua_State* L) {
    const char* str = luaL_checkstring(L, 1);
    std::string filtered = SecurityManager::filterSensitive(str);
    lua_pushstring(L, filtered.c_str());
    return 1;
}

} // namespace security

void LuaState::registerSecurityModule() {
    lua_newtable(L);

    lua_pushcfunction(L, security::getRandomDelay);
    lua_setfield(L, -2, "getRandomDelay");

    lua_pushcfunction(L, security::getRandomOffset);
    lua_setfield(L, -2, "getRandomOffset");

    lua_pushcfunction(L, security::isDebuggerPresent);
    lua_setfield(L, -2, "isDebuggerPresent");

    lua_pushcfunction(L, security::isRunningInVM);
    lua_setfield(L, -2, "isRunningInVM");

    lua_pushcfunction(L, security::verifyIntegrity);
    lua_setfield(L, -2, "verifyIntegrity");

    lua_pushcfunction(L, security::hashString);
    lua_setfield(L, -2, "hashString");

    lua_pushcfunction(L, security::encryptString);
    lua_setfield(L, -2, "encryptString");

    lua_pushcfunction(L, security::decryptString);
    lua_setfield(L, -2, "decryptString");

    lua_pushcfunction(L, security::generateRandomString);
    lua_setfield(L, -2, "generateRandomString");

    lua_pushcfunction(L, security::filterSensitive);
    lua_setfield(L, -2, "filterSensitive");

    lua_setglobal(L, "security");
}

// ============================================================================
// GameProfile 模块
// ============================================================================

namespace gameprofile {

// gameprofile.load(id)
static int load(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    lua_pushboolean(L, mgr.loadProfile(id));
    return 1;
}

// gameprofile.save(id)
static int save(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    auto* profile = mgr.getProfile(id);
    if (profile) {
        lua_pushboolean(L, mgr.saveProfile(*profile));
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// gameprofile.get(id)
static int get(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    const auto* profile = mgr.getProfile(id);
    if (profile) {
        lua_pushstring(L, profile->name.c_str());
        lua_pushstring(L, profile->window.title.c_str());
        return 2;
    }
    lua_pushnil(L);
    return 1;
}

// gameprofile.getActive()
static int getActive(lua_State* L) {
    auto& mgr = GameProfileManager::instance();
    const auto* profile = mgr.getActiveProfile();
    if (profile) {
        lua_pushstring(L, profile->id.c_str());
        lua_pushstring(L, profile->name.c_str());
        return 2;
    }
    lua_pushnil(L);
    return 1;
}

// gameprofile.setActive(id)
static int setActive(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    lua_pushboolean(L, mgr.setActiveProfile(id));
    return 1;
}

// gameprofile.list()
static int list(lua_State* L) {
    auto& mgr = GameProfileManager::instance();
    auto ids = mgr.getProfileIds();

    lua_newtable(L);
    for (size_t i = 0; i < ids.size(); ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, ids[i].c_str());
        lua_settable(L, -3);
    }
    return 1;
}

// gameprofile.findByWindow(title)
static int findByWindow(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    auto* profile = mgr.findProfileByWindow(title);
    if (profile) {
        lua_pushstring(L, profile->id.c_str());
        lua_pushstring(L, profile->name.c_str());
        return 2;
    }
    lua_pushnil(L);
    return 1;
}

// gameprofile.createTemplate(name)
static int createTemplate(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    auto profile = mgr.createTemplate(name);
    lua_pushstring(L, profile.id.c_str());
    lua_pushstring(L, profile.name.c_str());
    return 2;
}

// gameprofile.setProfilesDirectory(dir)
static int setProfilesDirectory(lua_State* L) {
    const char* dir = luaL_checkstring(L, 1);
    auto& mgr = GameProfileManager::instance();
    mgr.setProfilesDirectory(dir);
    return 0;
}

// gameprofile.scan()
static int scan(lua_State* L) {
    auto& mgr = GameProfileManager::instance();
    mgr.scanProfilesDirectory();
    return 0;
}

} // namespace gameprofile

void LuaState::registerGameProfileModule() {
    lua_newtable(L);

    lua_pushcfunction(L, gameprofile::load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, gameprofile::save);
    lua_setfield(L, -2, "save");

    lua_pushcfunction(L, gameprofile::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, gameprofile::getActive);
    lua_setfield(L, -2, "getActive");

    lua_pushcfunction(L, gameprofile::setActive);
    lua_setfield(L, -2, "setActive");

    lua_pushcfunction(L, gameprofile::list);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, gameprofile::findByWindow);
    lua_setfield(L, -2, "findByWindow");

    lua_pushcfunction(L, gameprofile::createTemplate);
    lua_setfield(L, -2, "createTemplate");

    lua_pushcfunction(L, gameprofile::setProfilesDirectory);
    lua_setfield(L, -2, "setProfilesDirectory");

    lua_pushcfunction(L, gameprofile::scan);
    lua_setfield(L, -2, "scan");

    lua_setglobal(L, "gameprofile");
}

// ============================================================================
// Vision 模块
// ============================================================================

namespace vision {

int findColor(lua_State* L) {
    Color color = getColor(L, 1);
    int tolerance = luaL_optinteger(L, 2, 0);
    Rect region = getRect(L, 3);

    auto result = Vision::findColor(color, tolerance, region);
    if (result.has_value()) {
        pushPoint(L, *result);
        return 1;
    }
    return 0;
}

int findAllColors(lua_State* L) {
    Color color = getColor(L, 1);
    int tolerance = luaL_optinteger(L, 2, 0);
    Rect region = getRect(L, 3);

    auto results = Vision::findAllColors(color, tolerance, region);
    lua_newtable(L);
    for (size_t i = 0; i < results.size(); i++) {
        pushPoint(L, results[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int hasColor(lua_State* L) {
    Color color = getColor(L, 1);
    int tolerance = luaL_optinteger(L, 2, 0);
    Rect region = getRect(L, 3);

    bool found = Vision::hasColor(color, tolerance, region);
    lua_pushboolean(L, found);
    return 1;
}

int getDominantColor(lua_State* L) {
    Rect region = getRect(L, 1);
    Color color = Vision::getDominantColor(region);
    pushColor(L, color);
    return 1;
}

int findImage(lua_State* L) {
    const char* templatePath = luaL_checkstring(L, 1);
    double threshold = luaL_optnumber(L, 2, 0.8);
    Rect region = getRect(L, 3);

    auto result = Vision::findImage(templatePath, region, threshold);
    lua_newtable(L);
    lua_pushboolean(L, result.found);
    lua_setfield(L, -2, "found");
    if (result.found) {
        pushPoint(L, result.position);
        lua_setfield(L, -2, "position");
        lua_pushnumber(L, result.confidence);
        lua_setfield(L, -2, "confidence");
    }
    return 1;
}

int detectEdges(lua_State* L) {
    Rect region = getRect(L, 1);
    double threshold1 = luaL_optnumber(L, 2, 50.0);
    double threshold2 = luaL_optnumber(L, 3, 150.0);

    auto edges = Vision::detectEdges(region, threshold1, threshold2);
    lua_newtable(L);
    for (size_t i = 0; i < edges.size(); i++) {
        pushPoint(L, edges[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int captureRegion(lua_State* L) {
    Rect region = getRect(L, 1);
    const char* outputPath = luaL_checkstring(L, 2);

    bool success = Vision::captureRegion(region, outputPath);
    lua_pushboolean(L, success);
    return 1;
}

} // namespace vision

void LuaState::registerVisionModule() {
    lua_newtable(L);

    lua_pushcfunction(L, vision::findColor);
    lua_setfield(L, -2, "findColor");

    lua_pushcfunction(L, vision::findAllColors);
    lua_setfield(L, -2, "findAllColors");

    lua_pushcfunction(L, vision::hasColor);
    lua_setfield(L, -2, "hasColor");

    lua_pushcfunction(L, vision::getDominantColor);
    lua_setfield(L, -2, "getDominantColor");

    lua_pushcfunction(L, vision::findImage);
    lua_setfield(L, -2, "findImage");

    lua_pushcfunction(L, vision::detectEdges);
    lua_setfield(L, -2, "detectEdges");

    lua_pushcfunction(L, vision::captureRegion);
    lua_setfield(L, -2, "captureRegion");

    lua_setglobal(L, "vision");
}

// ============================================================================
// OCR 模块
// ============================================================================

namespace ocr {

int recognize(lua_State* L) {
    Rect region = getRect(L, 1);

    auto result = OCR::recognize(region);
    lua_newtable(L);
    lua_pushboolean(L, result.success);
    lua_setfield(L, -2, "success");
    lua_pushstring(L, result.text.c_str());
    lua_setfield(L, -2, "text");
    lua_pushnumber(L, result.confidence);
    lua_setfield(L, -2, "confidence");
    return 1;
}

int recognizeText(lua_State* L) {
    Rect region = getRect(L, 1);

    auto result = OCR::recognize(region);
    if (result.success) {
        lua_pushstring(L, result.text.c_str());
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

} // namespace ocr

void LuaState::registerOcrModule() {
    lua_newtable(L);

    lua_pushcfunction(L, ocr::recognize);
    lua_setfield(L, -2, "recognize");

    lua_pushcfunction(L, ocr::recognizeText);
    lua_setfield(L, -2, "recognizeText");

    lua_setglobal(L, "ocr");
}

// ============================================================================
// SmartTrigger 模块
// ============================================================================

namespace smarttrigger {

// 存储 trigger 指针的 userdata 管理
static std::map<std::string, std::shared_ptr<SmartTrigger>> triggers;

int create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto trigger = SmartTriggerManager::instance().createTrigger(name);
    if (trigger) {
        triggers[name] = trigger;
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

int addCondition(lua_State* L) {
    const char* triggerName = luaL_checkstring(L, 1);
    const char* conditionType = luaL_checkstring(L, 2);

    auto trigger = SmartTriggerManager::instance().getTrigger(triggerName);
    if (!trigger) {
        return luaL_error(L, "Trigger '%s' not found", triggerName);
    }

    TriggerCondition condition;
    std::string typeStr(conditionType);

    if (typeStr == "COLOR_FOUND") {
        condition.type = TriggerConditionType::COLOR_FOUND;
        condition.targetColor = getColor(L, 3);
        condition.tolerance = luaL_optinteger(L, 4, 0);
    } else if (typeStr == "COLOR_NOT_FOUND") {
        condition.type = TriggerConditionType::COLOR_NOT_FOUND;
        condition.targetColor = getColor(L, 3);
        condition.tolerance = luaL_optinteger(L, 4, 0);
    } else if (typeStr == "IMAGE_FOUND") {
        condition.type = TriggerConditionType::IMAGE_FOUND;
        condition.templatePath = luaL_checkstring(L, 3);
        condition.threshold = luaL_optnumber(L, 4, 0.8);
    } else if (typeStr == "TEXT_FOUND") {
        condition.type = TriggerConditionType::TEXT_FOUND;
        condition.targetText = luaL_checkstring(L, 3);
    } else if (typeStr == "OCR_CONTAINS") {
        condition.type = TriggerConditionType::OCR_CONTAINS;
        condition.targetText = luaL_checkstring(L, 3);
    } else {
        return luaL_error(L, "Unknown condition type: %s", conditionType);
    }

    condition.searchRegion = getRect(L, 5);

    trigger->addCondition(condition);
    return 0;
}

int addAction(lua_State* L) {
    const char* triggerName = luaL_checkstring(L, 1);
    const char* actionType = luaL_checkstring(L, 2);

    auto trigger = SmartTriggerManager::instance().getTrigger(triggerName);
    if (!trigger) {
        return luaL_error(L, "Trigger '%s' not found", triggerName);
    }

    TriggerAction action;
    std::string typeStr(actionType);

    if (typeStr == "CLICK") {
        action.type = TriggerActionType::CLICK;
        action.clickPosition = {static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4))};
    } else if (typeStr == "KEY_PRESS") {
        action.type = TriggerActionType::KEY_PRESS;
        action.keyCode = static_cast<int>(luaL_checkinteger(L, 3));
    } else if (typeStr == "WAIT") {
        action.type = TriggerActionType::WAIT;
        action.waitMs = luaL_checkinteger(L, 3);
    } else if (typeStr == "LOG") {
        action.type = TriggerActionType::LOG;
        action.logMessage = luaL_checkstring(L, 3);
    } else if (typeStr == "STOP") {
        action.type = TriggerActionType::STOP;
    } else {
        return luaL_error(L, "Unknown action type: %s", actionType);
    }

    trigger->addAction(action);
    return 0;
}

int start(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto trigger = SmartTriggerManager::instance().getTrigger(name);
    if (!trigger) {
        return luaL_error(L, "Trigger '%s' not found", name);
    }

    bool success = trigger->start();
    lua_pushboolean(L, success);
    return 1;
}

int stop(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto trigger = SmartTriggerManager::instance().getTrigger(name);
    if (trigger) {
        trigger->stop();
    }

    return 0;
}

int remove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    SmartTriggerManager::instance().removeTrigger(name);
    triggers.erase(name);
    return 0;
}

int setCheckInterval(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int interval = luaL_checkinteger(L, 2);

    auto trigger = SmartTriggerManager::instance().getTrigger(name);
    if (trigger) {
        trigger->setCheckInterval(interval);
    }

    return 0;
}

int setMaxTriggers(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int max = luaL_checkinteger(L, 2);

    auto trigger = SmartTriggerManager::instance().getTrigger(name);
    if (trigger) {
        trigger->setMaxTriggers(max);
    }

    return 0;
}

int getTriggerCount(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto trigger = SmartTriggerManager::instance().getTrigger(name);
    if (trigger) {
        lua_pushinteger(L, trigger->getTriggerCount());
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

} // namespace smarttrigger

void LuaState::registerSmartTriggerModule() {
    lua_newtable(L);

    lua_pushcfunction(L, smarttrigger::create);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, smarttrigger::addCondition);
    lua_setfield(L, -2, "addCondition");

    lua_pushcfunction(L, smarttrigger::addAction);
    lua_setfield(L, -2, "addAction");

    lua_pushcfunction(L, smarttrigger::start);
    lua_setfield(L, -2, "start");

    lua_pushcfunction(L, smarttrigger::stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, smarttrigger::remove);
    lua_setfield(L, -2, "remove");

    lua_pushcfunction(L, smarttrigger::setCheckInterval);
    lua_setfield(L, -2, "setCheckInterval");

    lua_pushcfunction(L, smarttrigger::setMaxTriggers);
    lua_setfield(L, -2, "setMaxTriggers");

    lua_pushcfunction(L, smarttrigger::getTriggerCount);
    lua_setfield(L, -2, "getTriggerCount");

    lua_setglobal(L, "smarttrigger");
}

// ============================================================================
// BehaviorTree 模块
// ============================================================================

namespace behaviortree {

// 存储 tree 指针
static std::map<std::string, std::shared_ptr<BehaviorTree>> trees;

int create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto tree = BehaviorTreeManager::instance().createTree(name);
    if (tree) {
        trees[name] = tree;
        lua_pushboolean(L, true);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

int sequence(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");
    lua_pushlightuserdata(L, (void*)BehaviorTree::sequence(name).get());
    return 1;
}

int selector(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");
    lua_pushlightuserdata(L, (void*)BehaviorTree::selector(name).get());
    return 1;
}

int condition(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");
    // 对于条件节点，我们需要一个回调引用
    // 这里简化处理，返回一个标记
    lua_pushstring(L, "condition");
    lua_pushstring(L, name);
    return 2;
}

int action(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");
    lua_pushstring(L, "action");
    lua_pushstring(L, name);
    return 2;
}

int setRoot(lua_State* L) {
    const char* treeName = luaL_checkstring(L, 1);
    // 这里简化处理，实际需要更复杂的节点构建逻辑
    return 0;
}

int tick(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto tree = BehaviorTreeManager::instance().getTree(name);
    if (!tree) {
        return luaL_error(L, "Tree '%s' not found", name);
    }

    NodeStatus status = tree->tick();

    const char* statusStr = "RUNNING";
    if (status == NodeStatus::SUCCESS) statusStr = "SUCCESS";
    else if (status == NodeStatus::FAILURE) statusStr = "FAILURE";

    lua_pushstring(L, statusStr);
    return 1;
}

int remove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    BehaviorTreeManager::instance().removeTree(name);
    trees.erase(name);
    return 0;
}

} // namespace behaviortree

void LuaState::registerBehaviorTreeModule() {
    lua_newtable(L);

    lua_pushcfunction(L, behaviortree::create);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, behaviortree::sequence);
    lua_setfield(L, -2, "sequence");

    lua_pushcfunction(L, behaviortree::selector);
    lua_setfield(L, -2, "selector");

    lua_pushcfunction(L, behaviortree::condition);
    lua_setfield(L, -2, "condition");

    lua_pushcfunction(L, behaviortree::action);
    lua_setfield(L, -2, "action");

    lua_pushcfunction(L, behaviortree::tick);
    lua_setfield(L, -2, "tick");

    lua_pushcfunction(L, behaviortree::remove);
    lua_setfield(L, -2, "remove");

    lua_setglobal(L, "bt");
}

// ============================================================================
// Config 模块
// ============================================================================

namespace config {

// 全局 ConfigManager 实例（使用默认配置目录）
static std::unique_ptr<ConfigManager> g_configManager;

// 确保 ConfigManager 已初始化
static ConfigManager* getConfigManager() {
    if (!g_configManager) {
        g_configManager = std::make_unique<ConfigManager>("config");
    }
    return g_configManager.get();
}

// getServer() -> table
int getServer(lua_State* L) {
    auto* config = getConfigManager();
    ServerConfig serverConfig = config->getServerConfig();

    lua_newtable(L);
    lua_pushstring(L, serverConfig.host.c_str());
    lua_setfield(L, -2, "host");

    lua_pushinteger(L, serverConfig.port);
    lua_setfield(L, -2, "port");

    lua_pushstring(L, serverConfig.username.c_str());
    lua_setfield(L, -2, "username");

    lua_pushstring(L, serverConfig.password.c_str());
    lua_setfield(L, -2, "password");

    lua_pushboolean(L, serverConfig.autoConnect);
    lua_setfield(L, -2, "autoConnect");

    lua_pushboolean(L, serverConfig.serverControlled);
    lua_setfield(L, -2, "serverControlled");

    return 1;
}

// setServer(table)
int setServer(lua_State* L) {
    auto* config = getConfigManager();

    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "setServer: 期望 table 参数");
        lua_error(L);
        return 1;
    }

    ServerConfig serverConfig;

    lua_getfield(L, 1, "host");
    if (lua_isstring(L, -1)) {
        serverConfig.host = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "port");
    if (lua_isinteger(L, -1)) {
        serverConfig.port = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "username");
    if (lua_isstring(L, -1)) {
        serverConfig.username = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "password");
    if (lua_isstring(L, -1)) {
        serverConfig.password = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "autoConnect");
    if (lua_isboolean(L, -1)) {
        serverConfig.autoConnect = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "serverControlled");
    if (lua_isboolean(L, -1)) {
        serverConfig.serverControlled = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    config->setServerConfig(serverConfig);
    return 0;
}


// get(key) -> string
int get(lua_State* L) {
    auto* config = getConfigManager();
    const char* key = luaL_checkstring(L, 1);

    auto value = config->get(key);
    if (value.has_value()) {
        lua_pushstring(L, value.value().c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// set(key, value)
int set(lua_State* L) {
    auto* config = getConfigManager();
    const char* key = luaL_checkstring(L, 1);

    std::string value;
    if (lua_isboolean(L, 2)) {
        value = lua_toboolean(L, 2) ? "true" : "false";
    } else if (lua_isinteger(L, 2)) {
        value = std::to_string(lua_tointeger(L, 2));
    } else if (lua_isnumber(L, 2)) {
        value = std::to_string(lua_tonumber(L, 2));
    } else {
        value = luaL_checkstring(L, 2);
    }

    config->set(key, value);
    return 0;
}

// remove(key)
int remove(lua_State* L) {
    auto* config = getConfigManager();
    const char* key = luaL_checkstring(L, 1);

    bool removed = config->remove(key);
    lua_pushboolean(L, removed);
    return 1;
}

// getAutoRun() -> table
int getAutoRun(lua_State* L) {
    auto* config = getConfigManager();
    AutoRunConfig autoRunConfig = config->getAutoRunConfig();

    lua_newtable(L);
    lua_pushboolean(L, autoRunConfig.enabled);
    lua_setfield(L, -2, "enabled");

    lua_pushstring(L, autoRunConfig.scriptPath.c_str());
    lua_setfield(L, -2, "scriptPath");

    lua_pushinteger(L, autoRunConfig.delaySeconds);
    lua_setfield(L, -2, "delaySeconds");

    lua_pushboolean(L, autoRunConfig.repeat);
    lua_setfield(L, -2, "repeat");

    lua_pushinteger(L, autoRunConfig.repeatInterval);
    lua_setfield(L, -2, "repeatInterval");

    return 1;
}

// setAutoRun(table)
int setAutoRun(lua_State* L) {
    auto* config = getConfigManager();

    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "setAutoRun: 期望 table 参数");
        lua_error(L);
        return 1;
    }

    AutoRunConfig autoRunConfig;

    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) {
        autoRunConfig.enabled = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "scriptPath");
    if (lua_isstring(L, -1)) {
        autoRunConfig.scriptPath = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "delaySeconds");
    if (lua_isinteger(L, -1)) {
        autoRunConfig.delaySeconds = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "repeat");
    if (lua_isboolean(L, -1)) {
        autoRunConfig.repeat = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "repeatInterval");
    if (lua_isinteger(L, -1)) {
        autoRunConfig.repeatInterval = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    config->setAutoRunConfig(autoRunConfig);
    return 0;
}

// getHeartbeat() -> table
int getHeartbeat(lua_State* L) {
    auto* config = getConfigManager();
    HeartbeatConfig heartbeatConfig = config->getHeartbeatConfig();

    lua_newtable(L);
    lua_pushboolean(L, heartbeatConfig.enabled);
    lua_setfield(L, -2, "enabled");

    lua_pushinteger(L, heartbeatConfig.intervalSeconds);
    lua_setfield(L, -2, "intervalSeconds");

    lua_pushinteger(L, heartbeatConfig.timeoutSeconds);
    lua_setfield(L, -2, "timeoutSeconds");

    return 1;
}

// setHeartbeat(table)
int setHeartbeat(lua_State* L) {
    auto* config = getConfigManager();

    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "setHeartbeat: 期望 table 参数");
        lua_error(L);
        return 1;
    }

    HeartbeatConfig heartbeatConfig;

    lua_getfield(L, 1, "enabled");
    if (lua_isboolean(L, -1)) {
        heartbeatConfig.enabled = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "intervalSeconds");
    if (lua_isinteger(L, -1)) {
        heartbeatConfig.intervalSeconds = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "timeoutSeconds");
    if (lua_isinteger(L, -1)) {
        heartbeatConfig.timeoutSeconds = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    config->setHeartbeatConfig(heartbeatConfig);
    return 0;
}

// getGames() -> table
int getGames(lua_State* L) {
    auto* config = getConfigManager();
    std::vector<GameConfig> games = config->getGameConfigList();

    lua_newtable(L);
    for (size_t i = 0; i < games.size(); i++) {
        const auto& game = games[i];

        lua_newtable(L);
        lua_pushstring(L, game.name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, game.path.c_str());
        lua_setfield(L, -2, "path");
        lua_pushstring(L, game.args.c_str());
        lua_setfield(L, -2, "args");
        lua_pushstring(L, game.workingDir.c_str());
        lua_setfield(L, -2, "workingDir");
        lua_pushboolean(L, game.autoStart);
        lua_setfield(L, -2, "autoStart");
        lua_pushstring(L, game.scriptPath.c_str());
        lua_setfield(L, -2, "scriptPath");
        lua_pushstring(L, game.windowTitle.c_str());
        lua_setfield(L, -2, "windowTitle");
        lua_pushinteger(L, game.delaySeconds);
        lua_setfield(L, -2, "delaySeconds");
        lua_pushboolean(L, game.autoRestart);
        lua_setfield(L, -2, "autoRestart");
        lua_pushinteger(L, game.restartDelay);
        lua_setfield(L, -2, "restartDelay");
        lua_pushinteger(L, game.maxRestarts);
        lua_setfield(L, -2, "maxRestarts");

        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

// addGame(table)
int addGame(lua_State* L) {
    auto* config = getConfigManager();

    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "addGame: 期望 table 参数");
        lua_error(L);
        return 1;
    }

    GameConfig gameConfig;

    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1)) gameConfig.name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "path");
    if (lua_isstring(L, -1)) gameConfig.path = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "args");
    if (lua_isstring(L, -1)) gameConfig.args = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "workingDir");
    if (lua_isstring(L, -1)) gameConfig.workingDir = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "autoStart");
    if (lua_isboolean(L, -1)) gameConfig.autoStart = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "scriptPath");
    if (lua_isstring(L, -1)) gameConfig.scriptPath = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "windowTitle");
    if (lua_isstring(L, -1)) gameConfig.windowTitle = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "delaySeconds");
    if (lua_isinteger(L, -1)) gameConfig.delaySeconds = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "autoRestart");
    if (lua_isboolean(L, -1)) gameConfig.autoRestart = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "restartDelay");
    if (lua_isinteger(L, -1)) gameConfig.restartDelay = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "maxRestarts");
    if (lua_isinteger(L, -1)) gameConfig.maxRestarts = lua_tointeger(L, -1);
    lua_pop(L, 1);

    bool success = config->addGameConfig(gameConfig);
    lua_pushboolean(L, success);
    return 1;
}

// removeGame(name)
int removeGame(lua_State* L) {
    auto* config = getConfigManager();
    const char* name = luaL_checkstring(L, 1);

    bool removed = config->removeGameConfig(name);
    lua_pushboolean(L, removed);
    return 1;
}

// startGame(name)
int startGame(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto* config = getConfigManager();
    std::vector<GameConfig> games = config->getGameConfigList();

    for (const auto& game : games) {
        if (game.name == name) {
            // 启动游戏进程
            std::string command = "\"" + game.path + "\"";
            if (!game.args.empty()) {
                command += " " + game.args;
            }

            std::cout << "[GAME] Starting: " << command << "\n";
            std::cout.flush();

            // 使用 Windows API 启动进程
            STARTUPINFOA si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);

            std::string workingDir = game.workingDir.empty() ?
                game.path.substr(0, game.path.find_last_of("\\/")) : game.workingDir;

            BOOL result = CreateProcessA(
                nullptr,
                const_cast<char*>(command.c_str()),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NEW_CONSOLE,
                nullptr,
                workingDir.c_str(),
                &si,
                &pi
            );

            if (result) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                lua_pushboolean(L, true);
                std::cout << "[GAME] Started: " << name << " (PID: " << pi.dwProcessId << ")\n";
                std::cout.flush();
                return 1;
            } else {
                std::cerr << "[GAME] Failed to start: " << GetLastError() << "\n";
                lua_pushboolean(L, false);
                return 1;
            }
        }
    }

    std::cerr << "[GAME] Game not found: " << name << "\n";
    lua_pushboolean(L, false);
    return 1;
}

} // namespace config

void LuaState::registerConfigModule() {
    lua_newtable(L);

    lua_pushcfunction(L, config::getServer);
    lua_setfield(L, -2, "getServer");

    lua_pushcfunction(L, config::setServer);
    lua_setfield(L, -2, "setServer");

    lua_pushcfunction(L, config::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, config::set);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, config::remove);
    lua_setfield(L, -2, "remove");

    lua_pushcfunction(L, config::getAutoRun);
    lua_setfield(L, -2, "getAutoRun");

    lua_pushcfunction(L, config::setAutoRun);
    lua_setfield(L, -2, "setAutoRun");

    lua_pushcfunction(L, config::getHeartbeat);
    lua_setfield(L, -2, "getHeartbeat");

    lua_pushcfunction(L, config::setHeartbeat);
    lua_setfield(L, -2, "setHeartbeat");

    lua_pushcfunction(L, config::getGames);
    lua_setfield(L, -2, "getGames");

    lua_pushcfunction(L, config::addGame);
    lua_setfield(L, -2, "addGame");

    lua_pushcfunction(L, config::removeGame);
    lua_setfield(L, -2, "removeGame");

    lua_pushcfunction(L, config::startGame);
    lua_setfield(L, -2, "startGame");

    lua_setglobal(L, "config");
}

// ============================================================================
// Node 模块 (节点状态和心跳)
// ============================================================================

namespace node {

// 获取节点 ID
static std::string getNodeId() {
    // 尝试从配置读取，否则生成新的
    if (config::g_configManager) {
        auto nodeId = config::g_configManager->get("nodeId");
        if (nodeId.has_value()) {
            // 去掉 JSON 字符串的引号
            std::string id = nodeId.value();
            if (!id.empty() && id.front() == '"') id.erase(0, 1);
            if (!id.empty() && id.back() == '"') id.pop_back();
            if (!id.empty()) return id;
        }
    }

    // 生成新的节点 ID
    static const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string newId = "node-";
    for (int i = 0; i < 8; i++) {
        newId += charset[rand() % 36];
    }
    if (config::g_configManager) {
        config::g_configManager->set("nodeId", newId);
    }
    return newId;
}

// createHeartbeat() -> table
int createHeartbeat(lua_State* L) {
    NodeHeartbeat heartbeat;
    heartbeat.nodeId = getNodeId();
    heartbeat.status = NodeState::Online;
    heartbeat.timestamp = NodeHeartbeat::now();
    heartbeat.cpuUsage = 0.0;
    heartbeat.memoryUsage = 0.0;
    heartbeat.version = wingman::version::getFullVersion();

    lua_newtable(L);
    lua_pushstring(L, heartbeat.toJson().c_str());
    lua_setfield(L, -2, "json");

    lua_pushstring(L, heartbeat.nodeId.c_str());
    lua_setfield(L, -2, "nodeId");

    lua_pushstring(L, wingman::version::getFullVersion().c_str());
    lua_setfield(L, -2, "version");

    return 1;
}

// sendHeartbeat(table)
int sendHeartbeat(lua_State* L) {
    // 在服务器控制模式下，这会将心跳数据发送到服务器
    // 目前只是打印日志，实际网络通信需要配合 server 模块
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "json");
        if (lua_isstring(L, -1)) {
            const char* json = lua_tostring(L, -1);
            std::cout << "[HEARTBEAT] " << json << "\n";
            std::cout.flush();
        }
        lua_pop(L, 1);
    }
    return 0;
}

// getWindows() -> table
int getWindows(lua_State* L) {
    auto windows = Window::enumerate();

    lua_newtable(L);
    for (size_t i = 0; i < windows.size(); i++) {
        lua_newtable(L);

        lua_pushstring(L, windows[i].title.c_str());
        lua_setfield(L, -2, "title");

        // Handle 作为字符串传递
        char handleStr[32];
        snprintf(handleStr, sizeof(handleStr), "%llu", (unsigned long long)windows[i].handle);
        lua_pushstring(L, handleStr);
        lua_setfield(L, -2, "handle");

        lua_pushboolean(L, windows[i].isForeground);
        lua_setfield(L, -2, "isForeground");

        // 窗口位置和大小
        lua_newtable(L);
        lua_pushinteger(L, windows[i].bounds.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, windows[i].bounds.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, windows[i].bounds.width);
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, windows[i].bounds.height);
        lua_setfield(L, -2, "height");
        lua_setfield(L, -2, "bounds");

        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

} // namespace node

// ============================================================================
// Verification 模块 (TOTP, Email)
// ============================================================================

namespace verification {

static VerificationManager* getManager() {
    static VerificationManager manager;
    return &manager;
}

// TOTP 生成
static int totpGenerate(lua_State* L) {
    const char* secret = luaL_checkstring(L, 1);
    int digits = luaL_optinteger(L, 2, 6);
    int period = luaL_optinteger(L, 3, 30);

    TOTPConfig config;
    config.type = TOTPType::Steam;
    config.secret = secret;
    config.digits = digits;
    config.period = period;

    std::string code = getManager()->generateTOTP(config);
    lua_pushstring(L, code.c_str());
    return 1;
}

// Steam Guard 生成
static int steamGuard(lua_State* L) {
    const char* secret = luaL_checkstring(L, 1);
    std::string code = getManager()->generateSteamGuard(secret);
    lua_pushstring(L, code.c_str());
    return 1;
}

// 保存 TOTP 配置
static int saveTOTP(lua_State* L) {
    const char* account = luaL_checkstring(L, 1);
    const char* secret = luaL_checkstring(L, 2);
    int digits = luaL_optinteger(L, 3, 6);
    int period = luaL_optinteger(L, 4, 30);

    TOTPConfig config;
    config.type = TOTPType::Steam;
    config.secret = secret;
    config.digits = digits;
    config.period = period;
    config.account = account;

    bool result = getManager()->saveTOTP(account, config);
    lua_pushboolean(L, result);
    return 1;
}

// 加载 TOTP 配置并生成
static int loadTOTP(lua_State* L) {
    const char* account = luaL_checkstring(L, 1);
    auto config = getManager()->loadTOTP(account);
    if (!config) {
        lua_pushnil(L);
        return 1;
    }
    std::string code = getManager()->generateTOTP(*config);
    lua_pushstring(L, code.c_str());
    return 1;
}

// 列出所有 TOTP 账号
static int listTOTP(lua_State* L) {
    auto accounts = getManager()->listTOTPAccounts();
    lua_newtable(L);
    for (size_t i = 0; i < accounts.size(); ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, accounts[i].c_str());
        lua_settable(L, -3);
    }
    return 1;
}

// 删除 TOTP 配置
static int removeTOTP(lua_State* L) {
    const char* account = luaL_checkstring(L, 1);
    bool result = getManager()->removeTOTP(account);
    lua_pushboolean(L, result);
    return 1;
}

// 获取剩余时间
static int getRemaining(lua_State* L) {
    const char* secret = luaL_checkstring(L, 1);
    int period = luaL_optinteger(L, 2, 30);

    TOTPConfig config;
    config.secret = secret;
    config.period = period;
    config.digits = 6;

    int remaining = getManager()->getRemainingSeconds(config);
    lua_pushinteger(L, remaining);
    return 1;
}

// 验证 TOTP 码
static int verifyTOTP(lua_State* L) {
    const char* secret = luaL_checkstring(L, 1);
    const char* code = luaL_checkstring(L, 2);
    int window = luaL_optinteger(L, 3, 1);

    TOTPConfig config;
    config.secret = secret;
    config.digits = 6;
    config.period = 30;

    bool result = getManager()->verifyTOTP(config, code, window);
    lua_pushboolean(L, result);
    return 1;
}

} // namespace verification

void LuaState::registerVerificationModule() {
    lua_newtable(L);

    // TOTP 生成
    lua_pushcfunction(L, verification::totpGenerate);
    lua_setfield(L, -2, "totp");

    lua_pushcfunction(L, verification::steamGuard);
    lua_setfield(L, -2, "steamGuard");

    // TOTP 配置管理
    lua_pushcfunction(L, verification::saveTOTP);
    lua_setfield(L, -2, "saveTOTP");

    lua_pushcfunction(L, verification::loadTOTP);
    lua_setfield(L, -2, "loadTOTP");

    lua_pushcfunction(L, verification::listTOTP);
    lua_setfield(L, -2, "listTOTP");

    lua_pushcfunction(L, verification::removeTOTP);
    lua_setfield(L, -2, "removeTOTP");

    // 工具函数
    lua_pushcfunction(L, verification::getRemaining);
    lua_setfield(L, -2, "getRemaining");

    lua_pushcfunction(L, verification::verifyTOTP);
    lua_setfield(L, -2, "verify");

    lua_setglobal(L, "verification");
}

// ============================================================================
// QRCode 扫码登录模块
// ============================================================================

namespace qrcode {

static QRLoginManager* getManager() {
    static QRLoginManager manager;
    return &manager;
}

// 获取二维码内容
static int getQRCode(lua_State* L) {
    const char* qrUrl = luaL_checkstring(L, 1);
    const char* statusUrl = luaL_checkstring(L, 2);

    QRLoginConfig config = QRLoginManager::genericConfig(qrUrl, statusUrl);

    auto qrCode = getManager()->getQRCode(config);
    if (qrCode) {
        lua_pushstring(L, qrCode->c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// 获取二维码并保存为图片
static int getQRImage(lua_State* L) {
    const char* qrUrl = luaL_checkstring(L, 1);
    const char* statusUrl = luaL_checkstring(L, 2);
    const char* outputPath = luaL_checkstring(L, 3);

    QRLoginConfig config = QRLoginManager::genericConfig(qrUrl, statusUrl);

    auto result = getManager()->getQRCodeImage(config, outputPath);
    if (result) {
        lua_pushstring(L, result->c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// 执行扫码登录
static int login(lua_State* L) {
    const char* qrUrl = luaL_checkstring(L, 1);
    const char* statusUrl = luaL_checkstring(L, 2);

    QRLoginConfig config = QRLoginManager::genericConfig(qrUrl, statusUrl);

    // 可选参数：轮询间隔
    int pollInterval = luaL_optinteger(L, 3, 2000);
    config.pollInterval = pollInterval;

    QRLoginResult result = getManager()->login(config);

    // 返回结果表
    lua_newtable(L);

    lua_pushstring(L, qrLoginStateToString(result.state).c_str());
    lua_setfield(L, -2, "state");

    lua_pushstring(L, result.message.c_str());
    lua_setfield(L, -2, "message");

    if (!result.token.empty()) {
        lua_pushstring(L, result.token.c_str());
        lua_setfield(L, -2, "token");
    }

    if (!result.sessionId.empty()) {
        lua_pushstring(L, result.sessionId.c_str());
        lua_setfield(L, -2, "sessionId");
    }

    // 添加完整数据（JSON 字符串）
    if (!result.data.empty()) {
        lua_pushstring(L, result.data.dump().c_str());
        lua_setfield(L, -2, "data");
    }

    return 1;
}

// Steam 登录
static int steamLogin(lua_State* L) {
    QRLoginConfig config = QRLoginManager::steamConfig();

    QRLoginResult result = getManager()->login(config);

    lua_newtable(L);

    lua_pushstring(L, qrLoginStateToString(result.state).c_str());
    lua_setfield(L, -2, "state");

    lua_pushstring(L, result.message.c_str());
    lua_setfield(L, -2, "message");

    if (!result.token.empty()) {
        lua_pushstring(L, result.token.c_str());
        lua_setfield(L, -2, "token");
    }

    if (!result.data.empty()) {
        lua_pushstring(L, result.data.dump().c_str());
        lua_setfield(L, -2, "data");
    }

    return 1;
}

// 取消登录
static int cancel(lua_State* L) {
    getManager()->cancel();
    return 0;
}

// 识别二维码
static int detect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);

    auto code = getManager()->detectQRCode(x, y, width, height);
    if (code) {
        lua_pushstring(L, code->c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

} // namespace qrcode

void LuaState::registerQRCodeModule() {
    lua_newtable(L);

    // 获取二维码
    lua_pushcfunction(L, qrcode::getQRCode);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, qrcode::getQRImage);
    lua_setfield(L, -2, "getImage");

    // 登录流程
    lua_pushcfunction(L, qrcode::login);
    lua_setfield(L, -2, "login");

    lua_pushcfunction(L, qrcode::steamLogin);
    lua_setfield(L, -2, "steamLogin");

    // 控制
    lua_pushcfunction(L, qrcode::cancel);
    lua_setfield(L, -2, "cancel");

    // 二维码识别
    lua_pushcfunction(L, qrcode::detect);
    lua_setfield(L, -2, "detect");

    lua_setglobal(L, "qrcode");
}

void LuaState::registerNodeModule() {
    lua_newtable(L);

    lua_pushcfunction(L, node::createHeartbeat);
    lua_setfield(L, -2, "createHeartbeat");

    lua_pushcfunction(L, node::sendHeartbeat);
    lua_setfield(L, -2, "sendHeartbeat");

    lua_pushcfunction(L, node::getWindows);
    lua_setfield(L, -2, "getWindows");

    lua_setglobal(L, "node");
}

// ============================================================================
// UI Automation 模块
// ============================================================================

namespace uia {

// UI Element 元数据（存储在 Lua userdata 中）
struct UIElementData {
    std::shared_ptr<wingman::UIAutomationElement> element;
};

// 检查参数是否是 UIElement
UIElementData* checkUIElement(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, "UIElement");
    return static_cast<UIElementData*>(ud);
}

// 创建 UIElement userdata 并压入栈
void pushUIElement(lua_State* L, std::shared_ptr<wingman::UIAutomationElement> element) {
    auto* ud = static_cast<UIElementData*>(lua_newuserdata(L, sizeof(UIElementData)));
    new (ud) UIElementData();
    ud->element = element;
    luaL_setmetatable(L, "UIElement");
}

// UIElement:click()
int click(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->click();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// UIElement:rightClick()
int rightClick(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->rightClick();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// UIElement:doubleClick()
int doubleClick(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->doubleClick();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// UIElement:focus()
int focus(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->focus();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// UIElement:getValue()
int getValue(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        std::string value = data->element->getValue();
        lua_pushstring(L, value.c_str());
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

// UIElement:setValue(value)
int setValue(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    const char* value = luaL_checkstring(L, 2);
    if (data && data->element && value) {
        bool result = data->element->setValue(value);
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// UIElement:getName()
int getName(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        std::string name = data->element->getName();
        lua_pushstring(L, name.c_str());
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

// UIElement:getInfo()
int getInfo(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    lua_newtable(L);
    if (data && data->element) {
        UIElementInfo info = data->element->getInfo();
        lua_pushstring(L, info.name.c_str());
        lua_setfield(L, -2, "name");
        lua_pushstring(L, info.className.c_str());
        lua_setfield(L, -2, "className");
        lua_pushstring(L, info.automationId.c_str());
        lua_setfield(L, -2, "automationId");
        lua_pushstring(L, info.controlType.c_str());
        lua_setfield(L, -2, "controlType");
        lua_pushboolean(L, info.isEnabled);
        lua_setfield(L, -2, "isEnabled");
        lua_pushboolean(L, info.isVisible);
        lua_setfield(L, -2, "isVisible");

        // bounds
        lua_newtable(L);
        lua_pushinteger(L, info.bounds.x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, info.bounds.y);
        lua_setfield(L, -2, "y");
        lua_pushinteger(L, info.bounds.width);
        lua_setfield(L, -2, "width");
        lua_pushinteger(L, info.bounds.height);
        lua_setfield(L, -2, "height");
        lua_setfield(L, -2, "bounds");
    }
    return 1;
}

// UIElement:getChildren()
int getChildren(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    lua_newtable(L);
    if (data && data->element) {
        auto children = data->element->getChildren();
        for (size_t i = 0; i < children.size(); i++) {
            pushUIElement(L, children[i]);
            lua_rawseti(L, -2, i + 1);
        }
    }
    return 1;
}

// UIElement 元方法
int uiElementGC(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data) {
        data->element.~shared_ptr();
    }
    return 0;
}

int uiElementToString(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        std::string name = data->element->getName();
        lua_pushfstring(L, "UIElement(%s)", name.c_str());
    } else {
        lua_pushstring(L, "UIElement(invalid)");
    }
    return 1;
}

// UIElement 索引方法
int uiElementIndex(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    auto* data = checkUIElement(L, 1);

    if (strcmp(key, "click") == 0) {
        lua_pushcfunction(L, click);
        return 1;
    } else if (strcmp(key, "rightClick") == 0) {
        lua_pushcfunction(L, rightClick);
        return 1;
    } else if (strcmp(key, "doubleClick") == 0) {
        lua_pushcfunction(L, doubleClick);
        return 1;
    } else if (strcmp(key, "focus") == 0) {
        lua_pushcfunction(L, focus);
        return 1;
    } else if (strcmp(key, "getValue") == 0) {
        lua_pushcfunction(L, getValue);
        return 1;
    } else if (strcmp(key, "setValue") == 0) {
        lua_pushcfunction(L, setValue);
        return 1;
    } else if (strcmp(key, "getName") == 0) {
        lua_pushcfunction(L, getName);
        return 1;
    } else if (strcmp(key, "getInfo") == 0) {
        lua_pushcfunction(L, getInfo);
        return 1;
    } else if (strcmp(key, "getChildren") == 0) {
        lua_pushcfunction(L, getChildren);
        return 1;
    } else if (strcmp(key, "getParent") == 0) {
        lua_pushcfunction(L, getParent);
        return 1;
    } else if (strcmp(key, "expand") == 0) {
        lua_pushcfunction(L, expand);
        return 1;
    } else if (strcmp(key, "collapse") == 0) {
        lua_pushcfunction(L, collapse);
        return 1;
    } else if (strcmp(key, "isExpanded") == 0) {
        lua_pushcfunction(L, isExpanded);
        return 1;
    } else if (strcmp(key, "selectItem") == 0) {
        lua_pushcfunction(L, selectItem);
        return 1;
    } else if (strcmp(key, "getSelection") == 0) {
        lua_pushcfunction(L, getSelection);
        return 1;
    }

    // 返回属性
    if (data && data->element) {
        if (strcmp(key, "name") == 0) {
            std::string name = data->element->getName();
            lua_pushstring(L, name.c_str());
            return 1;
        } else if (strcmp(key, "className") == 0) {
            std::string className = data->element->getClassName();
            lua_pushstring(L, className.c_str());
            return 1;
        } else if (strcmp(key, "automationId") == 0) {
            std::string automationId = data->element->getAutomationId();
            lua_pushstring(L, automationId.c_str());
            return 1;
        }
    }

    return 0;
}

// ========== 全局函数 ==========

// uia.fromPoint(x, y)
int fromPoint(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);

    auto element = wingman::uia().fromPoint(x, y);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.fromWindow(hwnd)
int fromWindow(lua_State* L) {
    HWND hwnd = reinterpret_cast<HWND>(luaL_checkinteger(L, 1));

    auto element = wingman::uia().fromWindow(hwnd);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.fromForeground()
int fromForeground(lua_State* L) {
    auto element = wingman::uia().fromForegroundWindow();
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.findButton(name)
int findButton(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto element = wingman::uia().findButton(name);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.findEdit(name)
int findEdit(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto element = wingman::uia().findEdit(name);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.findText(name)
int findText(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto element = wingman::uia().findText(name);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.findByName(name)
int findByName(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto element = wingman::uia().findByName(name);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.findById(id)
int findById(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);

    auto element = wingman::uia().findById(id);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// uia.waitForName(name, timeout)
int waitForName(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int timeout = luaL_optinteger(L, 2, 5000);

    auto element = wingman::uia().waitForName(name, timeout);
    if (element) {
        pushUIElement(L, element);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// 查找所有匹配元素（简化版，返回第一个）
int findAll(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "");

    wingman::UIACondition condition;
    if (name && name[0]) {
        condition.withName(name);
    }

    auto elements = wingman::uia().findAll(condition);

    // 返回第一个匹配元素（简化）
    if (!elements.empty()) {
        pushUIElement(L, elements[0]);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// 便捷查找方法
int findCheckBox(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findCheckBox(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findRadioButton(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findRadioButton(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findComboBox(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findComboBox(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findList(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findList(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findListItem(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findListItem(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findTab(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findTab(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findTabItem(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findTabItem(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findTree(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findTree(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findTreeItem(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findTreeItem(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findMenuItem(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findMenuItem(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findHyperlink(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findHyperlink(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findImage(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findImage(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findSlider(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findSlider(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findSpinner(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findSpinner(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

int findProgressBar(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto element = wingman::uia().findProgressBar(name);
    if (element) pushUIElement(L, element);
    else lua_pushnil(L);
    return 1;
}

// 遍历所有子元素
int findChildren(lua_State* L) {
    // 从栈中获取 UIElement
    auto* data = checkUIElement(L, 1);
    if (!data || !data->element) {
        lua_pushnil(L);
        return 1;
    }

    auto children = data->element->getChildren();
    lua_newtable(L);

    for (size_t i = 0; i < children.size(); i++) {
        pushUIElement(L, children[i]);
        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

// 获取父元素
int getParent(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (!data || !data->element) {
        lua_pushnil(L);
        return 1;
    }

    auto parent = data->element->getParent();
    if (parent) {
        pushUIElement(L, parent);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// 展开元素
int expand(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->expand();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// 折叠元素
int collapse(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        bool result = data->element->collapse();
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// 检查是否展开
int isExpanded(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        lua_pushboolean(L, data->element->isExpanded());
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// 选择项目
int selectItem(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    const char* item = luaL_checkstring(L, 2);

    if (data && data->element && item) {
        bool result = data->element->selectItem(item);
        lua_pushboolean(L, result);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

// 获取选择项
int getSelection(lua_State* L) {
    auto* data = checkUIElement(L, 1);
    if (data && data->element) {
        std::string selection = data->element->getSelection();
        lua_pushstring(L, selection.c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// ============================================================================
// UIA 事件监听器支持
// ============================================================================

// 事件监听器数据结构
struct UIAEventListener {
    lua_State* L;
    int callbackRef;
    int listenerId;
    std::string propertyName;
    std::string elementName;
    bool isActive;

    UIAEventListener() : L(nullptr), callbackRef(LUA_NOREF), listenerId(0), isActive(false) {}

    ~UIAEventListener() {
        if (callbackRef != LUA_NOREF && L) {
            luaL_unref(L, LUA_REGISTRYINDEX, callbackRef);
        }
    }
};

// 全局监听器注册表
static std::unordered_map<int, std::shared_ptr<UIAEventListener>> g_uiEventListeners;
static std::atomic<int> g_nextListenerId{1};

// 调用 Lua 回调
static void invokeLuaCallback(lua_State* L, int callbackRef, const std::string& propertyName, const std::string& value) {
    if (callbackRef == LUA_NOREF || !L) return;

    // 获取 Lua 函数
    lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);

    // 推送参数
    lua_pushstring(L, propertyName.c_str());
    lua_pushstring(L, value.c_str());

    // 调用函数
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        const char* errorMsg = lua_tostring(L, -1);
        if (errorMsg) {
            spdlog::error("[UIA] Lua callback error: {}", errorMsg);
        }
        lua_pop(L, 1);
    }
}

// 注册属性变更事件
// 用法: uia.onPropertyChanged(name, callback)
// 示例: uia.onPropertyChanged("按钮1", function(prop, value) print(prop, value) end)
int onPropertyChanged(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        luaL_error(L, "Expected function as second argument");
        return 0;
    }

    // 保存回调函数引用
    lua_pushvalue(L, 2);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // 创建监听器
    auto listener = std::make_shared<UIAEventListener>();
    listener->L = L;
    listener->callbackRef = callbackRef;
    listener->listenerId = g_nextListenerId++;
    listener->elementName = name;
    listener->isActive = true;

    // 注册 C++ 层事件
    wingman::UIACondition condition;
    condition.withName(name);

    bool registered = wingman::uia().registerPropertyChangedHandler(
        condition,
        [listener](const std::string& propertyName, const std::string& value) {
            if (listener && listener->isActive) {
                invokeLuaCallback(listener->L, listener->callbackRef, propertyName, value);
            }
        }
    );

    if (registered) {
        g_uiEventListeners[listener->listenerId] = listener;
        lua_pushinteger(L, listener->listenerId);
        spdlog::info("[UIA] Property change listener registered for '{}' (id: {})", name, listener->listenerId);
    } else {
        lua_pushnil(L);
        spdlog::warn("[UIA] Failed to register property change listener for '{}'", name);
    }

    return 1;
}

// 注册结构变更事件
// 用法: uia.onStructureChanged(name, callback)
int onStructureChanged(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    if (!lua_isfunction(L, 2)) {
        luaL_error(L, "Expected function as second argument");
        return 0;
    }

    // 保存回调函数引用
    lua_pushvalue(L, 2);
    int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    // 创建监听器
    auto listener = std::make_shared<UIAEventListener>();
    listener->L = L;
    listener->callbackRef = callbackRef;
    listener->listenerId = g_nextListenerId++;
    listener->elementName = name;
    listener->isActive = true;

    // 注册 C++ 层事件
    wingman::UIACondition condition;
    condition.withName(name);

    bool registered = wingman::uia().registerStructureChangedHandler(
        condition,
        [listener]() {
            if (listener && listener->isActive) {
                invokeLuaCallback(listener->L, listener->callbackRef, "StructureChanged", "");
            }
        }
    );

    if (registered) {
        g_uiEventListeners[listener->listenerId] = listener;
        lua_pushinteger(L, listener->listenerId);
        spdlog::info("[UIA] Structure change listener registered for '{}' (id: {})", name, listener->listenerId);
    } else {
        lua_pushnil(L);
        spdlog::warn("[UIA] Failed to register structure change listener for '{}'", name);
    }

    return 1;
}

// 移除事件监听器
// 用法: uia.removeEventListener(listenerId)
int removeEventListener(lua_State* L) {
    int listenerId = luaL_checkinteger(L, 1);

    auto it = g_uiEventListeners.find(listenerId);
    if (it != g_uiEventListeners.end()) {
        it->second->isActive = false;
        g_uiEventListeners.erase(it);
        lua_pushboolean(L, true);
        spdlog::info("[UIA] Event listener removed (id: {})", listenerId);
    } else {
        lua_pushboolean(L, false);
        spdlog::warn("[UIA] Event listener not found (id: {})", listenerId);
    }

    return 1;
}

// 清理所有监听器（在 Lua 状态关闭时调用）
int cleanupAllListeners(lua_State* L) {
    for (auto& pair : g_uiEventListeners) {
        pair.second->isActive = false;
    }
    g_uiEventListeners.clear();
    return 0;
}

} // namespace uia

void LuaState::registerUIAutomationModule() {
    // 创建 UIElement 元表
    luaL_newmetatable(L, "UIElement");

    // 元方法
    lua_pushcfunction(L, uia::uiElementGC);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, uia::uiElementToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, uia::uiElementIndex);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    // 创建 uia 全局表
    lua_newtable(L);

    lua_pushcfunction(L, uia::fromPoint);
    lua_setfield(L, -2, "fromPoint");

    lua_pushcfunction(L, uia::fromWindow);
    lua_setfield(L, -2, "fromWindow");

    lua_pushcfunction(L, uia::fromForeground);
    lua_setfield(L, -2, "fromForeground");

    lua_pushcfunction(L, uia::findButton);
    lua_setfield(L, -2, "findButton");

    lua_pushcfunction(L, uia::findEdit);
    lua_setfield(L, -2, "findEdit");

    lua_pushcfunction(L, uia::findText);
    lua_setfield(L, -2, "findText");

    lua_pushcfunction(L, uia::findCheckBox);
    lua_setfield(L, -2, "findCheckBox");

    lua_pushcfunction(L, uia::findRadioButton);
    lua_setfield(L, -2, "findRadioButton");

    lua_pushcfunction(L, uia::findComboBox);
    lua_setfield(L, -2, "findComboBox");

    lua_pushcfunction(L, uia::findList);
    lua_setfield(L, -2, "findList");

    lua_pushcfunction(L, uia::findListItem);
    lua_setfield(L, -2, "findListItem");

    lua_pushcfunction(L, uia::findTab);
    lua_setfield(L, -2, "findTab");

    lua_pushcfunction(L, uia::findTabItem);
    lua_setfield(L, -2, "findTabItem");

    lua_pushcfunction(L, uia::findTree);
    lua_setfield(L, -2, "findTree");

    lua_pushcfunction(L, uia::findTreeItem);
    lua_setfield(L, -2, "findTreeItem");

    lua_pushcfunction(L, uia::findMenuItem);
    lua_setfield(L, -2, "findMenuItem");

    lua_pushcfunction(L, uia::findHyperlink);
    lua_setfield(L, -2, "findHyperlink");

    lua_pushcfunction(L, uia::findImage);
    lua_setfield(L, -2, "findImage");

    lua_pushcfunction(L, uia::findSlider);
    lua_setfield(L, -2, "findSlider");

    lua_pushcfunction(L, uia::findSpinner);
    lua_setfield(L, -2, "findSpinner");

    lua_pushcfunction(L, uia::findProgressBar);
    lua_setfield(L, -2, "findProgressBar");

    lua_pushcfunction(L, uia::findByName);
    lua_setfield(L, -2, "findByName");

    lua_pushcfunction(L, uia::findById);
    lua_setfield(L, -2, "findById");

    lua_pushcfunction(L, uia::waitForName);
    lua_setfield(L, -2, "waitForName");

    lua_pushcfunction(L, uia::findAll);
    lua_setfield(L, -2, "findAll");

    lua_pushcfunction(L, uia::onPropertyChanged);
    lua_setfield(L, -2, "onPropertyChanged");

    lua_pushcfunction(L, uia::onStructureChanged);
    lua_setfield(L, -2, "onStructureChanged");

    lua_pushcfunction(L, uia::removeEventListener);
    lua_setfield(L, -2, "removeEventListener");

    lua_setglobal(L, "uia");
}

// ============================================================================
// Human 模块 (人性化操作)
// ============================================================================

namespace human {

// ========== human.mouse 函数 ==========

int mouseMove(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    Human::mouse().moveTo(x, y);
    return 0;
}

int mouseMoveWithDuration(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int duration = luaL_optinteger(L, 3, 200);
    Human::mouse().moveTo(x, y, duration);
    return 0;
}

int mouseClick(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    Human::mouse().click(x, y);
    return 0;
}

int mouseRightClick(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    Human::mouse().rightClick(x, y);
    return 0;
}

int mouseDoubleClick(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    Human::mouse().doubleClick(x, y);
    return 0;
}

int mouseDrag(lua_State* L) {
    int fromX = luaL_checkinteger(L, 1);
    int fromY = luaL_checkinteger(L, 2);
    int toX = luaL_checkinteger(L, 3);
    int toY = luaL_checkinteger(L, 4);
    Human::mouse().drag(fromX, fromY, toX, toY);
    return 0;
}

int mouseScroll(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int delta = luaL_optinteger(L, 3, 120);
    Human::mouse().scroll(x, y, delta);
    return 0;
}

int mouseGetPosition(lua_State* L) {
    Point pos = Human::mouse().getCurrentPosition();
    pushPoint(L, pos);
    return 1;
}

// ========== human.mouse 配置函数 ==========

int mouseSetConfig(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "config must be a table");
    }

    HumanMouseConfig config;

    // 读取配置参数
    lua_getfield(L, 1, "minMoveDuration");
    if (lua_isinteger(L, -1)) config.minMoveDuration = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "maxMoveDuration");
    if (lua_isinteger(L, -1)) config.maxMoveDuration = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "moveVariance");
    if (lua_isinteger(L, -1)) config.moveVariance = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "pathVariance");
    if (lua_isinteger(L, -1)) config.pathVariance = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "clickDelayMin");
    if (lua_isinteger(L, -1)) config.clickDelayMin = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "clickDelayMax");
    if (lua_isinteger(L, -1)) config.clickDelayMax = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enableRandomDelay");
    if (lua_isboolean(L, -1)) config.enableRandomDelay = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enablePathRandomness");
    if (lua_isboolean(L, -1)) config.enablePathRandomness = lua_toboolean(L, -1);
    lua_pop(L, 1);

    Human::mouse().setConfig(config);
    return 0;
}

int mouseGetConfig(lua_State* L) {
    HumanMouseConfig config = Human::mouse().getConfig();

    lua_newtable(L);
    lua_pushinteger(L, config.minMoveDuration);
    lua_setfield(L, -2, "minMoveDuration");
    lua_pushinteger(L, config.maxMoveDuration);
    lua_setfield(L, -2, "maxMoveDuration");
    lua_pushinteger(L, config.moveVariance);
    lua_setfield(L, -2, "moveVariance");
    lua_pushinteger(L, config.pathVariance);
    lua_setfield(L, -2, "pathVariance");
    lua_pushinteger(L, config.clickDelayMin);
    lua_setfield(L, -2, "clickDelayMin");
    lua_pushinteger(L, config.clickDelayMax);
    lua_setfield(L, -2, "clickDelayMax");
    lua_pushboolean(L, config.enableRandomDelay);
    lua_setfield(L, -2, "enableRandomDelay");
    lua_pushboolean(L, config.enablePathRandomness);
    lua_setfield(L, -2, "enablePathRandomness");

    return 1;
}

// ========== human.keyboard 函数 ==========

int keyPress(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Human::keyboard().key(vkCode);
    return 0;
}

int keyDown(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Human::keyboard().keyDown(vkCode);
    return 0;
}

int keyUp(lua_State* L) {
    int vkCode = luaL_checkinteger(L, 1);
    Human::keyboard().keyUp(vkCode);
    return 0;
}

int typeText(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    bool randomCase = lua_toboolean(L, 2);
    Human::keyboard().type(text, randomCase);
    return 0;
}

// ========== human.keyboard 配置函数 ==========

int keyboardSetConfig(lua_State* L) {
    if (!lua_istable(L, 1)) {
        return luaL_error(L, "config must be a table");
    }

    HumanKeyboardConfig config;

    lua_getfield(L, 1, "keyDownDelayMin");
    if (lua_isinteger(L, -1)) config.keyDownDelayMin = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "keyDownDelayMax");
    if (lua_isinteger(L, -1)) config.keyDownDelayMax = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "typeDelayMin");
    if (lua_isinteger(L, -1)) config.typeDelayMin = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "typeDelayMax");
    if (lua_isinteger(L, -1)) config.typeDelayMax = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "enableRandomDelay");
    if (lua_isboolean(L, -1)) config.enableRandomDelay = lua_toboolean(L, -1);
    lua_pop(L, 1);

    Human::keyboard().setConfig(config);
    return 0;
}

} // namespace human

void LuaState::registerHumanModule() {
    // 创建 human 全局表
    lua_newtable(L);

    // ---------- mouse 子表 ----------
    lua_newtable(L);

    lua_pushcfunction(L, human::mouseMove);
    lua_setfield(L, -2, "move");

    lua_pushcfunction(L, human::mouseMoveWithDuration);
    lua_setfield(L, -2, "moveTo");

    lua_pushcfunction(L, human::mouseClick);
    lua_setfield(L, -2, "click");

    lua_pushcfunction(L, human::mouseRightClick);
    lua_setfield(L, -2, "rightClick");

    lua_pushcfunction(L, human::mouseDoubleClick);
    lua_setfield(L, -2, "doubleClick");

    lua_pushcfunction(L, human::mouseDrag);
    lua_setfield(L, -2, "drag");

    lua_pushcfunction(L, human::mouseScroll);
    lua_setfield(L, -2, "scroll");

    lua_pushcfunction(L, human::mouseGetPosition);
    lua_setfield(L, -2, "getPosition");

    lua_pushcfunction(L, human::mouseSetConfig);
    lua_setfield(L, -2, "setConfig");

    lua_pushcfunction(L, human::mouseGetConfig);
    lua_setfield(L, -2, "getConfig");

    lua_setfield(L, -2, "mouse");

    // ---------- keyboard 子表 ----------
    lua_newtable(L);

    lua_pushcfunction(L, human::keyPress);
    lua_setfield(L, -2, "press");

    lua_pushcfunction(L, human::keyDown);
    lua_setfield(L, -2, "down");

    lua_pushcfunction(L, human::keyUp);
    lua_setfield(L, -2, "up");

    lua_pushcfunction(L, human::typeText);
    lua_setfield(L, -2, "type");

    lua_pushcfunction(L, human::keyboardSetConfig);
    lua_setfield(L, -2, "setConfig");

    lua_setfield(L, -2, "keyboard");

    // 设置为全局表
    lua_setglobal(L, "human");
}

// ============================================================================
// debugger 模块 - Lua 调试器支持
// ============================================================================

namespace debugger {
    // debugger.start() - 启动调试器
    static int debuggerStart(lua_State* L) {
        bool success = wingman::startDebugger(L);
        lua_pushboolean(L, success);
        return 1;
    }

    // debugger.stop() - 停止调试器
    static int debuggerStop(lua_State* L) {
        wingman::stopDebugger();
        return 0;
    }

    // debugger.breakpoint(file, line) - 设置断点
    static int debuggerSetBreakpoint(lua_State* L) {
        const char* file = luaL_checkstring(L, 1);
        int line = luaL_checkinteger(L, 2);

        auto& bpManager = wingman::getDebugger().breakpoints();
        size_t id = bpManager.addBreakpoint(file, line);

        lua_pushinteger(L, id);
        return 1;
    }

    // debugger.breakHere() - 在当前位置中断（断点）
    static int debuggerBreakHere(lua_State* L) {
        // 在 Lua 中设置一个断点标记
        lua_pushstring(L, "DEBUG_BREAK_HERE");
        return 1;
    }
}

void LuaState::registerDebuggerModule() {
    lua_newtable(L);

    lua_pushcfunction(L, debugger::debuggerStart);
    lua_setfield(L, -2, "start");

    lua_pushcfunction(L, debugger::debuggerStop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, debugger::debuggerSetBreakpoint);
    lua_setfield(L, -2, "breakpoint");

    lua_pushcfunction(L, debugger::debuggerBreakHere);
    lua_setfield(L, -2, "breakHere");

    lua_setglobal(L, "debugger");
}

} // namespace wingman::lua
