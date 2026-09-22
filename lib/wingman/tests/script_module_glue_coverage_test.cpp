// script_module.cpp 胶水层补测（2026-09-22 覆盖率第五批）：12 个 wingman.script.*
// 函数此前零覆盖——没有任何测试把 ScriptManager 注入 g_scriptManager（runtime
// 专属入口）。本文件以真实 ScriptManager（wingman::lua 注册引擎）+ RAII 注入
// 走全链：load/run/getState/list 真脚本生命周期，env/config/hotReload 全局接口，
// 以及 mgr 为 null 与参数不足的全部防御分支。
#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录

#include "wingman/script/module_registry.hpp"
#include "wingman/script/runtime_injections.hpp"
#include "wingman/script_manager.hpp"

#include <fstream>

#ifdef WINGMAN_HAS_LUA
#include "wingman/lua/lua_script_engine.hpp"
#endif

using namespace wingman;
using wingman::script::ScriptValue;
using wingman::script::ModuleDescriptor;
using wingman::script::modules::getAllModules;

namespace {

const ModuleDescriptor::FunctionEntry* findFn(const ModuleDescriptor& mod, const std::string& name) {
    for (const auto& f : mod.functions) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

ScriptValue call(const ModuleDescriptor& mod, const std::string& name,
                 std::vector<ScriptValue> args = {}) {
    const auto* fn = findFn(mod, name);
    EXPECT_NE(fn, nullptr) << "missing function: " << name;
    if (!fn) return ScriptValue::null();
    return (*fn)(args);
}

std::string writeScript(const std::string& name, const std::string& content) {
    std::string path = std::string(::testing::TempDir()) + name;
    std::ofstream f(path);
    f << content;
    return path;
}

// 注入 RAII：用例结束必须复位，避免全局指针泄漏影响其他用例
class ScriptModuleGlueTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef WINGMAN_HAS_LUA
        wingman::lua::registerLuaEngine();
#endif
        mgr_.setOutputCallback([](const std::string&, const std::string&) {});
        script::setScriptManager(&mgr_);
        mod_ = getModule();
        ASSERT_FALSE(mod_.name.empty());
    }

    void TearDown() override {
        script::setScriptManager(nullptr);
    }

    static ModuleDescriptor getModule() {
        for (auto& m : getAllModules()) {
            if (m.name == "script") return m;
        }
        return {};
    }

    ScriptManager mgr_;
    ModuleDescriptor mod_;
};

} // namespace

// ========== mgr 为 null 的全部默认分支 ==========

TEST_F(ScriptModuleGlueTest, NullManagerReturnsDefaults) {
    script::setScriptManager(nullptr);

    EXPECT_EQ(call(mod_, "list").isArray(), true);
    EXPECT_EQ(call(mod_, "list").arrayVal.size(), 0u);
    EXPECT_EQ(call(mod_, "getState", {ScriptValue::fromString("x")}).asString(), "");
    EXPECT_EQ(call(mod_, "isRunning", {ScriptValue::fromString("x")}).asBool(), false);
    EXPECT_EQ(call(mod_, "has", {ScriptValue::fromString("x")}).asBool(), false);
    EXPECT_EQ(call(mod_, "reload", {ScriptValue::fromString("x")}).asBool(), false);
    EXPECT_EQ(call(mod_, "run", {ScriptValue::fromString("x")}).asBool(), false);
    EXPECT_EQ(call(mod_, "stop", {ScriptValue::fromString("x")}).asBool(), false);
    EXPECT_EQ(call(mod_, "setEnv", {ScriptValue::fromString("n"), ScriptValue::fromString("k"),
                                    ScriptValue::fromString("v")}).asBool(), false);
    EXPECT_EQ(call(mod_, "getEnv", {ScriptValue::fromString("n"), ScriptValue::fromString("k")}).asString(), "");
    EXPECT_EQ(call(mod_, "setConfig", {ScriptValue::fromString("k"), ScriptValue::fromString("v")}).asBool(), false);
    EXPECT_EQ(call(mod_, "getConfig", {ScriptValue::fromString("k")}).asString(), "");
    EXPECT_EQ(call(mod_, "setHotReload", {ScriptValue::fromBool(true)}).asBool(), false);
    EXPECT_EQ(call(mod_, "load", {ScriptValue::fromString("n"), ScriptValue::fromString("p")}).asBool(), false);
}

// ========== 参数不足防御分支（有 mgr）==========

TEST_F(ScriptModuleGlueTest, InsufficientArgsReturnDefaults) {
    EXPECT_EQ(call(mod_, "getState").asString(), "");
    EXPECT_EQ(call(mod_, "isRunning").asBool(), false);
    EXPECT_EQ(call(mod_, "has").asBool(), false);
    EXPECT_EQ(call(mod_, "reload").asBool(), false);
    EXPECT_EQ(call(mod_, "run").asBool(), false);
    EXPECT_EQ(call(mod_, "stop").asBool(), false);
    // setEnv 需 3 参 / getEnv 需 2 参
    EXPECT_EQ(call(mod_, "setEnv", {ScriptValue::fromString("k")}).asBool(), false);
    EXPECT_EQ(call(mod_, "getEnv", {ScriptValue::fromString("n")}).asString(), "");
    EXPECT_EQ(call(mod_, "setConfig", {ScriptValue::fromString("k")}).asBool(), false);
    EXPECT_EQ(call(mod_, "getConfig").asString(), "");
    EXPECT_EQ(call(mod_, "setHotReload").asBool(), false);
    EXPECT_EQ(call(mod_, "load", {ScriptValue::fromString("n")}).asBool(), false);
}

// ========== load + list 生命周期 ==========

TEST_F(ScriptModuleGlueTest, LoadWithConfigObjectAndList) {
    const auto path = writeScript("glue_list.lua", "return 1\n");
    std::unordered_map<std::string, ScriptValue> cfg;
    cfg["autoReload"] = ScriptValue::fromBool(true);
    cfg["sandboxed"] = ScriptValue::fromBool(false);
    cfg["timeoutMs"] = ScriptValue::fromInt(12345);
    EXPECT_EQ(call(mod_, "load", {ScriptValue::fromString("glue_list"),
                                  ScriptValue::fromString(path),
                                  ScriptValue::fromObject(std::move(cfg))}).asBool(), true);

    // config 三字段生效（直接对照 manager 内部状态）
    const auto info = mgr_.getScriptInfo("glue_list");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->config.autoReload, true);
    EXPECT_EQ(info->config.sandboxed, false);
    EXPECT_EQ(info->config.timeoutMs, 12345);

    // list 输出 infoToValue 全字段
    const auto arr = call(mod_, "list").arrayVal;
    ASSERT_GE(arr.size(), 1u);
    const ScriptValue* entry = nullptr;
    for (const auto& v : arr) {
        if (v.get("name") && v.get("name")->asString() == "glue_list") { entry = &v; break; }
    }
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->get("path")->asString(), path);
    EXPECT_EQ(entry->get("state")->asString(), "unloaded");
    EXPECT_EQ(entry->get("language")->asString(), "lua");
    EXPECT_EQ(entry->get("lastError"), nullptr); // 无错误时不含 lastError 键
}

TEST_F(ScriptModuleGlueTest, ListIncludesLastErrorAfterFailure) {
    const auto path = writeScript("glue_bad.lua", "this is not valid lua\n");
    ASSERT_TRUE(mgr_.loadScript("glue_bad", path));
    EXPECT_FALSE(mgr_.runScript("glue_bad")); // 语法错误：run 返回 false，错误进 lastError

    const auto arr = call(mod_, "list").arrayVal;
    const ScriptValue* entry = nullptr;
    for (const auto& v : arr) {
        if (v.get("name") && v.get("name")->asString() == "glue_bad") { entry = &v; break; }
    }
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->get("state")->asString(), "error");
    ASSERT_NE(entry->get("lastError"), nullptr);
    EXPECT_FALSE(entry->get("lastError")->asString().empty());
}

// ========== getState / isRunning / has / run / stop / reload ==========

TEST_F(ScriptModuleGlueTest, StateQueriesAndLifecycle) {
    const auto path = writeScript("glue_life.lua", "return 42\n");
    ASSERT_TRUE(mgr_.loadScript("glue_life", path));

    EXPECT_EQ(call(mod_, "getState", {ScriptValue::fromString("glue_life")}).asString(), "unloaded");
    EXPECT_EQ(call(mod_, "getState", {ScriptValue::fromString("nope")}).asString(), "unknown");
    // 同步执行模型下 runScript 返回时已 completed，无 running 中间态
    EXPECT_EQ(call(mod_, "isRunning", {ScriptValue::fromString("glue_life")}).asBool(), false);

    EXPECT_EQ(call(mod_, "has", {ScriptValue::fromString("glue_life")}).asBool(), true);
    EXPECT_EQ(call(mod_, "has", {ScriptValue::fromString("nope")}).asBool(), false);

    EXPECT_EQ(call(mod_, "run", {ScriptValue::fromString("glue_life")}).asBool(), true);
    EXPECT_EQ(call(mod_, "getState", {ScriptValue::fromString("glue_life")}).asString(), "completed");

    // completed 状态下 stop 返回 false（非 running/paused/starting）
    EXPECT_EQ(call(mod_, "stop", {ScriptValue::fromString("glue_life")}).asBool(), false);
    EXPECT_EQ(call(mod_, "stop", {ScriptValue::fromString("nope")}).asBool(), false);

    EXPECT_EQ(call(mod_, "reload", {ScriptValue::fromString("glue_life")}).asBool(), true);
    EXPECT_EQ(call(mod_, "reload", {ScriptValue::fromString("nope")}).asBool(), false);
    // reload 重建引擎，状态为 loaded（与初始 loadScript 仅注册的 unloaded 区分）
    EXPECT_EQ(call(mod_, "getState", {ScriptValue::fromString("glue_life")}).asString(), "loaded");
}

// ========== env / config / hotReload ==========

TEST_F(ScriptModuleGlueTest, EnvAndConfigRoundtrip) {
    // setEnv/getEnv 为 manager 全局环境（name 参数保留兼容调用形态）
    EXPECT_EQ(call(mod_, "setEnv", {ScriptValue::fromString("any"),
                                    ScriptValue::fromString("GLUE_KEY"),
                                    ScriptValue::fromString("glue-val")}).asBool(), true);
    EXPECT_EQ(call(mod_, "getEnv", {ScriptValue::fromString("any"),
                                    ScriptValue::fromString("GLUE_KEY")}).asString(), "glue-val");
    EXPECT_EQ(call(mod_, "getEnv", {ScriptValue::fromString("any"),
                                    ScriptValue::fromString("MISSING_KEY")}).asString(), "");

    EXPECT_EQ(call(mod_, "setConfig", {ScriptValue::fromString("glue.config"),
                                       ScriptValue::fromString("v1")}).asBool(), true);
    EXPECT_EQ(call(mod_, "getConfig", {ScriptValue::fromString("glue.config")}).asString(), "v1");
    EXPECT_EQ(call(mod_, "getConfig", {ScriptValue::fromString("glue.missing")}).asString(), "");
}

TEST_F(ScriptModuleGlueTest, SetHotReloadBothWays) {
    EXPECT_EQ(call(mod_, "setHotReload", {ScriptValue::fromBool(true)}).asBool(), true);
    EXPECT_EQ(call(mod_, "setHotReload", {ScriptValue::fromBool(false)}).asBool(), true);
}
