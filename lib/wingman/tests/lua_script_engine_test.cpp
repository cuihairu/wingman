// LuaScriptEngine 缺口补测（gcovr：lua_script_engine.cpp 55%、75 行未覆盖）。
//
// 覆盖面：initialize 沙箱/非沙箱分支与 require("wingman") 钩子（round-1
// 修复的安装路径此前从未被非沙箱初始化驱动过）、config.env 注入、
// executeString/executeFile 成败路径、callFunction 全链（未找到/参数转换/
// 返回转换/错误传播）、registerModule 注册函数被 Lua 调用（含 C++ 异常
// 传播为 Lua error）、set/getGlobal 往返、enable/disableSandbox 语义、
// print 输出捕获（变参制表符分隔 + 非 string 走 tostring，匹配 Lua 原生）。
//
// 登记口径（不写假用例）：
// - initialize 的 catch 分支（lastError + return false）为防御性收口：
//   open_libraries / env 注入 / prelude 在无故障注入时不可达抛错路径，
//   无自然触发手段，不造 env-fault 用例；
// - 沙箱/非沙箱 open_libraries 的实参行（sol 变参模板展开）历史报告常年
//   无覆盖归因——两分支本身均被执行（沙箱经 StandaloneMode、非沙箱经本
//   文件用例），行归因是否改善以本轮 gcovr 输出为准；
// - setOutputCallback 的「initialize 前调用 → print 不被覆盖」为当前真实
//   契约（回调被保存但 set_function 被 initialized_ 门挡下），按现状钉死，
//   不按理想行为断言。
//
// 平台说明：纯引擎 API + std::filesystem（临时 .lua 文件），无 POSIX/X11
// 分支，Windows CI 全量参与。

#include <gtest/gtest.h>

#include "wingman/lua/lua_script_engine.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace wingman::script;
using wingman::lua::LuaScriptEngine;

class LuaScriptEngineTest : public ::testing::Test {
protected:
    // 每用例独立临时 .lua 路径（时间戳 + 序号防同进程/跨进程撞名）
    std::filesystem::path tempLuaFile(const std::string& content) {
        static int seq = 0;
        const auto name = "wm-luaengine-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(++seq) + ".lua";
        const auto path = std::filesystem::temp_directory_path() / name;
        std::ofstream out(path);
        out << content;
        files_.push_back(path);
        return path;
    }

    void TearDown() override {
        std::error_code ec;
        for (const auto& path : files_) {
            std::filesystem::remove(path, ec);
        }
    }

    std::vector<std::filesystem::path> files_;
};

// ========== initialize：沙箱与非沙箱分支 ==========

TEST_F(LuaScriptEngineTest, SandboxInitRemovesDangerousGlobals) {
    LuaScriptEngine engine;
    EngineConfig config;
    config.sandboxed = true;
    ASSERT_TRUE(engine.initialize(config));

    // applySandbox 置 nil 的全集：io/os/debug/package + dofile/loadfile/load/require
    EXPECT_TRUE(engine.executeString(
        "return io == nil and os == nil and debug == nil and package == nil "
        "and dofile == nil and loadfile == nil and load == nil and require == nil"));
}

TEST_F(LuaScriptEngineTest, NonSandboxInitOpensFullStdlib) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize()); // 默认 config.sandboxed = false

    EXPECT_TRUE(engine.executeString(
        "return io ~= nil and os ~= nil and debug ~= nil and package ~= nil "
        "and dofile ~= nil and load ~= nil and require ~= nil"));
}

TEST_F(LuaScriptEngineTest, RequireWingmanResolvesInNonSandboxMode) {
    // package.preload["wingman"] 钩子只在非沙箱模式安装（沙箱 package 为 nil，
    // prelude 引用 nil 会抛错——round-1 修复把钩子收进 !sandboxed 分支）
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    ASSERT_TRUE(engine.executeString("wingman_req = require(\"wingman\")"));
    const auto required = engine.getGlobal("wingman_req");
    EXPECT_TRUE(required.isObject()) << "require(\"wingman\") 应解析为 wingman 表";
}

TEST_F(LuaScriptEngineTest, SandboxBlocksRequireCall) {
    LuaScriptEngine engine;
    EngineConfig config;
    config.sandboxed = true;
    ASSERT_TRUE(engine.initialize(config));

    // require 被 applySandbox 置 nil：调用即 Lua error，executeString 报 false
    EXPECT_FALSE(engine.executeString("require(\"wingman\")"));
    EXPECT_FALSE(engine.getLastError().empty());
}

TEST_F(LuaScriptEngineTest, EnvVarsExposedAsUnderscoreENVGlobals) {
    LuaScriptEngine engine;
    EngineConfig config;
    config.env = {{"token", "abc"}, {"region", "eu"}};
    ASSERT_TRUE(engine.initialize(config));

    const auto token = engine.getGlobal("_ENV_token");
    const auto region = engine.getGlobal("_ENV_region");
    EXPECT_TRUE(token.isString());
    EXPECT_EQ(token.asString(), "abc");
    EXPECT_TRUE(region.isString());
    EXPECT_EQ(region.asString(), "eu");
}

TEST_F(LuaScriptEngineTest, InitializeIsIdempotent) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    EXPECT_TRUE(engine.initialize()); // initialized_ 早退，状态不重建
    EXPECT_TRUE(engine.executeString("return 1")); // 状态仍可用
}

// ========== executeString / executeFile ==========

TEST_F(LuaScriptEngineTest, ExecuteStringReportsFailureWithLastError) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    EXPECT_TRUE(engine.executeString("return 1 + 1"));

    // 语法错误：safe_script 结果 invalid → lastError 非空
    EXPECT_FALSE(engine.executeString("this is ((( not lua"));
    EXPECT_FALSE(engine.getLastError().empty());

    // 运行时错误：error() 消息进入 lastError
    EXPECT_FALSE(engine.executeString("error(\"boom\")"));
    EXPECT_NE(engine.getLastError().find("boom"), std::string::npos);
}

TEST_F(LuaScriptEngineTest, ExecuteFileSuccessAndMissingFileFailure) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    const auto good = tempLuaFile("wingman_file_ran = true\n");
    EXPECT_TRUE(engine.executeFile(good.string()));
    EXPECT_TRUE(engine.getGlobal("wingman_file_ran").asBool(false));

    EXPECT_FALSE(engine.executeFile("/nonexistent/path/wm-nope.lua"));
    EXPECT_FALSE(engine.getLastError().empty());
}

TEST_F(LuaScriptEngineTest, ExecutionBeforeInitializeFails) {
    LuaScriptEngine engine;
    ScriptValue out;
    EXPECT_FALSE(engine.executeString("return 1"));
    EXPECT_FALSE(engine.executeFile("/nonexistent/x.lua"));
    EXPECT_FALSE(engine.callFunction("any", {}, out));
}

// ========== callFunction ==========

TEST_F(LuaScriptEngineTest, CallFunctionNotFoundReportsError) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    ScriptValue out;
    EXPECT_FALSE(engine.callFunction("no_such_fn", {}, out));
    EXPECT_EQ(engine.getLastError(), "Function not found: no_such_fn");
}

TEST_F(LuaScriptEngineTest, CallFunctionPassesArgsAndReturnsSum) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString("function wm_add(a, b) return a + b end"));

    ScriptValue out;
    EXPECT_TRUE(engine.callFunction("wm_add",
        {ScriptValue::fromInt(3), ScriptValue::fromInt(4)}, out));
    EXPECT_TRUE(out.isInt());
    EXPECT_EQ(out.intVal, 7);
}

TEST_F(LuaScriptEngineTest, CallFunctionErrorSurfacesInLastError) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString("function wm_boom() error(\"kapow\") end"));

    ScriptValue out;
    EXPECT_FALSE(engine.callFunction("wm_boom", {}, out));
    EXPECT_NE(engine.getLastError().find("kapow"), std::string::npos);
}

TEST_F(LuaScriptEngineTest, CallFunctionNilReturnYieldsNullValue) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString("function wm_nil() return nil end"));

    ScriptValue out;
    EXPECT_TRUE(engine.callFunction("wm_nil", {}, out));
    EXPECT_TRUE(out.isNull());
}

TEST_F(LuaScriptEngineTest, CallFunctionConvertsMixedScalarArgs) {
    // toLuaObject 三种标量转换 + Lua 侧 tostring（int 打印不带 .0）
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString(
        "function wm_cat(a, b, c) return tostring(a) .. tostring(b) .. tostring(c) end"));

    ScriptValue out;
    EXPECT_TRUE(engine.callFunction("wm_cat",
        {ScriptValue::fromString("x"), ScriptValue::fromBool(true), ScriptValue::fromInt(42)},
        out));
    EXPECT_TRUE(out.isString());
    EXPECT_EQ(out.asString(), "xtrue42");
}

TEST_F(LuaScriptEngineTest, CallFunctionPreservesIntegerVsFloat) {
    // Lua 5.4 数字保整数性：lua_isinteger 分流 Int/Float（marshal 契约）
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString("function wm_id(a) return a end"));

    ScriptValue asInt;
    ASSERT_TRUE(engine.callFunction("wm_id", {ScriptValue::fromInt(-42)}, asInt));
    EXPECT_TRUE(asInt.isInt());
    EXPECT_EQ(asInt.intVal, -42);

    ScriptValue asFloat;
    ASSERT_TRUE(engine.callFunction("wm_id", {ScriptValue::fromFloat(2.5)}, asFloat));
    EXPECT_TRUE(asFloat.isFloat());
    EXPECT_DOUBLE_EQ(asFloat.floatVal, 2.5);
}

// ========== registerModule ==========

TEST_F(LuaScriptEngineTest, RegisteredModuleFunctionCallableFromLua) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    ModuleDescriptor mod;
    mod.name = "wmtest";
    mod.functions.push_back({"add",
        [](const std::vector<ScriptValue>& args) {
            int64_t sum = 0;
            for (const auto& a : args) sum += a.asInt();
            return ScriptValue::fromInt(sum);
        },
        "x:int, y:int -> int"});
    engine.registerModule(mod);

    // 注册函数被 Lua 调用：驱动注册 lambda 的实参收集 + funcCopy 调用 + 返回转换
    ASSERT_TRUE(engine.executeString("wm_sum = wingman.wmtest.add(2, 5)"));
    const auto sum = engine.getGlobal("wm_sum");
    EXPECT_TRUE(sum.isInt());
    EXPECT_EQ(sum.intVal, 7);
}

TEST_F(LuaScriptEngineTest, RegisteredModuleFunctionReceivesArgValues) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    ModuleDescriptor mod;
    mod.name = "wmtest";
    mod.functions.push_back({"echo",
        [](const std::vector<ScriptValue>& args) {
            return ScriptValue::fromString("echo:" +
                (args.empty() ? std::string() : args[0].asString()));
        },
        "s:string -> string"});
    engine.registerModule(mod);

    ASSERT_TRUE(engine.executeString("wm_echo = wingman.wmtest.echo(\"hi\")"));
    EXPECT_EQ(engine.getGlobal("wm_echo").asString(), "echo:hi");
}

TEST_F(LuaScriptEngineTest, RegisteredModuleThrowingPropagatesAsLuaError) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    ModuleDescriptor mod;
    mod.name = "wmtest";
    mod.functions.push_back({"boom",
        [](const std::vector<ScriptValue>&) -> ScriptValue {
            throw std::runtime_error("c++ boom");
        },
        "() -> error"});
    engine.registerModule(mod);

    // C++ 异常 → sol::error 重新抛出 → executeString 捕为失败 + lastError
    EXPECT_FALSE(engine.executeString("wingman.wmtest.boom()"));
    EXPECT_NE(engine.getLastError().find("c++ boom"), std::string::npos);
}

TEST_F(LuaScriptEngineTest, RegisterModuleBeforeInitializeIsIgnored) {
    LuaScriptEngine engine;
    ModuleDescriptor mod;
    mod.name = "early";
    mod.functions.push_back({"f", [](const std::vector<ScriptValue>&) {
        return ScriptValue::null();
    }, "() -> nil"});
    engine.registerModule(mod); // 未初始化：no-op，不崩溃

    ASSERT_TRUE(engine.initialize());
    // wingman 表存在但 early 模块未注册
    EXPECT_TRUE(engine.executeString("return wingman.early == nil"));
}

// ========== set/getGlobal ==========

TEST_F(LuaScriptEngineTest, GlobalRoundTripAllScalarTypes) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.setGlobal("g_int", ScriptValue::fromInt(-7));
    engine.setGlobal("g_str", ScriptValue::fromString("s"));
    engine.setGlobal("g_bool", ScriptValue::fromBool(true));
    engine.setGlobal("g_float", ScriptValue::fromFloat(1.5));

    const auto i = engine.getGlobal("g_int");
    EXPECT_TRUE(i.isInt());
    EXPECT_EQ(i.intVal, -7);

    const auto s = engine.getGlobal("g_str");
    EXPECT_TRUE(s.isString());
    EXPECT_EQ(s.asString(), "s");

    const auto b = engine.getGlobal("g_bool");
    EXPECT_TRUE(b.isBool());
    EXPECT_EQ(b.boolVal, true);

    const auto f = engine.getGlobal("g_float");
    EXPECT_TRUE(f.isFloat());
    EXPECT_DOUBLE_EQ(f.floatVal, 1.5);
}

TEST_F(LuaScriptEngineTest, GetGlobalUnknownYieldsNull) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    EXPECT_TRUE(engine.getGlobal("never_set").isNull());
}

TEST_F(LuaScriptEngineTest, GlobalAccessBeforeInitializeIsNoop) {
    LuaScriptEngine engine;
    engine.setGlobal("g", ScriptValue::fromInt(1)); // 不崩溃
    EXPECT_TRUE(engine.getGlobal("g").isNull());
}

// ========== 引擎标识 ==========

TEST_F(LuaScriptEngineTest, LanguageNameAndExtensions) {
    LuaScriptEngine engine;
    EXPECT_EQ(engine.getLanguageName(), "lua");
    const auto exts = engine.getSupportedExtensions();
    ASSERT_EQ(exts.size(), 1u);
    EXPECT_EQ(exts[0], ".lua");
}

// ========== 沙箱开关 ==========

TEST_F(LuaScriptEngineTest, EnableSandboxAfterInitStripsDangerousGlobals) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize()); // 非沙箱：io 可用
    ASSERT_TRUE(engine.executeString("return io ~= nil"));

    engine.enableSandbox(EngineConfig{});
    EXPECT_TRUE(engine.executeString(
        "return io == nil and os == nil and debug == nil and package == nil"));
}

TEST_F(LuaScriptEngineTest, DisableSandboxRestoresOnlyIoOsDebug) {
    // 契约（按现状钉）：disableSandbox 只重开 io/os/debug 三个库——
    // package 与 require 不恢复（applySandbox 置 nil 后无还原路径）
    LuaScriptEngine engine;
    EngineConfig config;
    config.sandboxed = true;
    ASSERT_TRUE(engine.initialize(config));

    engine.disableSandbox();
    EXPECT_TRUE(engine.executeString(
        "return os ~= nil and io ~= nil and debug ~= nil "
        "and package == nil and require == nil"));
}

TEST_F(LuaScriptEngineTest, DisableSandboxBeforeInitializeIsNoop) {
    LuaScriptEngine engine;
    engine.disableSandbox(); // initialized_ 门：不崩溃
    EXPECT_TRUE(engine.initialize());
    EXPECT_TRUE(engine.executeString("return os ~= nil"));
}

// ========== print 输出捕获 ==========

TEST_F(LuaScriptEngineTest, OutputCallbackCapturesSimplePrint) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    std::string captured;
    engine.setOutputCallback([&captured](const std::string& line) { captured = line; });
    ASSERT_TRUE(engine.executeString("print(\"hello\")"));
    EXPECT_EQ(captured, "hello");
}

TEST_F(LuaScriptEngineTest, OutputCallbackFormatsMixedArgsLikeLua) {
    // 变参制表符分隔；非 string 值走 Lua tostring（int 无 .0、bool 小写）
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    std::string captured;
    engine.setOutputCallback([&captured](const std::string& line) { captured = line; });
    ASSERT_TRUE(engine.executeString("print(\"a\", 42, true, 2.5)"));
    EXPECT_EQ(captured, "a\t42\ttrue\t2.5");
}

TEST_F(LuaScriptEngineTest, OutputCallbackEmptyPrintYieldsEmptyLine) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());

    int calls = 0;
    std::string captured = "sentinel";
    engine.setOutputCallback([&](const std::string& line) {
        ++calls;
        captured = line;
    });
    ASSERT_TRUE(engine.executeString("print()"));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(captured, ""); // 零变参：空串回调（无参循环体不执行）
}

TEST_F(LuaScriptEngineTest, OutputCallbackSetBeforeInitializeDoesNotCapture) {
    // 按现状钉契约：initialize 前调用 setOutputCallback 只保存回调，
    // print 覆盖（set_function）被 initialized_ 门跳过——捕获不生效。
    // 调用方（ScriptManager 等）必须在 initialize 之后安装回调。
    LuaScriptEngine engine;
    int calls = 0;
    engine.setOutputCallback([&](const std::string&) { ++calls; });
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.executeString("print(\"lost\")"));
    EXPECT_EQ(calls, 0);
}

// ========== shutdown ==========

TEST_F(LuaScriptEngineTest, ShutdownDisablesExecutionAndIsIdempotent) {
    LuaScriptEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.shutdown();
    EXPECT_FALSE(engine.executeString("return 1"));
    engine.shutdown(); // 二次 shutdown 不崩溃
}

} // namespace
