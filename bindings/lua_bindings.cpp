#include "lua_bindings.hpp"
#include "lua_extensions.hpp"

#include "wingman/screen.hpp"
#include "wingman/input.hpp"
#include "wingman/window.hpp"
#include "wingman/process.hpp"

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
        std::cerr << "Lua 错误: " << msg << "\n";
    }
    lua_pop(L, 1);
}

void LuaState::registerAPIs() {
    registerScreenModule();
    registerInputModule();
    registerWindowModule();
    registerProcessModule();
    registerUtilModule();

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

} // namespace wingman::lua
