#include "script_function_test_helpers.hpp"

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


