#include <gtest/gtest.h>
#include "wingman/kvstore.hpp"
#include <filesystem>
#include <thread>
#include <chrono>

using namespace wingman;

class KvStoreTest : public ::testing::Test {
protected:
    std::unique_ptr<KeyValueStore> store;

    void SetUp() override {
        store = std::make_unique<KeyValueStore>();
    }

    void TearDown() override {
        // Clean up any test files
        std::filesystem::remove("test_kvstore.db");
    }
};

// ============================================================================
// String Operations Tests
// ============================================================================

TEST_F(KvStoreTest, SetGetDelete) {
    store->set("key1", "value1");
    EXPECT_EQ(store->get("key1"), "value1");

    store->del("key1");
    EXPECT_EQ(store->get("key1"), "");
}

TEST_F(KvStoreTest, SetWithOptions_NX) {
    KvOptions options;
    options.nx = true;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "value1");

    // Should not overwrite
    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value1");
}

TEST_F(KvStoreTest, SetWithOptions_XX) {
    KvOptions options;
    options.xx = true;

    // Should not set new key
    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "");

    // First set without XX
    store->set("key1", "value1");

    // Now XX should work
    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value2");
}

TEST_F(KvStoreTest, Exists) {
    EXPECT_FALSE(store->exists("nonexistent"));

    store->set("key1", "value1");
    EXPECT_TRUE(store->exists("key1"));
}

TEST_F(KvStoreTest, KeysPattern) {
    store->set("user:1:name", "Alice");
    store->set("user:1:age", "30");
    store->set("user:2:name", "Bob");
    store->set("session:abc", "data");

    auto userKeys = store->keys("user:*");
    EXPECT_EQ(userKeys.size(), 3);

    auto allUserKeys = store->keys("user:1:*");
    EXPECT_EQ(allUserKeys.size(), 2);
}

TEST_F(KvStoreTest, Expire) {
    store->set("key1", "value1");

    EXPECT_EQ(store->ttl("key1"), -1); // No expiry

    store->expire("key1", 2);
    EXPECT_GT(store->ttl("key1"), 0);
    EXPECT_LE(store->ttl("key1"), 2);

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_EQ(store->get("key1"), "");
}

TEST_F(KvStoreTest, Incr) {
    store->set("counter", "10");

    EXPECT_EQ(store->incr("counter"), 11);
    EXPECT_EQ(store->get("counter"), "11");

    EXPECT_EQ(store->incr("counter", 5), 16);
    EXPECT_EQ(store->get("counter"), "16");

    // Non-existent key
    EXPECT_EQ(store->incr("newcounter"), 1);
}

TEST_F(KvStoreTest, DeleteMultiple) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->set("key3", "value3");

    store->del(std::vector<std::string>{"key1", "key3"});

    EXPECT_EQ(store->get("key1"), "");
    EXPECT_EQ(store->get("key2"), "value2");
    EXPECT_EQ(store->get("key3"), "");
}

// ============================================================================
// Hash Operations Tests
// ============================================================================

TEST_F(KvStoreTest, HashSetGet) {
    store->hset("user:1", "name", "Alice");
    EXPECT_EQ(store->hget("user:1", "name"), "Alice");

    store->hset("user:1", "age", "30");
    EXPECT_EQ(store->hget("user:1", "age"), "30");
}

TEST_F(KvStoreTest, HashMSetGetAll) {
    HashFields fields;
    fields["name"] = "Bob";
    fields["age"] = "25";
    fields["city"] = "NYC";

    store->hmset("user:2", fields);

    auto all = store->hgetall("user:2");
    EXPECT_EQ(all.size(), 3);
    EXPECT_EQ(all["name"], "Bob");
    EXPECT_EQ(all["age"], "25");
    EXPECT_EQ(all["city"], "NYC");
}

TEST_F(KvStoreTest, HashDel) {
    store->hset("hash1", "field1", "value1");
    store->hset("hash1", "field2", "value2");

    EXPECT_TRUE(store->hexists("hash1", "field1"));

    store->hdel("hash1", "field1");
    EXPECT_FALSE(store->hexists("hash1", "field1"));
    EXPECT_EQ(store->hget("hash1", "field1"), "");
    EXPECT_EQ(store->hget("hash1", "field2"), "value2");
}

TEST_F(KvStoreTest, HashKeys) {
    store->hset("hash1", "field1", "value1");
    store->hset("hash1", "field2", "value2");
    store->hset("hash1", "field3", "value3");

    auto keys = store->hkeys("hash1");
    EXPECT_EQ(keys.size(), 3);
}

// ============================================================================
// List Operations Tests
// ============================================================================

TEST_F(KvStoreTest, ListPushPop) {
    store->lpush("list1", "item1");
    store->lpush("list1", "item2");
    store->rpush("list1", "item3");

    EXPECT_EQ(store->llen("list1"), 3);
    EXPECT_EQ(store->lpop("list1"), "item2");
    EXPECT_EQ(store->rpop("list1"), "item3");
    EXPECT_EQ(store->llen("list1"), 1);
}

TEST_F(KvStoreTest, ListRange) {
    store->lpush("list1", "item1");
    store->lpush("list1", "item2");
    store->lpush("list1", "item3");
    store->lpush("list1", "item4");
    store->lpush("list1", "item5");

    auto items = store->lrange("list1", 0, 2);
    EXPECT_EQ(items.size(), 3);
    EXPECT_EQ(items[0], "item5"); // lpush adds to front
    EXPECT_EQ(items[1], "item4");
    EXPECT_EQ(items[2], "item3");
}

TEST_F(KvStoreTest, ListRem) {
    store->lpush("list1", "item1");
    store->lpush("list1", "item2");
    store->lpush("list1", "item1");
    store->lpush("list1", "item3");

    // Remove 2 occurrences of "item1"
    size_t removed = store->lrem("list1", 2, "item1");
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(store->llen("list1"), 2);
}

// ============================================================================
// Persistence Tests
// ============================================================================

TEST_F(KvStoreTest, SaveLoad) {
    store->set("key1", "value1");
    store->hset("hash1", "field1", "value1");
    store->lpush("list1", "item1");

    ASSERT_TRUE(store->save("test_kvstore.db"));

    // Create new store and load
    auto newStore = std::make_unique<KeyValueStore>();
    ASSERT_TRUE(newStore->load("test_kvstore.db"));

    EXPECT_EQ(newStore->get("key1"), "value1");
    EXPECT_EQ(newStore->hget("hash1", "field1"), "value1");
    EXPECT_EQ(newStore->llen("list1"), 1);
    EXPECT_EQ(newStore->lpop("list1"), "item1");
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(KvStoreTest, CleanupExpired) {
    store->set("key1", "value1");
    store->expire("key1", 1);

    store->set("key2", "value2");
    store->expire("key2", 1);

    store->set("key3", "value3"); // No expiry

    std::this_thread::sleep_for(std::chrono::seconds(2));

    size_t cleaned = store->cleanupExpired();
    EXPECT_GT(cleaned, 0);

    EXPECT_EQ(store->get("key1"), "");
    EXPECT_EQ(store->get("key2"), "");
    EXPECT_EQ(store->get("key3"), "value3");
}

TEST_F(KvStoreTest, Stats) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->hset("hash1", "field1", "value1");
    store->lpush("list1", "item1");

    auto stats = store->stats();
    EXPECT_GT(stats.size(), 0);
}

// ============================================================================
// PubSub Tests (Basic)
// ============================================================================

TEST_F(KvStoreTest, PubSubBasic) {
    bool messageReceived = false;
    std::string receivedMessage;

    store->subscribe("test_channel", [&](const std::string& msg) {
        messageReceived = true;
        receivedMessage = msg;
    });

    store->publish("test_channel", "Hello, World!");

    // Give some time for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(messageReceived);
    EXPECT_EQ(receivedMessage, "Hello, World!");

    store->unsubscribe("test_channel");
}
