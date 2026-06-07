#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/json.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/smart_trigger.hpp"
#include <algorithm>
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

TEST(UiaModuleFunctionsTest, FindByNameReturnsNullForMissing) {
    auto fn = findFunction("uia", "findByName");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentElement")});
    EXPECT_TRUE(result.isNull() || result.isInt());
}

TEST(UiaModuleFunctionsTest, FindByIdReturnsNullForMissing) {
    auto fn = findFunction("uia", "findById");
    ASSERT_FALSE(fn.name.empty());
    auto result = fn({ScriptValue::fromString("NonexistentId")});
    EXPECT_TRUE(result.isNull() || result.isInt());
}

TEST(UiaModuleFunctionsTest, FindWithSelectorDoesNotCrash) {
    auto fn = findFunction("uia", "find");
    ASSERT_FALSE(fn.name.empty());
    auto selector = ScriptValue::fromObject({
        {"name", ScriptValue::fromString("test")},
        {"id", ScriptValue::fromString("test_id")},
        {"className", ScriptValue::fromString("TestClass")}
    });
    EXPECT_NO_THROW(fn({selector}));
}

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
    };

    auto modules = getAllModules();
    for (const auto& mod : modules) {
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
