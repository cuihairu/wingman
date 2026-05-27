#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/json.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// Helper: find a function by module name and function name
static ModuleDescriptor::FunctionEntry findFunction(const std::string& moduleName, const std::string& funcName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (mod.name == moduleName) {
            for (const auto& fn : mod.functions) {
                if (fn.name == funcName) return fn;
            }
        }
    }
    return {};
}

static bool hasModule(const std::string& moduleName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (mod.name == moduleName) return true;
    }
    return false;
}

// ========== JSON module functions ==========

TEST(JsonModuleFunctionsTest, DecodeValidJson) {
    auto fn = findFunction("json", "decode");
    ASSERT_FALSE(fn.name.empty());

    auto result = fn({ScriptValue::fromString(R"({"name":"test","count":42,"active":true})")});
    EXPECT_TRUE(result.isObject());

    auto* name = result.get("name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->asString(), "test");

    auto* count = result.get("count");
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->asInt(), 42);

    auto* active = result.get("active");
    ASSERT_NE(active, nullptr);
    EXPECT_TRUE(active->asBool());
}

TEST(JsonModuleFunctionsTest, DecodeArray) {
    auto fn = findFunction("json", "decode");
    auto result = fn({ScriptValue::fromString("[1,2,3]")});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 3u);
}

TEST(JsonModuleFunctionsTest, DecodeNested) {
    auto fn = findFunction("json", "decode");
    auto result = fn({ScriptValue::fromString(R"({"obj":{"a":1},"arr":[2,3]})")});
    EXPECT_TRUE(result.isObject());

    auto* obj = result.get("obj");
    ASSERT_NE(obj, nullptr);
    EXPECT_TRUE(obj->isObject());
    auto* a = obj->get("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->asInt(), 1);

    auto* arr = result.get("arr");
    ASSERT_NE(arr, nullptr);
    EXPECT_TRUE(arr->isArray());
    EXPECT_EQ(arr->size(), 2u);
}

TEST(JsonModuleFunctionsTest, EncodeCompact) {
    auto fn = findFunction("encode");
    auto obj = ScriptValue::fromObject({
        {"key", ScriptValue::fromString("value")}
    });
    auto result = fn({obj});
    EXPECT_TRUE(result.isString());
    EXPECT_NE(result.asString().find("key"), std::string::npos);
    EXPECT_NE(result.asString().find("value"), std::string::npos);
}

TEST(JsonModuleFunctionsTest, EncodeWithIndent) {
    auto fn = findFunction("encode");
    auto obj = ScriptValue::fromObject({
        {"key", ScriptValue::fromInt(42)}
    });
    auto result = fn({obj, ScriptValue::fromInt(2)});
    EXPECT_TRUE(result.isString());
    EXPECT_NE(result.asString().find("key"), std::string::npos);
}

TEST(JsonModuleFunctionsTest, EncodeArray) {
    auto fn = findFunction("encode");
    auto arr = ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)});
    auto result = fn({arr});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString(), "[1,2]");
}

TEST(JsonModuleFunctionsTest, EncodeBool) {
    auto fn = findFunction("encode");
    auto result = fn({ScriptValue::fromBool(true)});
    EXPECT_EQ(result.asString(), "true");
}

TEST(JsonModuleFunctionsTest, EncodeNull) {
    auto fn = findFunction("encode");
    auto result = fn({ScriptValue::null()});
    EXPECT_EQ(result.asString(), "null");
}

// ========== System module functions ==========

TEST(SystemModuleFunctionsTest, GetCpuInfo) {
    auto fn = findFunction("system", "getCpuInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    // Should have 'cores' key
    auto* cores = result.get("cores");
    ASSERT_NE(cores, nullptr);
    EXPECT_GT(cores->asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetMemoryInfo) {
    auto fn = findFunction("system", "getMemoryInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    auto* total = result.get("total");
    ASSERT_NE(total, nullptr);
    EXPECT_GT(total->asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetOsInfo) {
    auto fn = findFunction("system", "getOsInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    auto* platform = result.get("platform");
    ASSERT_NE(platform, nullptr);
    EXPECT_FALSE(platform->asString().empty());
}

TEST(SystemModuleFunctionsTest, GetUptime) {
    auto fn = findFunction("system", "getUptime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetDateTime) {
    auto fn = findFunction("system", "getDateTime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
    EXPECT_FALSE(result.asString().empty());
}

TEST(SystemModuleFunctionsTest, GetTimeZone) {
    auto fn = findFunction("system", "getTimeZone");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
}

TEST(SystemModuleFunctionsTest, GetProcessCount) {
    auto fn = findFunction("system", "getProcessCount");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetThreadCount) {
    auto fn = findFunction("system", "getThreadCount");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(SystemModuleFunctionsTest, GetGpuInfo) {
    auto fn = findFunction("system", "getGpuInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetDiskInfoDefault) {
    auto fn = findFunction("system", "getDiskInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetDisplayInfo) {
    auto fn = findFunction("system", "getDisplayInfo");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(SystemModuleFunctionsTest, GetNetworkAdapters) {
    auto fn = findFunction("system", "getNetworkAdapters");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

// ========== Orchestration module (stubs) ==========

TEST(OrchestrationModuleFunctionsTest, SubmitWorkflowReturnsNull) {
    auto fn = findFunction("orchestration", "submit_workflow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromObject({})});
    EXPECT_TRUE(result.isNull());
}

TEST(OrchestrationModuleFunctionsTest, CancelWorkflowReturnsFalse) {
    auto fn = findFunction("orchestration", "cancel_workflow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("wf1")});
    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(result.asBool());
}

TEST(OrchestrationModuleFunctionsTest, GetWorkflowReturnsNull) {
    auto fn = findFunction("orchestration", "get_workflow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("wf1")});
    EXPECT_TRUE(result.isNull());
}

TEST(OrchestrationModuleFunctionsTest, GetAllWorkflowsReturnsEmptyArray) {
    auto fn = findFunction("orchestration", "get_all_workflows");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 0u);
}

// ========== Team module (stubs) ==========

TEST(TeamModuleFunctionsTest, RegisterReturnsNull) {
    auto fn = findFunction("team", "register");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("user1")});
    EXPECT_TRUE(result.isNull());
}

TEST(TeamModuleFunctionsTest, JoinReturnsFalse) {
    auto fn = findFunction("team", "join");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("team1")});
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, LeaveReturnsFalse) {
    auto fn = findFunction("team", "leave");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, SendReturnsFalse) {
    auto fn = findFunction("team", "send");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("action")});
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, PollReturnsEmptyArray) {
    auto fn = findFunction("team", "poll");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(TeamModuleFunctionsTest, MembersReturnsEmptyArray) {
    auto fn = findFunction("team", "members");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(TeamModuleFunctionsTest, InfoReturnsObject) {
    auto fn = findFunction("team", "info");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

// ========== SmartTrigger module functions ==========

TEST(SmartTriggerModuleFunctionsTest, CreateTrigger) {
    auto fn = findFunction("smarttrigger", "create");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test_st_func")});
    EXPECT_TRUE(result.asBool());

    // Cleanup
    SmartTriggerManager::instance().removeTrigger("test_st_func");
}

TEST(SmartTriggerModuleFunctionsTest, StartNonexistent) {
    auto fn = findFunction("smarttrigger", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_st")});
    EXPECT_FALSE(result.asBool());
}

TEST(SmartTriggerModuleFunctionsTest, StopNonexistent) {
    auto fn = findFunction("smarttrigger", "stop");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_st")});
    EXPECT_FALSE(result.asBool());
}

TEST(SmartTriggerModuleFunctionsTest, RemoveDoesNotCrash) {
    auto fn = findFunction("smarttrigger", "remove");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_st")}));
}

// ========== BehaviorTree module functions ==========

TEST(BtModuleFunctionsTest, CreateTree) {
    auto fn = findFunction("bt", "create");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test_bt_func")});
    EXPECT_TRUE(result.asBool());

    // Cleanup
    BehaviorTreeManager::instance().removeTree("test_bt_func");
}

TEST(BtModuleFunctionsTest, TickNonexistent) {
    auto fn = findFunction("bt", "tick");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_bt")});
    EXPECT_EQ(result.asString(), "FAILURE");
}

TEST(BtModuleFunctionsTest, RemoveDoesNotCrash) {
    auto fn = findFunction("bt", "remove");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_bt")}));
}

// ========== QRCode module functions ==========

TEST(QrcodeModuleFunctionsTest, CancelDoesNotCrash) {
    auto fn = findFunction("qrcode", "cancel");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({}));
}

// ========== Module presence verification ==========

TEST(ModulePresenceTest, AllExpectedModulesExist) {
    std::vector<std::string> expectedModules = {
        "json", "system", "orchestration", "team",
        "smarttrigger", "bt", "qrcode", "ocr",
        "screen", "input", "window", "vision",
        "http", "human", "verification", "gameprofile",
        "performance", "config", "kv", "debugger",
        "misc"
    };

    for (const auto& name : expectedModules) {
        EXPECT_TRUE(hasModule(name)) << "Module '" << name << "' not found";
    }
}

TEST(ModulePresenceTest, ModuleFunctionsAreCallable) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        for (const auto& fn : mod.functions) {
            EXPECT_NO_THROW(fn.func({ScriptValue::fromString("test")}))
                << "Module '" << mod.name << "' function '" << fn.name << "' threw";
        }
    }
}
