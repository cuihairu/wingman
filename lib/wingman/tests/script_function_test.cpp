#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/json.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/human.hpp"
#include "wingman/ui_automation.hpp"
#include <algorithm>
#include <cstdio>
#include <set>

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

static ModuleDescriptor::FunctionEntry findFunction(const std::string& funcName) {
    auto modules = getAllModules();
    for (const auto& mod : modules) {
        for (const auto& fn : mod.functions) {
            if (fn.name == funcName) return fn;
        }
    }
    return {};
}

static bool result_is_string_or_null(const ScriptValue& v) {
    return v.isString() || v.isNull();
}

// Plan 5: 配置修改测试的全局状态守卫，测试结束自动恢复 Human mouse/keyboard 配置
struct HumanConfigGuard {
    HumanMouseConfig mc = Human::mouse().getConfig();
    HumanKeyboardConfig kc = Human::keyboard().getConfig();
    ~HumanConfigGuard() {
        Human::setMouseConfig(mc);
        Human::setKeyboardConfig(kc);
    }
};

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

TEST(JsonModuleFunctionsTest, DecodeFloat) {
    auto fn = findFunction("json", "decode");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("3.14")});
    EXPECT_TRUE(result.isFloat());
    EXPECT_DOUBLE_EQ(result.asFloat(), 3.14);
}

TEST(JsonModuleFunctionsTest, EncodeFloat) {
    auto fn = findFunction("encode");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromFloat(2.718)});
    EXPECT_TRUE(result.isString());
    EXPECT_NE(result.asString().find("2.718"), std::string::npos);
}

TEST(JsonModuleFunctionsTest, EncodeCallableReturnsNull) {
    auto fn = findFunction("encode");
    ASSERT_FALSE(fn.name.empty());
    // Callable type triggers default branch in scriptToJson
    auto callable = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::null();
    });
    auto result = fn({callable});
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

// ========== Team module ==========

TEST(TeamModuleFunctionsTest, JoinTeamReturnsTrueOptimistically) {
    auto fn = findFunction("team", "joinTeam");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("team1")});
    // Returns true optimistically (local state updated immediately)
    // Actual server confirmation happens asynchronously via inbox
    EXPECT_TRUE(result.asBool());
}

TEST(TeamModuleFunctionsTest, LeaveTeamReturnsTrueWhenInTeam) {
    // First join a team
    auto joinFn = findFunction("team", "joinTeam");
    ASSERT_FALSE(joinFn.name.empty());
    joinFn({ScriptValue::fromString("team1")});

    // Then leave
    auto fn = findFunction("team", "leaveTeam");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return true when in a team
    EXPECT_TRUE(result.asBool());
}

TEST(TeamModuleFunctionsTest, CreateVoteReturnsFalseWhenNotJoined) {
    auto fn = findFunction("team", "createVote");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test vote")});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, GetTeamStatusReturnsJsonString) {
    auto fn = findFunction("team", "getTeamStatus");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return JSON string
    EXPECT_TRUE(result.isString());
}

TEST(TeamModuleFunctionsTest, IsJoinedReturnsFalseInitially) {
    auto fn = findFunction("team", "isJoined");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
}

TEST(TeamModuleFunctionsTest, GetMemberIdReturnsEmptyStringInitially) {
    auto fn = findFunction("team", "getMemberId");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    // Should return empty string when not in a team
    EXPECT_TRUE(result.asString().empty());
}

TEST(TeamModuleFunctionsTest, BroadcastReturnsFalseWhenNotJoined) {
    auto fn = findFunction("team", "broadcast");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test message")});
    // Should return false when not in a team
    EXPECT_FALSE(result.asBool());
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

// ========== Verification module functions ==========

TEST(VerificationModuleFunctionsTest, TotpGeneratesCode) {
    auto fn = findFunction("verification", "totp");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 6u);
}

TEST(VerificationModuleFunctionsTest, TotpWithDigits) {
    auto fn = findFunction("verification", "totp");
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP"), ScriptValue::fromInt(8)});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 8u);
}

TEST(VerificationModuleFunctionsTest, TotpWithPeriod) {
    auto fn = findFunction("verification", "totp");
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP"), ScriptValue::fromInt(6), ScriptValue::fromInt(60)});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 6u);
}

TEST(VerificationModuleFunctionsTest, SteamGuardGeneratesCode) {
    auto fn = findFunction("verification", "steamGuard");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 5u);
}

TEST(VerificationModuleFunctionsTest, RemainingSeconds) {
    auto fn = findFunction("verification", "remaining");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(30)});
    EXPECT_TRUE(result.isInt());
    int remaining = result.asInt();
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 30);
}

TEST(VerificationModuleFunctionsTest, VerifyCode) {
    auto totpFn = findFunction("verification", "totp");
    auto verifyFn = findFunction("verification", "verify");
    ASSERT_FALSE(totpFn.name.empty());
    ASSERT_FALSE(verifyFn.name.empty());

    auto code = totpFn({ScriptValue::fromString("JBSWY3DPEHPK3PXP")}).asString();
    EXPECT_FALSE(code.empty());
}

// ========== Config module functions ==========

TEST(ConfigModuleFunctionsTest, GetSetRemove) {
    auto setFn = findFunction("config", "set");
    auto getFn = findFunction("config", "get");
    auto removeFn = findFunction("config", "remove");

    ASSERT_FALSE(setFn.name.empty());

    setFn({ScriptValue::fromString("test_key"), ScriptValue::fromString("test_value")});
    auto result = getFn({ScriptValue::fromString("test_key")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString(), "test_value");

    auto removeResult = removeFn({ScriptValue::fromString("test_key")});
    EXPECT_TRUE(removeResult.isBool());

    auto afterRemove = getFn({ScriptValue::fromString("test_key")});
    EXPECT_TRUE(afterRemove.isNull());
}

TEST(ConfigModuleFunctionsTest, GetNonexistentReturnsNull) {
    auto fn = findFunction("config", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_key_xyz")});
    EXPECT_TRUE(result.isNull());
}

TEST(ConfigModuleFunctionsTest, SaveAndLoadDoNotCrash) {
    auto saveFn = findFunction("config", "save");
    auto loadFn = findFunction("config", "load");
    ASSERT_FALSE(saveFn.name.empty());
    EXPECT_NO_THROW(saveFn({}));
    EXPECT_NO_THROW(loadFn({}));
}

// ========== KV module functions (null store) ==========

TEST(KvModuleFunctionsTest, GetReturnsEmptyWhenNoStore) {
    auto fn = findFunction("kv", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("any_key")});
    EXPECT_TRUE(result.isString());
}

TEST(KvModuleFunctionsTest, SetReturnsNullWhenNoStore) {
    auto fn = findFunction("kv", "set");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("key"), ScriptValue::fromString("val")});
    EXPECT_TRUE(result.isNull());
}

TEST(KvModuleFunctionsTest, SetWithOptionsDoesNotCrash) {
    auto fn = findFunction("kv", "set");
    auto opts = ScriptValue::fromObject({
        {"ttl", ScriptValue::fromInt(60)},
        {"nx", ScriptValue::fromBool(true)},
        {"xx", ScriptValue::fromBool(false)}
    });
    EXPECT_NO_THROW(fn({ScriptValue::fromString("key"), ScriptValue::fromString("val"), opts}));
}

TEST(KvModuleFunctionsTest, DelWithStringKey) {
    auto fn = findFunction("kv", "delete");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("key")});
    EXPECT_TRUE(result.isNull());
}

TEST(KvModuleFunctionsTest, DelWithArrayKey) {
    auto fn = findFunction("kv", "delete");
    auto keys = ScriptValue::fromArray({ScriptValue::fromString("k1"), ScriptValue::fromString("k2")});
    EXPECT_NO_THROW(fn({keys}));
}

TEST(KvModuleFunctionsTest, ExistsReturnsFalseWhenNoStore) {
    auto fn = findFunction("kv", "exists");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("key")});
    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(result.asBool());
}

TEST(KvModuleFunctionsTest, ExpireReturnsNullWhenNoStore) {
    auto fn = findFunction("kv", "expire");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("key"), ScriptValue::fromInt(60)});
    EXPECT_TRUE(result.isNull());
}

TEST(KvModuleFunctionsTest, TtlReturnsMinusOneWhenNoStore) {
    auto fn = findFunction("kv", "ttl");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("key")});
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(result.asInt(), -1);
}

TEST(KvModuleFunctionsTest, IncrCreatesKeyWithDefaultDelta) {
    auto fn = findFunction("kv", "incr");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("incr_test_key")});
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(result.asInt(), 1);  // incr on non-existent key creates it with delta=1
}

TEST(KvModuleFunctionsTest, IncrCreatesKeyWithCustomDelta) {
    auto fn = findFunction("kv", "incr");
    auto result = fn({ScriptValue::fromString("incr_test_key2"), ScriptValue::fromInt(5)});
    EXPECT_TRUE(result.isInt());
    EXPECT_EQ(result.asInt(), 5);  // incr on non-existent key creates it with delta=5
}

TEST(KvModuleFunctionsTest, HashOperationsWhenNoStore) {
    auto hsetFn = findFunction("kv", "hset");
    auto hgetFn = findFunction("kv", "hget");
    auto hgetallFn = findFunction("kv", "hgetall");
    auto hdelFn = findFunction("kv", "hdel");

    ASSERT_FALSE(hsetFn.name.empty());

    EXPECT_TRUE(hsetFn({ScriptValue::fromString("h"), ScriptValue::fromString("f"), ScriptValue::fromString("v")}).isNull());
    EXPECT_TRUE(hgetFn({ScriptValue::fromString("h"), ScriptValue::fromString("f")}).isString());
    EXPECT_TRUE(hgetallFn({ScriptValue::fromString("h")}).isObject());
    EXPECT_TRUE(hdelFn({ScriptValue::fromString("h"), ScriptValue::fromString("f")}).isNull());
}

TEST(KvModuleFunctionsTest, HashExistsAndKeys) {
	auto hsetFn = findFunction("kv", "hset");
	auto hexistsFn = findFunction("kv", "hexists");
	auto hkeysFn = findFunction("kv", "hkeys");

	ASSERT_FALSE(hsetFn.name.empty());
	ASSERT_FALSE(hexistsFn.name.empty());
	ASSERT_FALSE(hkeysFn.name.empty());

	const std::string hash = "team:hashops";

	// field absent before set
	EXPECT_FALSE(hexistsFn({ScriptValue::fromString(hash), ScriptValue::fromString("leader")}).asBool());

	// set two fields
	EXPECT_TRUE(hsetFn({ScriptValue::fromString(hash), ScriptValue::fromString("leader"), ScriptValue::fromString("alice")}).isNull());
	EXPECT_TRUE(hsetFn({ScriptValue::fromString(hash), ScriptValue::fromString("count"), ScriptValue::fromString("2")}).isNull());

	// hexists now true for present field, false for missing
	EXPECT_TRUE(hexistsFn({ScriptValue::fromString(hash), ScriptValue::fromString("leader")}).asBool());
	EXPECT_FALSE(hexistsFn({ScriptValue::fromString(hash), ScriptValue::fromString("nope")}).asBool());

	// hkeys returns array of field names
	auto keysResult = hkeysFn({ScriptValue::fromString(hash)});
	ASSERT_TRUE(keysResult.isArray());
	EXPECT_EQ(keysResult.size(), 2u);
}

TEST(KvModuleFunctionsTest, ListOperationsWhenNoStore) {
    auto lpushFn = findFunction("kv", "lpush");
    auto rpushFn = findFunction("kv", "rpush");
    auto lpopFn = findFunction("kv", "lpop");
    auto rpopFn = findFunction("kv", "rpop");
    auto llenFn = findFunction("kv", "llen");
    auto lrangeFn = findFunction("kv", "lrange");

    ASSERT_FALSE(lpushFn.name.empty());

    EXPECT_TRUE(lpushFn({ScriptValue::fromString("list"), ScriptValue::fromString("val")}).isNull());
    EXPECT_TRUE(rpushFn({ScriptValue::fromString("list"), ScriptValue::fromString("val")}).isNull());
    EXPECT_TRUE(lpopFn({ScriptValue::fromString("list")}).isString());
    EXPECT_TRUE(rpopFn({ScriptValue::fromString("list")}).isString());
    EXPECT_EQ(llenFn({ScriptValue::fromString("list")}).asInt(), 0);
    EXPECT_TRUE(lrangeFn({ScriptValue::fromString("list"), ScriptValue::fromInt(0), ScriptValue::fromInt(-1)}).isArray());
}

TEST(KvModuleFunctionsTest, PersistenceSaveLoadRoundTrip) {
	auto setFn = findFunction("kv", "set");
	auto getFn = findFunction("kv", "get");
	auto saveFn = findFunction("kv", "save");
	auto loadFn = findFunction("kv", "load");

	ASSERT_FALSE(saveFn.name.empty());
	ASSERT_FALSE(loadFn.name.empty());

	const std::string path = "wingman_kvtest_roundtrip.db";
	// clean any leftover
	std::remove(path.c_str());

	// seed data and persist
	EXPECT_TRUE(setFn({ScriptValue::fromString("persist:k"), ScriptValue::fromString("v1")}).isNull());
	auto saveResult = saveFn({ScriptValue::fromString(path)});
	EXPECT_TRUE(saveResult.isBool());
	EXPECT_TRUE(saveResult.asBool());

	// load returns bool (file exists)
	auto loadResult = loadFn({ScriptValue::fromString(path)});
	EXPECT_TRUE(loadResult.isBool());
	EXPECT_TRUE(loadResult.asBool());

	// value should still be readable after reload
	EXPECT_EQ(getFn({ScriptValue::fromString("persist:k")}).asString(), "v1");

	// cleanup
	std::remove(path.c_str());
}

TEST(KvModuleFunctionsTest, EnableAutoSaveIsCallable) {
	auto enableFn = findFunction("kv", "enableAutoSave");
	ASSERT_FALSE(enableFn.name.empty());

	// backend returns void; script layer returns nil and must not throw
	const std::string path = "wingman_kvtest_autosave.db";
	std::remove(path.c_str());

	auto result = enableFn({ScriptValue::fromString(path), ScriptValue::fromInt(60)});
	EXPECT_TRUE(result.isNull());

	std::remove(path.c_str());
}

// ========== Security module functions ==========

TEST(SecurityModuleFunctionsTest, GetRandomDelay) {
    auto fn = findFunction("security", "getRandomDelay");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(SecurityModuleFunctionsTest, GetRandomOffset) {
    auto fn = findFunction("security", "getRandomOffset");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 2u);
}

TEST(SecurityModuleFunctionsTest, IsDebuggerPresent) {
    auto fn = findFunction("security", "isDebuggerPresent");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
}

TEST(SecurityModuleFunctionsTest, IsRunningInVM) {
    auto fn = findFunction("security", "isRunningInVM");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
}

TEST(SecurityModuleFunctionsTest, VerifyIntegrity) {
    auto fn = findFunction("security", "verifyIntegrity");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
}

TEST(SecurityModuleFunctionsTest, HashString) {
    auto fn = findFunction("security", "hashString");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 64u);
}

TEST(SecurityModuleFunctionsTest, EncryptDecryptString) {
    auto encFn = findFunction("security", "encryptString");
    auto decFn = findFunction("security", "decryptString");
    ASSERT_FALSE(encFn.name.empty());

    auto encrypted = encFn({ScriptValue::fromString("hello"), ScriptValue::fromString("key")});
    EXPECT_TRUE(encrypted.isString());

    auto decrypted = decFn({encrypted, ScriptValue::fromString("key")});
    EXPECT_TRUE(decrypted.isString());
    EXPECT_EQ(decrypted.asString(), "hello");
}

TEST(SecurityModuleFunctionsTest, GenerateRandomString) {
    auto fn = findFunction("security", "generateRandomString");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(16)});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().size(), 16u);
}

TEST(SecurityModuleFunctionsTest, FilterSensitive) {
    auto fn = findFunction("security", "filterSensitive");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("password=secret")});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString().find("password"), std::string::npos);
}

// ========== Performance module functions (stubs) ==========

TEST(PerformanceModuleFunctionsTest, SetConfigDoesNotCrash) {
    auto fn = findFunction("perf", "setConfig");
    ASSERT_FALSE(fn.name.empty());
    auto config = ScriptValue::fromObject({
        {"enableImageCache", ScriptValue::fromBool(true)},
        {"maxCacheSize", ScriptValue::fromInt(100)},
        {"enableParallelProcessing", ScriptValue::fromBool(false)},
        {"numThreads", ScriptValue::fromInt(4)}
    });
    auto result = fn({config});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, GetConfigReturnsObject) {
    auto fn = findFunction("perf", "getConfig");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, PreloadImageReturnsNull) {
    auto fn = findFunction("perf", "preloadImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, ClearCacheReturnsNull) {
    auto fn = findFunction("perf", "clearCache");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, GetCacheSizeReturnsInt) {
    auto fn = findFunction("perf", "getCacheSize");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(PerformanceModuleFunctionsTest, GetCacheStatsReturnsObject) {
    auto fn = findFunction("perf", "getCacheStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, FastFindImageReturnsNull) {
    auto fn = findFunction("perf", "fastFindImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({
        ScriptValue::fromString("img.png"),
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromFloat(0.9)
    });
    EXPECT_TRUE(result.isNull());
}

TEST(PerformanceModuleFunctionsTest, ParallelFindColorsReturnsArray) {
    auto fn = findFunction("perf", "parallelFindColors");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({
        ScriptValue::fromInt(0xFF0000),
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromInt(10)
    });
    EXPECT_TRUE(result.isArray());
}

TEST(PerformanceModuleFunctionsTest, GetStatsReturnsObject) {
    auto fn = findFunction("perf", "getStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(PerformanceModuleFunctionsTest, ResetStatsReturnsNull) {
    auto fn = findFunction("perf", "resetStats");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

// ========== Debugger module functions (stubs) ==========

TEST(DebuggerModuleFunctionsTest, StartReturnsFalse) {
    auto fn = findFunction("debugger", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
    EXPECT_FALSE(result.asBool());
}

TEST(DebuggerModuleFunctionsTest, StopReturnsNull) {
    auto fn = findFunction("debugger", "stop");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(DebuggerModuleFunctionsTest, BreakpointReturnsString) {
    auto fn = findFunction("debugger", "breakpoint");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("test.lua"), ScriptValue::fromInt(42)});
    EXPECT_TRUE(result.isString());
    EXPECT_NE(result.asString().find("test.lua"), std::string::npos);
    EXPECT_NE(result.asString().find("42"), std::string::npos);
}

TEST(DebuggerModuleFunctionsTest, BreakHereReturnsString) {
    auto fn = findFunction("debugger", "breakHere");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isString());
    EXPECT_EQ(result.asString(), "DEBUG_BREAK_HERE");
}

// ========== Screen module functions (stubs/platform) ==========

TEST(ScreenModuleFunctionsTest, CaptureReturnsBool) {
    auto fn = findFunction("screen", "capture");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isBool());
}

TEST(ScreenModuleFunctionsTest, CaptureRegionWithObject) {
    auto fn = findFunction("screen", "captureRegion");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({region});
    EXPECT_TRUE(result.isBool());
}

TEST(ScreenModuleFunctionsTest, GetPixelReturnsObject) {
    auto fn = findFunction("screen", "getPixel");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0), ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isObject());
}

TEST(ScreenModuleFunctionsTest, FindColorReturnsArray) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({ScriptValue::fromInt(0xFF0000), region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorsReturnsArray) {
    auto fn = findFunction("screen", "findColors");
    ASSERT_FALSE(fn.name.empty());
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({ScriptValue::fromInt(0xFF0000), region, ScriptValue::fromInt(10)});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorWithObjectColor) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(255)},
        {"g", ScriptValue::fromInt(0)},
        {"b", ScriptValue::fromInt(0)}
    });
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({color, region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorsWithObjectColor) {
    auto fn = findFunction("screen", "findColors");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(0)},
        {"g", ScriptValue::fromInt(255)},
        {"b", ScriptValue::fromInt(0)},
        {"a", ScriptValue::fromInt(255)}
    });
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(100)},
        {"height", ScriptValue::fromInt(100)}
    });
    auto result = fn({color, region, ScriptValue::fromInt(10)});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, FindColorWithStringColorFallback) {
    auto fn = findFunction("screen", "findColor");
    ASSERT_FALSE(fn.name.empty());
    // String color triggers toColor fallback (not int, not object)
    auto region = ScriptValue::fromObject({
        {"x", ScriptValue::fromInt(0)},
        {"y", ScriptValue::fromInt(0)},
        {"width", ScriptValue::fromInt(10)},
        {"height", ScriptValue::fromInt(10)}
    });
    auto result = fn({ScriptValue::fromString("red"), region});
    EXPECT_TRUE(result.isArray());
}

TEST(ScreenModuleFunctionsTest, GetScreenWidth) {
    auto fn = findFunction("screen", "getScreenWidth");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(ScreenModuleFunctionsTest, GetScreenHeight) {
    auto fn = findFunction("screen", "getScreenHeight");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(ScreenModuleFunctionsTest, FindImageReturnsArray) {
    auto fn = findFunction("screen", "findImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isArray());
}

// ========== Input module functions (stubs/platform) ==========

TEST(InputModuleFunctionsTest, ClickDoesNotCrash) {
    auto fn = findFunction("input", "click");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(InputModuleFunctionsTest, MoveDoesNotCrash) {
    auto fn = findFunction("input", "move");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(InputModuleFunctionsTest, ScrollDoesNotCrash) {
    auto fn = findFunction("input", "scroll");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}

TEST(InputModuleFunctionsTest, KeyDownUpDoesNotCrash) {
    auto downFn = findFunction("input", "keyDown");
    auto upFn = findFunction("input", "keyUp");
    auto keyFn = findFunction("input", "key");
    ASSERT_FALSE(downFn.name.empty());
    EXPECT_NO_THROW(downFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(upFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(keyFn({ScriptValue::fromInt(65)}));
}

TEST(InputModuleFunctionsTest, TypeDoesNotCrash) {
    auto fn = findFunction("input", "type");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hello")}));
}

TEST(InputModuleFunctionsTest, DelayDoesNotCrash) {
    auto fn = findFunction("input", "delay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1)}));
}

TEST(InputModuleFunctionsTest, RandomDelayDoesNotCrash) {
    auto fn = findFunction("input", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}));
}

TEST(InputModuleFunctionsTest, RandomDelaySwapsWhenMaxLessThanMin) {
    auto fn = findFunction("input", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    // max < min triggers std::swap branch
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}

// ========== Window module functions (stubs/platform) ==========

TEST(WindowModuleFunctionsTest, FindReturnsResults) {
    auto fn = findFunction("window", "find");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow")});
    EXPECT_TRUE(result.isArray() || result.isBool());
}

TEST(WindowModuleFunctionsTest, GetForegroundReturnsInt) {
    auto fn = findFunction("window", "getForeground");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
}

TEST(WindowModuleFunctionsTest, GetTitleReturnsString) {
    auto fn = findFunction("window", "getTitle");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isString());
}

TEST(WindowModuleFunctionsTest, GetBoundsReturnsObject) {
    auto fn = findFunction("window", "getBounds");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isObject());
}

TEST(WindowModuleFunctionsTest, ActivateReturnsBool) {
    auto fn = findFunction("window", "activate");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0)});
    EXPECT_TRUE(result.isBool());
}

TEST(WindowModuleFunctionsTest, WaitForReturnsBool) {
    auto fn = findFunction("window", "waitFor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow"), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}

// ========== Vision module functions (stubs/platform) ==========

TEST(VisionModuleFunctionsTest, FindColorReturnsNull) {
    auto fn = findFunction("vision", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(VisionModuleFunctionsTest, FindColorWithObjectColor) {
    auto fn = findFunction("vision", "findColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(255)},
        {"g", ScriptValue::fromInt(0)},
        {"b", ScriptValue::fromInt(0)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(VisionModuleFunctionsTest, FindAllColorsReturnsArray) {
    auto fn = findFunction("vision", "findAllColors");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isArray());
}

TEST(VisionModuleFunctionsTest, FindAllColorsWithObjectColor) {
    auto fn = findFunction("vision", "findAllColors");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(0)},
        {"g", ScriptValue::fromInt(128)},
        {"b", ScriptValue::fromInt(255)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isArray());
}

TEST(VisionModuleFunctionsTest, HasColorReturnsBool) {
    auto fn = findFunction("vision", "hasColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(0xFF0000)});
    EXPECT_TRUE(result.isBool());
}

TEST(VisionModuleFunctionsTest, HasColorWithObjectColor) {
    auto fn = findFunction("vision", "hasColor");
    ASSERT_FALSE(fn.name.empty());
    auto color = ScriptValue::fromObject({
        {"r", ScriptValue::fromInt(100)},
        {"g", ScriptValue::fromInt(100)},
        {"b", ScriptValue::fromInt(100)}
    });
    auto result = fn({color});
    EXPECT_TRUE(result.isBool());
}

TEST(VisionModuleFunctionsTest, GetDominantColorReturnsObject) {
    auto fn = findFunction("vision", "getDominantColor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(VisionModuleFunctionsTest, FindImageReturnsObject) {
    auto fn = findFunction("vision", "findImage");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.png")});
    EXPECT_TRUE(result.isObject() || result.isNull());
}

// ========== HTTP module functions (stubs) ==========

TEST(HttpModuleFunctionsTest, GetReturnsObject) {
    auto fn = findFunction("http", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, GetWithOptions) {
    auto fn = findFunction("http", "get");
    auto opts = ScriptValue::fromObject({
        {"timeout", ScriptValue::fromInt(1)},
        {"followRedirects", ScriptValue::fromBool(false)},
        {"maxRedirects", ScriptValue::fromInt(0)},
        {"headers", ScriptValue::fromObject({
            {"X-Test", ScriptValue::fromString("value")}
        })}
    });
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), opts});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, PostReturnsObject) {
    auto fn = findFunction("http", "post");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), ScriptValue::fromString("{}")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, PutReturnsObject) {
    auto fn = findFunction("http", "put");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1"), ScriptValue::fromString("{}")});
    EXPECT_TRUE(result.isObject());
}

TEST(HttpModuleFunctionsTest, DelReturnsObject) {
    auto fn = findFunction("http", "delete");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("http://127.0.0.1:1")});
    EXPECT_TRUE(result.isObject());
}

// ========== GameProfile module functions (stubs) ==========

TEST(GameProfileModuleFunctionsTest, ListReturnsArray) {
    auto fn = findFunction("gameprofile", "list");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

TEST(GameProfileModuleFunctionsTest, LoadReturnsBool) {
    auto fn = findFunction("gameprofile", "load");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, SaveReturnsBool) {
    auto fn = findFunction("gameprofile", "save");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, GetReturnsStrings) {
    auto fn = findFunction("gameprofile", "get");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}

TEST(GameProfileModuleFunctionsTest, GetActiveReturnsStrings) {
    auto fn = findFunction("gameprofile", "getActive");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}

TEST(GameProfileModuleFunctionsTest, SetActiveReturnsBool) {
    auto fn = findFunction("gameprofile", "setActive");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_profile")});
    EXPECT_TRUE(result.isBool());
}

TEST(GameProfileModuleFunctionsTest, FindByWindowReturnsStrings) {
    auto fn = findFunction("gameprofile", "findByWindow");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentWindow")});
    EXPECT_TRUE(result.isNull() || result.isString() || result.isArray());
}

// ========== Process module functions (stubs/platform) ==========

TEST(ProcessModuleFunctionsTest, FindReturnsResults) {
    auto fn = findFunction("process", "find");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_process")});
    EXPECT_TRUE(result.isArray() || result.isBool());
}

TEST(ProcessModuleFunctionsTest, StartReturnsInt) {
    auto fn = findFunction("process", "start");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent.exe")});
    EXPECT_TRUE(result.isInt());
}

TEST(ProcessModuleFunctionsTest, ExistsReturnsBool) {
    auto fn = findFunction("process", "exists");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, WaitReturnsBool) {
    auto fn = findFunction("process", "wait");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, TerminateReturnsBool) {
    auto fn = findFunction("process", "terminate");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromInt(99999)});
    EXPECT_TRUE(result.isBool());
}

TEST(ProcessModuleFunctionsTest, WaitForReturnsBool) {
    auto fn = findFunction("process", "waitFor");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("nonexistent_process"), ScriptValue::fromInt(1)});
    EXPECT_TRUE(result.isBool());
}

// ========== Human module functions (stubs/platform) ==========

TEST(HumanModuleFunctionsTest, MouseMoveDoesNotCrash) {
    auto fn = findFunction("human", "mouse_move");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_click");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseRightClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_rightClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseDoubleClickDoesNotCrash) {
    auto fn = findFunction("human", "mouse_doubleClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)}));
}

TEST(HumanModuleFunctionsTest, MouseDragDoesNotCrash) {
    auto fn = findFunction("human", "mouse_drag");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100)
    }));
}

TEST(HumanModuleFunctionsTest, MouseScrollDoesNotCrash) {
    auto fn = findFunction("human", "mouse_scroll");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromInt(1)}));
}

TEST(HumanModuleFunctionsTest, KeyboardPressDoesNotCrash) {
    auto fn = findFunction("human", "keyboard_press");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(65)}));
}

TEST(HumanModuleFunctionsTest, KeyboardDownUpDoesNotCrash) {
    auto downFn = findFunction("human", "keyboard_down");
    auto upFn = findFunction("human", "keyboard_up");
    ASSERT_FALSE(downFn.name.empty());
    EXPECT_NO_THROW(downFn({ScriptValue::fromInt(65)}));
    EXPECT_NO_THROW(upFn({ScriptValue::fromInt(65)}));
}

TEST(HumanModuleFunctionsTest, KeyboardTypeDoesNotCrash) {
    auto fn = findFunction("human", "keyboard_type");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hello")}));
}

// ========== Plan 5: human 高层 API（文档声明） ==========

TEST(HumanModuleFunctionsTest, GetConfigReturnsHighLevelFields) {
    auto fn = findFunction("human", "getConfig");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
    // 文档承诺的 4 个扁平字段（human.md:359）
    EXPECT_NE(result.get("delay_min"), nullptr);
    EXPECT_NE(result.get("delay_max"), nullptr);
    EXPECT_NE(result.get("move_speed"), nullptr);
    EXPECT_NE(result.get("typing_variance"), nullptr);
}

TEST(HumanModuleFunctionsTest, SetConfigUpdatesDelayRangeRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setConfig");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    ASSERT_FALSE(getFn.name.empty());

    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_min"), ScriptValue::fromInt(40)}).isNull());
    EXPECT_TRUE(setFn({ScriptValue::fromString("delay_max"), ScriptValue::fromInt(120)}).isNull());

    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 40);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 120);
}

TEST(HumanModuleFunctionsTest, SetConfigUnknownKeyDoesNotCrash) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto fn = findFunction("human", "setConfig");
    ASSERT_FALSE(fn.name.empty());
    // 未知 key 应静默忽略（脚本层惯例），不抛异常
    EXPECT_NO_THROW(fn({ScriptValue::fromString("nonexistent_key"), ScriptValue::fromInt(1)}));
}

// ========== Util module functions ==========

TEST(UtilModuleFunctionsTest, GetTimeReturnsInt) {
    auto fn = findFunction("util", "getTime");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isInt());
    EXPECT_GT(result.asInt(), 0);
}

TEST(UtilModuleFunctionsTest, SleepDoesNotCrash) {
    auto fn = findFunction("util", "sleep");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(1)}));
}

TEST(UtilModuleFunctionsTest, LogDoesNotCrash) {
    auto fn = findFunction("util", "log");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("test message")}));
}

// ========== Node module functions ==========

TEST(NodeModuleFunctionsTest, CreateHeartbeatReturnsObject) {
    auto fn = findFunction("node", "createHeartbeat");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isObject());
}

TEST(NodeModuleFunctionsTest, SendHeartbeatReturnsNull) {
    auto fn = findFunction("node", "sendHeartbeat");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isNull());
}

TEST(NodeModuleFunctionsTest, GetWindowsReturnsArray) {
    auto fn = findFunction("node", "getWindows");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({});
    EXPECT_TRUE(result.isArray());
}

// ========== UIA module functions ==========

// ========== Module presence verification ==========

TEST(ModulePresenceTest, AllExpectedModulesExist) {
    std::vector<std::string> expectedModules = {
        "json", "system", "orchestration", "team",
        "smarttrigger", "bt", "ocr",
        "screen", "input", "window", "vision",
        "http", "human", "verification", "gameprofile",
        "perf", "config", "kv", "debugger",
        "util", "node", "uia", "process", "security"
    };

    for (const auto& name : expectedModules) {
        EXPECT_TRUE(hasModule(name)) << "Module '" << name << "' not found";
    }
}

TEST(ModulePresenceTest, ModuleFunctionsAreCallable) {
    // This is a conservative smoke test for low-risk functions only. Modules
    // with real I/O, background state, input events, or singleton mutations have
    // dedicated tests and are excluded to avoid cross-test pollution.
    static const std::set<std::string> smokeModules = {
        "debugger",
        "json",
        "node",
        "orchestration",
        "util",
        "verification"
    };

    // Functions that perform blocking I/O (network requests, process waiting)
    // or require specific runtime state and must be skipped in this smoke test.
    static const std::set<std::pair<std::string, std::string>> blockedFunctions = {
        // HTTP — performs network I/O with timeout
        {"http", "get"},
        {"http", "post"},
        {"http", "put"},
        {"http", "delete"},
        // Process — waits/polls for external processes
        {"process", "wait"},
        {"process", "waitFor"},
        {"process", "start"},
        // Window — polls for external window state
        {"window", "waitFor"},
        // Task — waits on condition variable with timeout
        {"task", "wait"},
        {"task", "submit"},
        {"task", "retry"},
        // Notify — webhook performs network I/O
        {"notify", "webhook"},
        {"notify", "bridge"},
        // Event — registers persistent callbacks
        {"event", "on"},
        {"event", "once"},
        // Config/game profile — mutate repository files or singleton profile state
        {"config", "save"},
        {"config", "set"},
        {"config", "remove"},
        {"gameprofile", "save"},
        {"gameprofile", "setActive"},
        {"gameprofile", "setProfilesDirectory"},
        {"gameprofile", "scan"},
        {"gameprofile", "createTemplate"},
        {"gameprofile", "importJson"},
        {"gameprofile", "exportPackage"},
        {"gameprofile", "importPackage"},
        {"gameprofile", "delete"},
    };

    auto modules = getAllModules();
    for (const auto& mod : modules) {
        if (!smokeModules.count(mod.name)) continue;
        for (const auto& fn : mod.functions) {
            if (blockedFunctions.count({mod.name, fn.name})) continue;
            std::vector<ScriptValue> args;
            // Build minimal args from signature to avoid UB on empty vector access.
            // Count parameters by counting commas before "->", then provide that many defaults.
            const auto& sig = fn.signature;
            auto arrowPos = sig.find(" -> ");
            std::string paramsPart = (arrowPos != std::string::npos) ? sig.substr(0, arrowPos) : sig;
            bool hasParams = !paramsPart.empty() && paramsPart != "()";
            if (hasParams) {
                // Count commas to determine parameter count
                int paramCount = 1;
                for (char c : paramsPart) {
                    if (c == ',') paramCount++;
                }
                // Determine first param type for the first arg
                auto colonPos = paramsPart.find(':');
                if (colonPos != std::string::npos && paramsPart.substr(colonPos, 7) == ":string") {
                    args.push_back(ScriptValue::fromString("test"));
                } else {
                    args.push_back(ScriptValue::fromInt(1));
                }
                // Fill remaining args with int defaults
                for (int i = 1; i < paramCount; ++i) {
                    args.push_back(ScriptValue::fromInt(1));
                }
            }
            EXPECT_NO_THROW(fn.func(args))
                << "Module '" << mod.name << "' function '" << fn.name << "' threw";
        }
    }
}

TEST(HumanModuleFunctionsTest, SetDelayRangeRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setDelayRange");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromInt(50), ScriptValue::fromInt(200)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_EQ(cfg.get("delay_min")->asInt(), 50);
    EXPECT_EQ(cfg.get("delay_max")->asInt(), 200);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setMoveSpeed");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(1.5)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("move_speed")->asFloat(), 1.5, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetTypingVarianceRoundTrip) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    auto setFn = findFunction("human", "setTypingVariance");
    auto getFn = findFunction("human", "getConfig");
    ASSERT_FALSE(setFn.name.empty());
    EXPECT_TRUE(setFn({ScriptValue::fromFloat(0.3)}).isNull());
    auto cfg = getFn({});
    ASSERT_TRUE(cfg.isObject());
    EXPECT_NEAR(cfg.get("typing_variance")->asFloat(), 0.3, 1e-9);
}

TEST(HumanModuleFunctionsTest, SetMoveSpeedAffectsBackendMoveDuration) {
    HumanConfigGuard hcg; // 测试结束恢复全局 Human 配置
    // 验证映射应用：move_speed=2.0 -> minMoveDuration=50, maxMoveDuration=150
    auto setFn = findFunction("human", "setMoveSpeed");
    ASSERT_FALSE(setFn.name.empty());
    setFn({ScriptValue::fromFloat(2.0)});
    auto mc = Human::mouse().getConfig();
    EXPECT_EQ(mc.minMoveDuration, 50); // 100/2
    EXPECT_EQ(mc.maxMoveDuration, 150); // 300/2
}

TEST(HumanModuleFunctionsTest, RandomDelayExplicitRangeDoesNotCrash) {
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    // 显式传 0-1ms，避免测试耗时（默认 100-300ms 会真实 sleep）
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(0), ScriptValue::fromInt(1)}));
}

TEST(HumanModuleFunctionsTest, RandomDelayNoArgsUsesDefaultRange) {
    // 无参调用走默认 100-300ms 路径（仅验证可调用，接受单次 ~300ms sleep）
    auto fn = findFunction("human", "randomDelay");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({}));
}

TEST(HumanModuleFunctionsTest, NaturalTypeDoesNotCrash) {
    auto fn = findFunction("human", "naturalType");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromString("hi")}));
}

TEST(HumanModuleFunctionsTest, MoveMouseWithDefaultDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(50), ScriptValue::fromInt(50)
    }));
}

TEST(HumanModuleFunctionsTest, MoveMouseExplicitDurationDoesNotCrash) {
    auto fn = findFunction("human", "moveMouse");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({
        ScriptValue::fromInt(0), ScriptValue::fromInt(0),
        ScriptValue::fromInt(100), ScriptValue::fromInt(100),
        ScriptValue::fromInt(1) // 1ms，避免测试耗时
    }));
}

TEST(HumanModuleFunctionsTest, NaturalClickLeftDefaultDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100)})); // 默认 left
}

TEST(HumanModuleFunctionsTest, NaturalClickRightButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("right")}));
}

TEST(HumanModuleFunctionsTest, NaturalClickMiddleButtonDoesNotCrash) {
    auto fn = findFunction("human", "naturalClick");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_NO_THROW(fn({ScriptValue::fromInt(100), ScriptValue::fromInt(100), ScriptValue::fromString("middle")}));
}

// ========== Plan 7: bt 节点构造脚本桥接 ==========

TEST(BtModuleFunctionsTest, SequenceReturnsPositiveHandle) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("seq1")});
    ASSERT_TRUE(h.isInt());
    EXPECT_GT(h.asInt(), 0); // 有效句柄 ≥ 1
}

TEST(BtModuleFunctionsTest, SelectorReturnsPositiveHandle) {
    auto fn = findFunction("bt", "selector");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromString("sel1")}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SequenceDefaultNameDoesNotCrash) {
    auto fn = findFunction("bt", "sequence");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认名
}

TEST(BtModuleFunctionsTest, ParallelWithPolicyStringReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    auto h = fn({ScriptValue::fromString("par"), ScriptValue::fromString("FAIL_ON_ONE")});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ParallelDefaultPolicyReturnsHandle) {
    auto fn = findFunction("bt", "parallel");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({}).asInt(), 0); // 无参 → 默认 SUCCEED_ON_ALL
}

TEST(BtModuleFunctionsTest, WaitReturnsPositiveHandle) {
    auto fn = findFunction("bt", "wait");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_GT(fn({ScriptValue::fromInt(100)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterWithChildReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto invFn = findFunction("bt", "inverter");
    ASSERT_FALSE(invFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(50)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(invFn({ScriptValue::fromInt(child)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, InverterInvalidChildReturnsZero) {
    auto fn = findFunction("bt", "inverter");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromInt(999999)}).asInt(), 0); // 无效 child handle → 0
}

TEST(BtModuleFunctionsTest, RepeatWithCountReturnsHandle) {
    auto waitFn = findFunction("bt", "wait");
    auto repFn = findFunction("bt", "repeat");
    ASSERT_FALSE(repFn.name.empty());
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_GT(child, 0);
    EXPECT_GT(repFn({ScriptValue::fromInt(child), ScriptValue::fromInt(3)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, AddChildToSequenceReturnsTrue) {
    auto seqFn = findFunction("bt", "sequence");
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    ASSERT_FALSE(addFn.name.empty());
    int64_t parent = seqFn({}).asInt();
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    ASSERT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToParallelReturnsTrue) {
    auto parFn = findFunction("bt", "parallel");
    auto seqFn = findFunction("bt", "sequence");
    auto addFn = findFunction("bt", "addChild");
    int64_t parent = parFn({}).asInt();
    int64_t child = seqFn({}).asInt(); // 复合节点也可作 child
    EXPECT_TRUE(addFn({ScriptValue::fromInt(parent), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildInvalidParentReturnsFalse) {
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t child = waitFn({ScriptValue::fromInt(10)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(999999), ScriptValue::fromInt(child)}).asBool());
}

TEST(BtModuleFunctionsTest, AddChildToLeafReturnsFalse) {
    // 叶子/装饰节点不支持 addChild（仅复合节点 Sequence/Selector/Parallel 支持）
    auto waitFn = findFunction("bt", "wait");
    auto addFn = findFunction("bt", "addChild");
    int64_t leaf = waitFn({ScriptValue::fromInt(10)}).asInt();
    int64_t another = waitFn({ScriptValue::fromInt(20)}).asInt();
    EXPECT_FALSE(addFn({ScriptValue::fromInt(leaf), ScriptValue::fromInt(another)}).asBool());
}

TEST(BtModuleFunctionsTest, ConditionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // callable 返回 bool true（模拟脚本条件函数）
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true);
    });
    auto h = fn({ScriptValue::fromString("hp_low"), cb});
    EXPECT_GT(h.asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    // 第二参非 callable → 返回 0
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(123)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ConditionMissingCallableReturnsZero) {
    auto fn = findFunction("bt", "condition");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("nocb")}).asInt(), 0); // 缺 callable
}

TEST(BtModuleFunctionsTest, ActionWithCallableReturnsHandle) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    ScriptValue cb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    EXPECT_GT(fn({ScriptValue::fromString("attack"), cb}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, ActionInvalidCallableReturnsZero) {
    auto fn = findFunction("bt", "action");
    ASSERT_FALSE(fn.name.empty());
    EXPECT_EQ(fn({ScriptValue::fromString("bad"), ScriptValue::fromInt(1)}).asInt(), 0);
}

TEST(BtModuleFunctionsTest, SetRootOnExistingTreeReturnsTrue) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    ASSERT_FALSE(setRootFn.name.empty());
    std::string tree = "plan7_setroot_test";
    createFn({ScriptValue::fromString(tree)});
    int64_t node = seqFn({}).asInt();
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(node)}).asBool());
    findFunction("bt", "remove")({ScriptValue::fromString(tree)});
}

TEST(BtModuleFunctionsTest, SetRootInvalidTreeReturnsFalse) {
    auto seqFn = findFunction("bt", "sequence");
    auto setRootFn = findFunction("bt", "setRoot");
    int64_t node = seqFn({}).asInt();
    EXPECT_FALSE(setRootFn({ScriptValue::fromString("no_such_tree_xyz"), ScriptValue::fromInt(node)}).asBool());
}

// 端到端：condition(true) + action(SUCCESS) 组装成 sequence，setRoot，tick → SUCCESS
TEST(BtModuleFunctionsTest, EndToEndConditionActionTickReturnsSuccess) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_test";
    ASSERT_TRUE(createFn({ScriptValue::fromString(tree)}).asBool());

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(true);
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    ASSERT_GT(cond, 0); ASSERT_GT(act, 0); ASSERT_GT(seq, 0);

    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)}).asBool());
    ASSERT_TRUE(addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)}).asBool());
    ASSERT_TRUE(setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)}).asBool());

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "SUCCESS");
    removeFn({ScriptValue::fromString(tree)});
}

// 端到端：condition(false) → sequence 失败 → tick → FAILURE
TEST(BtModuleFunctionsTest, EndToEndFalseConditionTickReturnsFailure) {
    auto createFn = findFunction("bt", "create");
    auto seqFn = findFunction("bt", "sequence");
    auto condFn = findFunction("bt", "condition");
    auto actFn = findFunction("bt", "action");
    auto addFn = findFunction("bt", "addChild");
    auto setRootFn = findFunction("bt", "setRoot");
    auto tickFn = findFunction("bt", "tick");
    auto removeFn = findFunction("bt", "remove");

    std::string tree = "plan7_e2e_fail_test";
    createFn({ScriptValue::fromString(tree)});

    ScriptValue condCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromBool(false);
    });
    ScriptValue actCb = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        return ScriptValue::fromString("SUCCESS");
    });
    int64_t cond = condFn({ScriptValue::fromString("ready"), condCb}).asInt();
    int64_t act = actFn({ScriptValue::fromString("go"), actCb}).asInt();
    int64_t seq = seqFn({}).asInt();
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(cond)});
    addFn({ScriptValue::fromInt(seq), ScriptValue::fromInt(act)});
    setRootFn({ScriptValue::fromString(tree), ScriptValue::fromInt(seq)});

    EXPECT_EQ(tickFn({ScriptValue::fromString(tree)}).asString(), "FAILURE");
    removeFn({ScriptValue::fromString(tree)});
}

// ========== Plan 6: UiaModuleFunctionsTest ==========

TEST(UiaModuleFunctionsTest, NonEventFunctionsRegistered) {
	// 10 个非事件函数全部注册
	EXPECT_FALSE(findFunction("uia", "from_foreground").name.empty());
	EXPECT_FALSE(findFunction("uia", "from_point").name.empty());
	EXPECT_FALSE(findFunction("uia", "from_window").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_by_name").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_by_id").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_all_by_control_type").name.empty());
	EXPECT_FALSE(findFunction("uia", "wait_for_name").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_button").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_edit").name.empty());
	EXPECT_FALSE(findFunction("uia", "find_text").name.empty());
}

TEST(UiaModuleFunctionsTest, FindByNameReturnsNullOrObject) {
	auto fn = findFunction("uia", "find_by_name");
	auto result = fn.func({ScriptValue::fromString("nonexistent_element")});
	// 无辅助功能权限：null；有权限：Object 或 null（找不到）
	EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(UiaModuleFunctionsTest, FromForegroundReturnsNullOrObject) {
	auto fn = findFunction("uia", "from_foreground");
	auto result = fn.func({});
	EXPECT_TRUE(result.isNull() || result.isObject());
}

TEST(UiaModuleFunctionsTest, FindAllByControlTypeReturnsArray) {
	auto fn = findFunction("uia", "find_all_by_control_type");
	auto result = fn.func({ScriptValue::fromInt(static_cast<int64_t>(UIARole::Button))});
	EXPECT_TRUE(result.isArray());
	// 数组元素（若有）应为 Object 或 null
	for (size_t i = 0; i < result.size(); ++i) {
		EXPECT_TRUE(result.at(i).isNull() || result.at(i).isObject());
	}
}

TEST(UiaModuleFunctionsTest, WaitForNameShortTimeoutNoCrash) {
	auto fn = findFunction("uia", "wait_for_name");
	EXPECT_NO_THROW(fn.func({ScriptValue::fromString("x"), ScriptValue::fromInt(50)}));
}

TEST(UiaModuleFunctionsTest, FindButtonAndEditReturnNullOrObject) {
	auto fnBtn = findFunction("uia", "find_button");
	auto r1 = fnBtn.func({ScriptValue::fromString("OK")});
	EXPECT_TRUE(r1.isNull() || r1.isObject());
	auto fnEdit = findFunction("uia", "find_edit");
	auto r2 = fnEdit.func({ScriptValue::fromString("username")});
	EXPECT_TRUE(r2.isNull() || r2.isObject());
}

TEST(UiaModuleFunctionsTest, UIElementObjectStructureWhenAvailable) {
	// 有辅助功能权限时验证 OO 对象结构；无权限则跳过（不失败）
	if (!uia().initialize()) {
		SUCCEED() << "无辅助功能权限，跳过 OO 结构验证";
		return;
	}
	auto fn = findFunction("uia", "from_foreground");
	auto root = fn.func({});
	if (root.isNull()) {
		SUCCEED() << "无前台元素，跳过";
		return;
	}
	ASSERT_TRUE(root.isObject());
	ASSERT_TRUE(root.get("_handle") != nullptr);
	EXPECT_TRUE(root.get("_handle")->isInt());
	EXPECT_GT(root.get("_handle")->asInt(0), 0); // handle >= 1
	// 12 方法均为 Callable
	const std::vector<std::string> methods = {
		"get_info", "click", "double_click", "focus", "get_value", "set_value",
		"get_children", "expand", "collapse", "is_expanded", "is_visible", "is_enabled"
	};
	for (const auto& m : methods) {
		const ScriptValue* method = root.get(m);
		ASSERT_TRUE(method != nullptr) << "缺少方法: " << m;
		EXPECT_TRUE(method->isCallable()) << "方法非 Callable: " << m;
	}
	// 调用 get_info 验证返回 Object（或 null，若元素失效）
	auto info = root.get("get_info")->call({});
	EXPECT_TRUE(info.isObject() || info.isNull());
}
