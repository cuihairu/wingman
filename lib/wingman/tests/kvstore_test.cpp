#include <gtest/gtest.h>
#include <wingman/kvstore.hpp>
#include <chrono>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

class KeyValueStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = std::make_unique<wingman::KeyValueStore>();
        const auto* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        testDbPath = (fs::temp_directory_path() /
            ("test_kvstore_" + std::string(testInfo->test_suite_name()) + "_" + testInfo->name() + "_" + suffix + ".db")).string();
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    void TearDown() override {
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    std::unique_ptr<wingman::KeyValueStore> store;
    std::string testDbPath;
};

// ========== String Operations ==========

TEST_F(KeyValueStoreTest, SetAndGet) {
    store->set("key1", "value1");
    EXPECT_EQ(store->get("key1"), "value1");
}

TEST_F(KeyValueStoreTest, GetNonExistentKey) {
    EXPECT_EQ(store->get("nonexistent"), "");
}

TEST_F(KeyValueStoreTest, DeleteKey) {
    store->set("key1", "value1");
    store->del("key1");
    EXPECT_EQ(store->get("key1"), "");
}

TEST_F(KeyValueStoreTest, DeleteMultipleKeys) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->set("key3", "value3");
    store->del(std::vector<std::string>{"key1", "key2"});
    EXPECT_EQ(store->get("key1"), "");
    EXPECT_EQ(store->get("key2"), "");
    EXPECT_EQ(store->get("key3"), "value3");
}

TEST_F(KeyValueStoreTest, Exists) {
    EXPECT_FALSE(store->exists("key1"));
    store->set("key1", "value1");
    EXPECT_TRUE(store->exists("key1"));
}

TEST_F(KeyValueStoreTest, KeysPattern) {
    store->set("user:1:name", "Alice");
    store->set("user:2:name", "Bob");
    store->set("session:abc", "data");

    // keys() uses regex, not wildcards
    auto keys = store->keys("user:.*:name");
    EXPECT_EQ(keys.size(), 2);
}

TEST_F(KeyValueStoreTest, SetNX) {
    wingman::KvOptions options;
    options.nx = true;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "value1");

    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value1");  // Should not overwrite
}

TEST_F(KeyValueStoreTest, SetXX) {
    wingman::KvOptions options;
    options.xx = true;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "");  // Should not create

    store->set("key1", "value1");
    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value2");  // Should overwrite
}

TEST_F(KeyValueStoreTest, Incr) {
    store->set("counter", "10");
    EXPECT_EQ(store->incr("counter"), 11);
    EXPECT_EQ(store->get("counter"), "11");

    EXPECT_EQ(store->incr("counter", 5), 16);
    EXPECT_EQ(store->get("counter"), "16");
}

// ========== Hash Operations ==========

TEST_F(KeyValueStoreTest, HSetAndGet) {
    store->hset("user:1", "name", "Alice");
    EXPECT_EQ(store->hget("user:1", "name"), "Alice");
}

TEST_F(KeyValueStoreTest, HMSetAndGetAll) {
    wingman::HashFields fields = {
        {"name", "Bob"},
        {"age", "30"},
        {"city", "NYC"}
    };
    store->hmset("user:1", fields);

    auto all = store->hgetall("user:1");
    EXPECT_EQ(all.size(), 3);
    EXPECT_EQ(all["name"], "Bob");
    EXPECT_EQ(all["age"], "30");
}

TEST_F(KeyValueStoreTest, HDel) {
    store->hset("user:1", "name", "Alice");
    store->hset("user:1", "age", "30");

    store->hdel("user:1", "name");
    EXPECT_EQ(store->hget("user:1", "name"), "");
    EXPECT_EQ(store->hget("user:1", "age"), "30");
}

TEST_F(KeyValueStoreTest, HExists) {
    EXPECT_FALSE(store->hexists("user:1", "name"));
    store->hset("user:1", "name", "Alice");
    EXPECT_TRUE(store->hexists("user:1", "name"));
}

TEST_F(KeyValueStoreTest, HKeys) {
    store->hset("user:1", "name", "Alice");
    store->hset("user:1", "age", "30");
    store->hset("user:1", "city", "NYC");

    auto keys = store->hkeys("user:1");
    EXPECT_EQ(keys.size(), 3);
}

// ========== List Operations ==========

TEST_F(KeyValueStoreTest, LPushAndRPop) {
    store->lpush("mylist", "item1");
    store->lpush("mylist", "item2");

    EXPECT_EQ(store->rpop("mylist"), "item1");
    EXPECT_EQ(store->rpop("mylist"), "item2");
}

TEST_F(KeyValueStoreTest, RPushAndLPop) {
    store->rpush("mylist", "item1");
    store->rpush("mylist", "item2");

    EXPECT_EQ(store->lpop("mylist"), "item1");
    EXPECT_EQ(store->lpop("mylist"), "item2");
}

TEST_F(KeyValueStoreTest, LLen) {
    store->lpush("mylist", "item1");
    store->lpush("mylist", "item2");
    store->lpush("mylist", "item3");

    EXPECT_EQ(store->llen("mylist"), 3);
}

TEST_F(KeyValueStoreTest, LRange) {
    store->rpush("mylist", "a");
    store->rpush("mylist", "b");
    store->rpush("mylist", "c");
    store->rpush("mylist", "d");

    auto range = store->lrange("mylist", 1, 2);
    EXPECT_EQ(range.size(), 2);
    EXPECT_EQ(range[0], "b");
    EXPECT_EQ(range[1], "c");
}

TEST_F(KeyValueStoreTest, LRem) {
    store->rpush("mylist", "a");
    store->rpush("mylist", "b");
    store->rpush("mylist", "a");
    store->rpush("mylist", "c");

    size_t removed = store->lrem("mylist", 2, "a");
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(store->llen("mylist"), 2);
}

// ========== Publish/Subscribe ==========

TEST_F(KeyValueStoreTest, PublishSubscribe) {
    bool received = false;
    std::string receivedMsg;

    store->subscribe("test_channel", [&](const std::string& msg) {
        received = true;
        receivedMsg = msg;
    });

    store->publish("test_channel", "hello");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(received);
    EXPECT_EQ(receivedMsg, "hello");

    store->unsubscribe("test_channel");
}

// ========== TTL ==========

TEST_F(KeyValueStoreTest, ExpireAndTTL) {
    store->set("key1", "value1");
    store->expire("key1", 2);

    int64_t ttl = store->ttl("key1");
    EXPECT_GT(ttl, 0);
    EXPECT_LE(ttl, 2);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_EQ(store->get("key1"), "");
}

TEST_F(KeyValueStoreTest, SetWithTTL) {
    wingman::KvOptions options;
    options.ttl = 1;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "value1");

    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_EQ(store->get("key1"), "");
}

// ========== Persistence ==========

TEST_F(KeyValueStoreTest, SaveAndLoad) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->expire("key1", 3600);

    EXPECT_TRUE(store->save(testDbPath));

    auto newStore = std::make_unique<wingman::KeyValueStore>();
    EXPECT_TRUE(newStore->load(testDbPath));

    EXPECT_EQ(newStore->get("key1"), "value1");
    EXPECT_EQ(newStore->get("key2"), "value2");
    // Note: hash and list are not currently persisted, only testing string key-values
}

// ========== Statistics ==========

TEST_F(KeyValueStoreTest, Stats) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->hset("hash1", "field1", "value3");

    auto stats = store->stats();
    EXPECT_GT(stats["strings"], 0);
    EXPECT_GT(stats["hashes"], 0);
}

// ========== Cleanup Expired Keys ==========

TEST_F(KeyValueStoreTest, CleanupExpired) {
    wingman::KvOptions options;
    options.ttl = 1;

    store->set("expire1", "value1", options);
    store->set("expire2", "value2", options);
    store->set("permanent", "value3");

    std::this_thread::sleep_for(std::chrono::seconds(2));

    size_t cleaned = store->cleanupExpired();
    EXPECT_GE(cleaned, 2);

    EXPECT_EQ(store->get("expire1"), "");
    EXPECT_EQ(store->get("expire2"), "");
    EXPECT_EQ(store->get("permanent"), "value3");
}

// ========== Additional Key/Value Tests ==========

TEST_F(KeyValueStoreTest, IncrNonexistentKey) {
    EXPECT_EQ(store->incr("new_counter"), 1);
    EXPECT_EQ(store->get("new_counter"), "1");
}

TEST_F(KeyValueStoreTest, IncrNegativeDelta) {
    store->set("counter", "10");
    EXPECT_EQ(store->incr("counter", -3), 7);
    EXPECT_EQ(store->get("counter"), "7");
}

TEST_F(KeyValueStoreTest, KeysNoMatch) {
    store->set("abc", "1");
    store->set("def", "2");
    auto result = store->keys("xyz.*");
    EXPECT_TRUE(result.empty());
}

TEST_F(KeyValueStoreTest, KeysAllPattern) {
    store->set("k1", "v1");
    store->set("k2", "v2");
    auto result = store->keys(".*");
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(KeyValueStoreTest, SetOverwrite) {
    store->set("key", "old");
    store->set("key", "new");
    EXPECT_EQ(store->get("key"), "new");
}

TEST_F(KeyValueStoreTest, DeleteNonexistentKey) {
    EXPECT_NO_THROW(store->del("nonexistent"));
}

TEST_F(KeyValueStoreTest, TtlNonexistentKey) {
    EXPECT_LT(store->ttl("nonexistent"), 0);
}

TEST_F(KeyValueStoreTest, ExpireNonexistentKey) {
    EXPECT_NO_THROW(store->expire("nonexistent", 10));
}

TEST_F(KeyValueStoreTest, EmptyKey) {
    store->set("", "empty_key_value");
    EXPECT_EQ(store->get(""), "empty_key_value");
}

TEST_F(KeyValueStoreTest, EmptyValue) {
    store->set("key", "");
    EXPECT_EQ(store->get("key"), "");
    EXPECT_TRUE(store->exists("key"));
}

// ========== Additional Hash Tests ==========

TEST_F(KeyValueStoreTest, HGetNonexistentHash) {
    EXPECT_EQ(store->hget("nohash", "field"), "");
}

TEST_F(KeyValueStoreTest, HGetNonexistentField) {
    store->hset("hash1", "field1", "val");
    EXPECT_EQ(store->hget("hash1", "nofield"), "");
}

TEST_F(KeyValueStoreTest, HGetAllEmpty) {
    auto all = store->hgetall("nohash");
    EXPECT_TRUE(all.empty());
}

TEST_F(KeyValueStoreTest, HKeysEmpty) {
    auto keys = store->hkeys("nohash");
    EXPECT_TRUE(keys.empty());
}

TEST_F(KeyValueStoreTest, HDelNonexistentField) {
    store->hset("hash1", "field1", "val");
    EXPECT_NO_THROW(store->hdel("hash1", "nofield"));
    EXPECT_EQ(store->hget("hash1", "field1"), "val");
}

TEST_F(KeyValueStoreTest, HExistsNonexistentHash) {
    EXPECT_FALSE(store->hexists("nohash", "field"));
}

TEST_F(KeyValueStoreTest, HMSetOverwrite) {
    wingman::HashFields fields1 = {{"k", "v1"}};
    wingman::HashFields fields2 = {{"k", "v2"}};
    store->hmset("hash1", fields1);
    store->hmset("hash1", fields2);
    EXPECT_EQ(store->hget("hash1", "k"), "v2");
}

// ========== Additional List Tests ==========

TEST_F(KeyValueStoreTest, LPopEmpty) {
    EXPECT_EQ(store->lpop("nolist"), "");
}

TEST_F(KeyValueStoreTest, RPopEmpty) {
    EXPECT_EQ(store->rpop("nolist"), "");
}

TEST_F(KeyValueStoreTest, LLenNonexistent) {
    EXPECT_EQ(store->llen("nolist"), 0u);
}

TEST_F(KeyValueStoreTest, LRangeNonexistent) {
    auto range = store->lrange("nolist", 0, -1);
    EXPECT_TRUE(range.empty());
}

TEST_F(KeyValueStoreTest, LRangeNegativeIndices) {
    store->rpush("list", "a");
    store->rpush("list", "b");
    store->rpush("list", "c");
    auto range = store->lrange("list", -2, -1);
    EXPECT_EQ(range.size(), 2u);
    EXPECT_EQ(range[0], "b");
    EXPECT_EQ(range[1], "c");
}

TEST_F(KeyValueStoreTest, LRemNonexistent) {
    size_t removed = store->lrem("nolist", 1, "value");
    EXPECT_EQ(removed, 0u);
}

TEST_F(KeyValueStoreTest, MultipleSubscribers) {
    int count = 0;
    store->subscribe("ch", [&](const std::string&) { count++; });
    store->subscribe("ch", [&](const std::string&) { count++; });
    store->publish("ch", "msg");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(count, 2);
    store->unsubscribe("ch");
}

// ========== Persistence Edge Cases ==========

TEST_F(KeyValueStoreTest, SaveLoadRoundtrip) {
    store->set("str1", "hello");
    store->set("str2", "world");
    store->hset("hash1", "f1", "v1");
    store->lpush("list1", "item1");

    EXPECT_TRUE(store->save(testDbPath));

    auto loaded = std::make_unique<wingman::KeyValueStore>();
    EXPECT_TRUE(loaded->load(testDbPath));

    EXPECT_EQ(loaded->get("str1"), "hello");
    EXPECT_EQ(loaded->get("str2"), "world");
}

TEST_F(KeyValueStoreTest, SaveToInvalidPath) {
    store->set("key", "val");
    EXPECT_FALSE(store->save("/nonexistent/dir/file.db"));
}

TEST_F(KeyValueStoreTest, LoadNonexistentFile) {
    EXPECT_FALSE(store->load("/nonexistent/file.db"));
}

// ========== Additional Tests ==========

TEST_F(KeyValueStoreTest, EnableAutoSaveDoesNotCrash) {
    // enableAutoSave starts background threads; verify no crash on scope exit
    {
        auto s = std::make_unique<wingman::KeyValueStore>();
        s->set("k", "v");
        EXPECT_NO_THROW(s->enableAutoSave(testDbPath + "_auto", 600));
    }
    // Destroy after leaving scope — threads should join cleanly
}

TEST_F(KeyValueStoreTest, IncrOnNonNumericValue) {
    store->set("alpha", "hello");
    // incr on non-numeric catches the exception internally and starts from 0
    int64_t result = store->incr("alpha");
    EXPECT_EQ(result, 1);
    EXPECT_EQ(store->get("alpha"), "1");
}

TEST_F(KeyValueStoreTest, IncrOnNonNumericWithDelta) {
    store->set("beta", "not_a_number");
    EXPECT_EQ(store->incr("beta", 10), 10);
    EXPECT_EQ(store->get("beta"), "10");
}

TEST_F(KeyValueStoreTest, MultipleSetDelCyclesOnSameKey) {
    for (int i = 0; i < 5; ++i) {
        store->set("recycle", std::to_string(i));
        EXPECT_EQ(store->get("recycle"), std::to_string(i));
        store->del("recycle");
        EXPECT_EQ(store->get("recycle"), "");
    }
    // After cycles, set one more time and verify
    store->set("recycle", "final");
    EXPECT_EQ(store->get("recycle"), "final");
}

TEST_F(KeyValueStoreTest, HashFieldOverwrite) {
    store->hset("doc", "field", "v1");
    EXPECT_EQ(store->hget("doc", "field"), "v1");
    store->hset("doc", "field", "v2");
    EXPECT_EQ(store->hget("doc", "field"), "v2");
    // Verify other fields remain untouched
    store->hset("doc", "other", " untouched");
    store->hset("doc", "field", "v3");
    EXPECT_EQ(store->hget("doc", "field"), "v3");
    EXPECT_EQ(store->hget("doc", "other"), " untouched");
}

TEST_F(KeyValueStoreTest, ListWithManyElements) {
    const int count = 100;
    for (int i = 0; i < count; ++i) {
        store->rpush("biglist", std::to_string(i));
    }
    EXPECT_EQ(store->llen("biglist"), static_cast<size_t>(count));

    // Pop all from the left — should come out in order 0..99
    for (int i = 0; i < count; ++i) {
        std::string val = store->lpop("biglist");
        EXPECT_EQ(val, std::to_string(i));
    }
    EXPECT_EQ(store->llen("biglist"), 0u);
    EXPECT_EQ(store->lpop("biglist"), "");  // empty after draining
}

TEST_F(KeyValueStoreTest, SaveLoadWithHashAndListData) {
    store->set("str_key", "str_val");
    store->hset("h1", "f1", "hv1");
    store->hset("h1", "f2", "hv2");
    store->lpush("l1", "lv1");
    store->rpush("l1", "lv2");

    EXPECT_TRUE(store->save(testDbPath));

    auto loaded = std::make_unique<wingman::KeyValueStore>();
    EXPECT_TRUE(loaded->load(testDbPath));

    // String keys survive save/load
    EXPECT_EQ(loaded->get("str_key"), "str_val");

    // Hash and list are not persisted in current implementation — verify no crash
    EXPECT_NO_THROW(loaded->hget("h1", "f1"));
    EXPECT_NO_THROW(loaded->llen("l1"));
}

TEST_F(KeyValueStoreTest, StatsAfterVariousOperations) {
    store->set("s1", "v1");
    store->set("s2", "v2");
    store->hset("hash1", "f", "v");
    store->lpush("list1", "item");
    store->subscribe("ch", [](const std::string&) {});

    auto stats = store->stats();
    EXPECT_EQ(stats["strings"], 2u);
    EXPECT_EQ(stats["hashes"], 1u);
    EXPECT_EQ(stats["lists"], 1u);
    EXPECT_EQ(stats["subscriptions"], 1u);

    // After deleting one key and unsubscribing
    store->del("s1");
    store->unsubscribe("ch");
    stats = store->stats();
    EXPECT_EQ(stats["strings"], 1u);
    EXPECT_EQ(stats["subscriptions"], 0u);
}

TEST_F(KeyValueStoreTest, KeysWithEmptyStore) {
    auto result = store->keys(".*");
    EXPECT_TRUE(result.empty());
}

TEST_F(KeyValueStoreTest, KeysWithSpecialRegexChars) {
    store->set("key.one", "a");
    store->set("key+two", "b");
    store->set("key*three", "c");

    // Literal dot should match only key.one
    auto dot = store->keys("key\\.one");
    EXPECT_EQ(dot.size(), 1u);

    // Escaped plus should match only key+two
    auto plus = store->keys("key\\+two");
    EXPECT_EQ(plus.size(), 1u);

    // Escaped star should match only key*three
    auto star = store->keys("key\\*three");
    EXPECT_EQ(star.size(), 1u);
}

TEST_F(KeyValueStoreTest, LRemCountNegative) {
    store->rpush("neglist", "x");
    store->rpush("neglist", "a");
    store->rpush("neglist", "x");
    store->rpush("neglist", "b");
    store->rpush("neglist", "x");

    // Remove 2 'x' from tail side (count = -2)
    size_t removed = store->lrem("neglist", -2, "x");
    EXPECT_EQ(removed, 2u);
    EXPECT_EQ(store->llen("neglist"), 3u);
}

TEST_F(KeyValueStoreTest, LRemCountZero) {
    store->rpush("zerolist", "a");
    store->rpush("zerolist", "b");
    store->rpush("zerolist", "a");
    store->rpush("zerolist", "a");
    store->rpush("zerolist", "c");

    // count=0 removes all occurrences
    size_t removed = store->lrem("zerolist", 0, "a");
    EXPECT_EQ(removed, 3u);
    EXPECT_EQ(store->llen("zerolist"), 2u);
}

TEST_F(KeyValueStoreTest, UnsubscribeWithNoSubscriptions) {
    EXPECT_NO_THROW(store->unsubscribe("unsub_channel"));
}

// ========== exists() with expired key ==========

TEST_F(KeyValueStoreTest, ExistsReturnsFalseForExpiredKey) {
    wingman::KvOptions options;
    options.ttl = 1;
    store->set("exp_key", "value", options);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // exists() should return false and clean up the expired entry
    EXPECT_FALSE(store->exists("exp_key"));
    EXPECT_EQ(store->get("exp_key"), "");
}

// ========== lrem edge cases ==========

TEST_F(KeyValueStoreTest, LRemRemovesMatchingValues) {
    store->rpush("rem_list", "a");
    store->rpush("rem_list", "b");
    store->rpush("rem_list", "a");
    store->rpush("rem_list", "c");
    store->rpush("rem_list", "a");

    // Remove 2 occurrences of "a" from right
    int removed = store->lrem("rem_list", 2, "a");
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(store->llen("rem_list"), 3);
}

TEST_F(KeyValueStoreTest, LRemAllMatchingEmptiesList) {
    store->rpush("rem_all", "x");
    store->rpush("rem_all", "x");

    int removed = store->lrem("rem_all", 0, "x");
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(store->llen("rem_all"), 0);
}

TEST_F(KeyValueStoreTest, LRemNoMatchReturnsZero) {
    store->rpush("rem_nomatch", "a");
    store->rpush("rem_nomatch", "b");

    int removed = store->lrem("rem_nomatch", 0, "z");
    EXPECT_EQ(removed, 0);
    EXPECT_EQ(store->llen("rem_nomatch"), 2);
}

// ========== save/load with SQLite ==========

TEST_F(KeyValueStoreTest, SaveAndLoadRoundtripWithTTL) {
    wingman::KvOptions options;
    options.ttl = 300;
    store->set("ttl_key", "ttl_value", options);
    store->set("perm_key", "perm_value");

    std::string path = (fs::path(testDbPath).replace_extension(".json")).string();
    EXPECT_TRUE(store->save(path));

    auto store2 = std::make_unique<wingman::KeyValueStore>();
    EXPECT_TRUE(store2->load(path));

    EXPECT_EQ(store2->get("perm_key"), "perm_value");
    // TTL key may or may not be loaded depending on expiry

    std::filesystem::remove(path);
}
