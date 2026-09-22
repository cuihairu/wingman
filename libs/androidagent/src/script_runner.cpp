#include "wingman/androidagent/script_runner.hpp"

#include "wingman/androidagent/android_script_api.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <chrono>
#include <sstream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace wingman::android {

namespace {

// 停止标志指针经 LUA_REGISTRYINDEX 传递给指令 hook（light userdata）。
// 取地址保证键唯一；runSync 存活期内指针有效。
char kStopFlagKey = 0;

// 指令计数 hook：脚本线程内执行（lua_State 单线程约束下唯一安全的
// 中断方式），在停止标志置位后强制 luaL_error 打断执行。
void stopHookThunk(lua_State* L, lua_Debug* /*ar*/) {
    lua_rawgetp(L, LUA_REGISTRYINDEX, &kStopFlagKey);
    auto* stopFlag = static_cast<std::atomic<bool>*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) {
        luaL_error(L, "script stopped");
    }
}

constexpr int kHookInstructionCount = 100000;  // 死循环兜底的检查粒度

// print/log 实现：逐参数走 Lua tostring（number/table/__tostring 全兼容），
// 以标准 print 语义用 tab 连接。停止标志置位时同样中断（print 循环场景）。
void luaPrint(sol::state& lua, std::atomic<bool>& stopFlag,
              const ScriptRunner::OutputCallback& output, sol::variadic_args va) {
    std::ostringstream oss;
    bool first = true;
    for (auto item : va) {
        if (!first) {
            oss << '\t';
        }
        first = false;
        oss << lua["tostring"](item).get<std::string>();
    }
    output(oss.str());
    if (stopFlag.load(std::memory_order_relaxed)) {
        throw sol::error("script stopped");
    }
}

// wingman.sleep：分片睡眠（每片 50ms），置停止即抛错中断
void wingmanSleep(std::atomic<bool>& stopFlag, double milliseconds) {
    if (milliseconds < 0) {
        milliseconds = 0;
    }
    constexpr double kSliceMs = 50.0;
    double waited = 0;
    while (waited < milliseconds && !stopFlag.load(std::memory_order_relaxed)) {
        const double step = std::min(kSliceMs, milliseconds - waited);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<long long>(step)));
        waited += step;
    }
    if (stopFlag.load(std::memory_order_relaxed)) {
        throw sol::error("script stopped");
    }
}

} // namespace

ScriptRunner::~ScriptRunner() {
    stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ScriptRunner::start(const std::string& executionId, const std::string& content,
                         const std::string& language) {
    if (language != "lua" && !language.empty()) {
        return false;
    }
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;  // 已有脚本在跑
    }
    // running_ 已置 false 的上一线程必然已退出，安全回收
    if (worker_.joinable()) {
        worker_.join();
    }
    {
        std::lock_guard<std::mutex> lock(executionMutex_);
        executionId_ = executionId;
    }
    stopFlag_.store(false, std::memory_order_relaxed);
    worker_ = std::thread(&ScriptRunner::threadMain, this,
                          executionId, content, language);
    return true;
}

void ScriptRunner::stop() {
    stopFlag_.store(true, std::memory_order_relaxed);
}

std::string ScriptRunner::executionId() const {
    std::lock_guard<std::mutex> lock(executionMutex_);
    return executionId_;
}

void ScriptRunner::setOutputCallback(OutputCallback callback) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    output_ = std::move(callback);
}

void ScriptRunner::setHostBridge(platform::android::AndroidHostBridge* bridge) {
    hostBridge_ = bridge;
}

void ScriptRunner::setFilesDir(std::string dir) {
    filesDir_ = std::move(dir);
}

void ScriptRunner::emitLine(const std::string& line) {
    OutputCallback cb;
    {
        std::lock_guard<std::mutex> lock(outputMutex_);
        cb = output_;
    }
    if (cb) {
        cb(line);
    }
}

void ScriptRunner::threadMain(std::string executionId, std::string content,
                              std::string language) {
    runSync(executionId, content, language, stopFlag_,
            [this](const std::string& line) { emitLine(line); });
    running_.store(false, std::memory_order_release);
}

bool ScriptRunner::runSync(const std::string& executionId, const std::string& content,
                           const std::string& language, std::atomic<bool>& stopFlag,
                           const OutputCallback& output) {
    (void)language;  // A1 仅 lua

    sol::state lua;
    // 白名单标准库：base/string/math/table/os 常规脚本够用；
    // io/package（文件系统、任意加载）不开放
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math,
                       sol::lib::table, sol::lib::os);

    lua.set_function("print", [&lua, &stopFlag, &output](sol::variadic_args va) {
        luaPrint(lua, stopFlag, output, va);
    });

    lua["wingman"] = lua.create_table();
    lua["wingman"]["log"] = [&lua, &stopFlag, &output](sol::variadic_args va) {
        luaPrint(lua, stopFlag, output, va);
    };
    lua["wingman"]["sleep"] = [&stopFlag](double ms) { wingmanSleep(stopFlag, ms); };

    // A2 能力 API（input/screen/vision）：桥为 null 时函数仍注册但降级
    // 返回 false/nil（无宿主环境行为可预期，见 android_script_api.hpp）
    registerAndroidApis(lua, stopFlag, hostBridge_, filesDir_);

    // 注册指令计数 hook（注册表携带停止标志指针）
    lua_State* L = lua.lua_state();
    lua_pushlightuserdata(L, &stopFlag);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &kStopFlagKey);
    lua_sethook(L, stopHookThunk, LUA_MASKCOUNT, kHookInstructionCount);

    bool ok = true;
    sol::protected_function_result result = lua.safe_script(content, sol::script_pass_on_error);
    if (!result.valid()) {
        ok = false;
        const std::string err = [&]() -> std::string {
            try {
                sol::error e = result;
                return e.what();
            } catch (...) {
                return "unknown lua error";
            }
        }();
        output("[" + executionId + "] ERROR: " + err);
    } else if (stopFlag.load(std::memory_order_relaxed)) {
        ok = false;
        output("[" + executionId + "] stopped");
    } else {
        output("[" + executionId + "] finished");
    }

    lua_sethook(L, nullptr, 0, 0);
    return ok;
}

} // namespace wingman::android
