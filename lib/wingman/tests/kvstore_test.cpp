#include <gtest/gtest.h>
#include <wingman/kvstore.hpp>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

class KeyValueStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = std::make_unique<wingman::KeyValueStore>();
        testDbPath = "test_kvstore.db";
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

// ========== 字符串操作 ==========

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

    // keys() 使用正则表达式，而不是通配符
    auto keys = store->keys("user:.*:name");
    EXPECT_EQ(keys.size(), 2);
}

TEST_F(KeyValueStoreTest, SetNX) {
    wingman::KvOptions options;
    options.nx = true;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "value1");

    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value1");  // 不应覆盖
}

TEST_F(KeyValueStoreTest, SetXX) {
    wingman::KvOptions options;
    options.xx = true;

    store->set("key1", "value1", options);
    EXPECT_EQ(store->get("key1"), "");  // 不应创建

    store->set("key1", "value1");
    store->set("key1", "value2", options);
    EXPECT_EQ(store->get("key1"), "value2");  // 应覆盖
}

TEST_F(KeyValueStoreTest, Incr) {
    store->set("counter", "10");
    EXPECT_EQ(store->incr("counter"), 11);
    EXPECT_EQ(store->get("counter"), "11");

    EXPECT_EQ(store->incr("counter", 5), 16);
    EXPECT_EQ(store->get("counter"), "16");
}

// ========== Hash 操作 ==========

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

// ========== List 操作 ==========

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

// ========== 发布订阅 ==========

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

// ========== 持久化 ==========

TEST_F(KeyValueStoreTest, SaveAndLoad) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->expire("key1", 3600);

    EXPECT_TRUE(store->save(testDbPath));

    auto newStore = std::make_unique<wingman::KeyValueStore>();
    EXPECT_TRUE(newStore->load(testDbPath));

    EXPECT_EQ(newStore->get("key1"), "value1");
    EXPECT_EQ(newStore->get("key2"), "value2");
    // Note: hash 和 list 当前不持久化，仅测试字符串键值
}

// ========== 统计信息 ==========

TEST_F(KeyValueStoreTest, Stats) {
    store->set("key1", "value1");
    store->set("key2", "value2");
    store->hset("hash1", "field1", "value3");

    auto stats = store->stats();
    EXPECT_GT(stats["strings"], 0);
    EXPECT_GT(stats["hashes"], 0);
}

// ========== 清理过期键 ==========

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
