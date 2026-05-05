#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

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
#include "wingman/tray.hpp"
#include "wingman/config.hpp"

#include <cstring>
#include <ctime>
#include <iostream>

namespace wingman::lua {

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
    registerTrayModule();
    registerConfigModule();

    // 注册扩展模块
    registerHttpModule(L);
    registerJsonModule(L);
    registerKvModule(L);
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
    lua_newtable(L);

    lua_pushcfunction(L, script::load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, script::unload);
    lua_setfield(L, -2, "unload");

    lua_pushcfunction(L, script::reload);
    lua_setfield(L, -2, "reload");

    lua_pushcfunction(L, script::run);
    lua_setfield(L, -2, "run");

    lua_pushcfunction(L, script::stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, script::pause);
    lua_setfield(L, -2, "pause");

    lua_pushcfunction(L, script::resume);
    lua_setfield(L, -2, "resume");

    lua_pushcfunction(L, script::list);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, script::isRunning);
    lua_setfield(L, -2, "isRunning");

    lua_pushcfunction(L, script::getState);
    lua_setfield(L, -2, "getState");

    lua_pushcfunction(L, script::setConfig);
    lua_setfield(L, -2, "setConfig");

    lua_pushcfunction(L, script::getConfig);
    lua_setfield(L, -2, "getConfig");

    lua_pushcfunction(L, script::setEnv);
    lua_setfield(L, -2, "setEnv");

    lua_pushcfunction(L, script::getEnv);
    lua_setfield(L, -2, "getEnv");

    lua_pushcfunction(L, script::setHotReload);
    lua_setfield(L, -2, "setHotReload");

    lua_pushcfunction(L, script::checkReload);
    lua_setfield(L, -2, "checkReload");

    lua_setglobal(L, "script");
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
// Tray 模块
// ============================================================================

// TrayIcon userdata 的元方法名称
#define TRAY_ICON_METATABLE "Wingman.TrayIcon"

// 从栈获取 TrayIcon 指针
static wingman::TrayIcon* checkTrayIcon(lua_State* L, int index) {
    void* ud = luaL_checkudata(L, index, TRAY_ICON_METATABLE);
    return *static_cast<wingman::TrayIcon**>(ud);
}

// 创建新的 TrayIcon
static wingman::TrayIcon* pushTrayIcon(lua_State* L, const std::string& tooltip) {
    auto icon = new wingman::TrayIcon(tooltip);
    *static_cast<wingman::TrayIcon**>(lua_newuserdata(L, sizeof(wingman::TrayIcon*))) = icon;

    // 设置元表
    luaL_getmetatable(L, TRAY_ICON_METATABLE);
    lua_setmetatable(L, -2);

    return icon;
}

namespace tray {

int create(lua_State* L) {
    const char* tooltip = luaL_checkstring(L, 1);
    pushTrayIcon(L, tooltip);
    return 1;
}

int get(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto icon = wingman::TrayManager::instance().getIcon(id);
    if (icon) {
        // 重新包装已存在的 icon
        *static_cast<wingman::TrayIcon**>(lua_newuserdata(L, sizeof(wingman::TrayIcon*))) = icon.get();
        luaL_getmetatable(L, TRAY_ICON_METATABLE);
        lua_setmetatable(L, -2);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int remove(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    wingman::TrayManager::instance().removeIcon(id);
    return 0;
}

} // namespace tray

namespace tray_icon {

int setIcon(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* iconPath = luaL_checkstring(L, 2);
    icon->setIcon(iconPath);
    return 0;
}

int setTooltip(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* tooltip = luaL_checkstring(L, 2);
    icon->setTooltip(tooltip);
    return 0;
}

int addItem(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* id = luaL_checkstring(L, 2);
    const char* label = luaL_checkstring(L, 3);

    // 检查是否有回调函数
    if (lua_isfunction(L, 4)) {
        // 将函数保存到注册表中
        int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

        wingman::TrayItem item;
        item.id = id;
        item.label = label;
        item.type = wingman::TrayItemType::NORMAL;
        item.callback = [L, callbackRef]() {
            // 从注册表中获取函数并调用
            lua_rawgeti(L, LUA_REGISTRYINDEX, callbackRef);
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                // 错误处理
                lua_pop(L, 1);
            }
        };

        icon->addItem(item);
    } else {
        wingman::TrayItem item;
        item.id = id;
        item.label = label;
        item.type = wingman::TrayItemType::NORMAL;
        icon->addItem(item);
    }

    return 0;
}

int addSeparator(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* id = luaL_checkstring(L, 2);
    icon->addSeparator(id);
    return 0;
}

int addSubmenu(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* id = luaL_checkstring(L, 2);
    const char* label = luaL_checkstring(L, 3);

    std::vector<wingman::TrayItem> subitems;

    // 遍历表格，获取子菜单项
    luaL_checktype(L, 4, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 4) != 0) {
        if (lua_istable(L, -1)) {
            wingman::TrayItem item;

            lua_getfield(L, -1, "id");
            if (lua_isstring(L, -1)) {
                item.id = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "label");
            if (lua_isstring(L, -1)) {
                item.label = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            item.type = wingman::TrayItemType::NORMAL;
            subitems.push_back(item);
        }
        lua_pop(L, 1);
    }

    icon->addSubmenu(id, label, subitems);
    return 0;
}

int removeItem(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    const char* id = luaL_checkstring(L, 2);
    icon->removeItem(id);
    return 0;
}

int clearItems(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    icon->clearItems();
    return 0;
}

int show(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    icon->show();
    return 0;
}

int hide(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    icon->hide();
    return 0;
}

int updateMenu(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    icon->updateMenu();
    return 0;
}

int isVisible(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    lua_pushboolean(L, icon->isVisible());
    return 1;
}

int destroy(lua_State* L) {
    auto* icon = checkTrayIcon(L, 1);
    delete icon;
    return 0;
}

} // namespace tray_icon

void LuaState::registerTrayModule() {
    // 创建 TrayIcon 元表
    if (luaL_newmetatable(L, TRAY_ICON_METATABLE)) {
        // 设置 __index
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        // 注册对象方法
        lua_pushcfunction(L, tray_icon::setIcon);
        lua_setfield(L, -2, "setIcon");

        lua_pushcfunction(L, tray_icon::setTooltip);
        lua_setfield(L, -2, "setTooltip");

        lua_pushcfunction(L, tray_icon::addItem);
        lua_setfield(L, -2, "addItem");

        lua_pushcfunction(L, tray_icon::addSeparator);
        lua_setfield(L, -2, "addSeparator");

        lua_pushcfunction(L, tray_icon::addSubmenu);
        lua_setfield(L, -2, "addSubmenu");

        lua_pushcfunction(L, tray_icon::removeItem);
        lua_setfield(L, -2, "removeItem");

        lua_pushcfunction(L, tray_icon::clearItems);
        lua_setfield(L, -2, "clearItems");

        lua_pushcfunction(L, tray_icon::show);
        lua_setfield(L, -2, "show");

        lua_pushcfunction(L, tray_icon::hide);
        lua_setfield(L, -2, "hide");

        lua_pushcfunction(L, tray_icon::updateMenu);
        lua_setfield(L, -2, "updateMenu");

        lua_pushcfunction(L, tray_icon::isVisible);
        lua_setfield(L, -2, "isVisible");

        lua_pushcfunction(L, tray_icon::destroy);
        lua_setfield(L, -2, "destroy");
    }
    lua_pop(L, 1);

    // 注册 tray 全局模块
    lua_newtable(L);

    lua_pushcfunction(L, tray::create);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, tray::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, tray::remove);
    lua_setfield(L, -2, "remove");

    lua_setglobal(L, "tray");
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

    config->setServerConfig(serverConfig);
    return 0;
}

// getTray() -> table
int getTray(lua_State* L) {
    auto* config = getConfigManager();
    TrayConfig trayConfig = config->getTrayConfig();

    lua_newtable(L);
    lua_pushboolean(L, trayConfig.minimizeToTray);
    lua_setfield(L, -2, "minimizeToTray");

    lua_pushboolean(L, trayConfig.startMinimized);
    lua_setfield(L, -2, "startMinimized");

    lua_pushboolean(L, trayConfig.showNotifications);
    lua_setfield(L, -2, "showNotifications");

    return 1;
}

// setTray(table)
int setTray(lua_State* L) {
    auto* config = getConfigManager();

    if (!lua_istable(L, 1)) {
        lua_pushstring(L, "setTray: 期望 table 参数");
        lua_error(L);
        return 1;
    }

    TrayConfig trayConfig;

    lua_getfield(L, 1, "minimizeToTray");
    if (lua_isboolean(L, -1)) {
        trayConfig.minimizeToTray = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "startMinimized");
    if (lua_isboolean(L, -1)) {
        trayConfig.startMinimized = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "showNotifications");
    if (lua_isboolean(L, -1)) {
        trayConfig.showNotifications = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    config->setTrayConfig(trayConfig);
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

} // namespace config

void LuaState::registerConfigModule() {
    lua_newtable(L);

    lua_pushcfunction(L, config::getServer);
    lua_setfield(L, -2, "getServer");

    lua_pushcfunction(L, config::setServer);
    lua_setfield(L, -2, "setServer");

    lua_pushcfunction(L, config::getTray);
    lua_setfield(L, -2, "getTray");

    lua_pushcfunction(L, config::setTray);
    lua_setfield(L, -2, "setTray");

    lua_pushcfunction(L, config::get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, config::set);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, config::remove);
    lua_setfield(L, -2, "remove");

    lua_setglobal(L, "config");
}

} // namespace wingman::lua
