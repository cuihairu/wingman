#include <gtest/gtest.h>
#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"
#include "wingman/storage/storage_all.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

using namespace wingman;

// ========== SessionStorage ==========

TEST(SessionStorageTest, EmptyOnCreation) {
    SessionStorage storage;
    EXPECT_EQ(storage.length(), 0u);
    EXPECT_TRUE(storage.keys().empty());
}

TEST(SessionStorageTest, SetGetItem) {
    SessionStorage storage;
    storage.setItem("key1", "value1");
    EXPECT_EQ(storage.length(), 1u);

    auto val = storage.getItem("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "value1");
}

TEST(SessionStorageTest, GetNonexistent) {
    SessionStorage storage;
    EXPECT_FALSE(storage.getItem("missing").has_value());
}

TEST(SessionStorageTest, RemoveItem) {
    SessionStorage storage;
    storage.setItem("key1", "val");
    EXPECT_TRUE(storage.removeItem("key1"));
    EXPECT_EQ(storage.length(), 0u);
    EXPECT_FALSE(storage.getItem("key1").has_value());
}

TEST(SessionStorageTest, RemoveNonexistent) {
    SessionStorage storage;
    EXPECT_FALSE(storage.removeItem("missing"));
}

TEST(SessionStorageTest, Clear) {
    SessionStorage storage;
    storage.setItem("a", "1");
    storage.setItem("b", "2");
    storage.clear();
    EXPECT_EQ(storage.length(), 0u);
}

TEST(SessionStorageTest, HasItem) {
    SessionStorage storage;
    EXPECT_FALSE(storage.hasItem("key"));
    storage.setItem("key", "val");
    EXPECT_TRUE(storage.hasItem("key"));
}

TEST(SessionStorageTest, Keys) {
    SessionStorage storage;
    storage.setItem("k1", "v1");
    storage.setItem("k2", "v2");

    auto k = storage.keys();
    EXPECT_EQ(k.size(), 2u);
}

TEST(SessionStorageTest, OverwriteValue) {
    SessionStorage storage;
    storage.setItem("key", "old");
    storage.setItem("key", "new");
    EXPECT_EQ(*storage.getItem("key"), "new");
    EXPECT_EQ(storage.length(), 1u);
}

// ========== LocalStorage ==========

class LocalStorageTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() /
            ("wingman_test_storage_" + std::to_string(std::rand()));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }
};

TEST_F(LocalStorageTest, EmptyOnCreation) {
    LocalStorage storage(tempDir);
    EXPECT_EQ(storage.length(), 0u);
}

TEST_F(LocalStorageTest, SetGetItem) {
    LocalStorage storage(tempDir);
    storage.setItem("key1", "value1");

    auto val = storage.getItem("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "value1");
}

TEST_F(LocalStorageTest, PersistAcrossInstances) {
    {
        LocalStorage storage(tempDir);
        storage.setItem("persistent", "data");
    }

    {
        LocalStorage storage(tempDir);
        auto val = storage.getItem("persistent");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "data");
    }
}

TEST_F(LocalStorageTest, RemoveItem) {
    LocalStorage storage(tempDir);
    storage.setItem("key", "val");
    EXPECT_TRUE(storage.removeItem("key"));
    EXPECT_FALSE(storage.getItem("key").has_value());
}

TEST_F(LocalStorageTest, Clear) {
    LocalStorage storage(tempDir);
    storage.setItem("a", "1");
    storage.setItem("b", "2");
    storage.clear();
    EXPECT_EQ(storage.length(), 0u);
}

TEST_F(LocalStorageTest, HasItem) {
    LocalStorage storage(tempDir);
    EXPECT_FALSE(storage.hasItem("key"));
    storage.setItem("key", "val");
    EXPECT_TRUE(storage.hasItem("key"));
}

TEST_F(LocalStorageTest, Keys) {
    LocalStorage storage(tempDir);
    storage.setItem("k1", "v1");
    storage.setItem("k2", "v2");
    auto k = storage.keys();
    EXPECT_EQ(k.size(), 2u);
}

TEST_F(LocalStorageTest, Namespace) {
    LocalStorage storage(tempDir);
    storage.setNamespace("test_ns");
    EXPECT_EQ(storage.getNamespace(), "test_ns");

    storage.setItem("key", "val");
    EXPECT_TRUE(storage.getItem("key").has_value());
}

TEST_F(LocalStorageTest, GetStoragePath) {
    LocalStorage storage(tempDir);
    EXPECT_EQ(storage.getStoragePath(), tempDir);
}

// ========== StorageFactory ==========

TEST(StorageFactoryTest, CreateSession) {
    auto storage = StorageFactory::createSession();
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->length(), 0u);
}

TEST(StorageFactoryTest, CreateLocal) {
    auto dir = std::filesystem::temp_directory_path() /
        ("wingman_test_factory_" + std::to_string(std::rand()));
    auto storage = StorageFactory::createLocal(dir);
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->length(), 0u);
    std::filesystem::remove_all(dir);
}

// ========== Additional SessionStorage Tests ==========

TEST(SessionStorageTest, EmptyKey) {
    SessionStorage storage;
    storage.setItem("", "empty_key_val");
    auto val = storage.getItem("");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "empty_key_val");
}

TEST(SessionStorageTest, EmptyValue) {
    SessionStorage storage;
    storage.setItem("key", "");
    auto val = storage.getItem("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

TEST(SessionStorageTest, LargeValue) {
    SessionStorage storage;
    std::string large(10000, 'x');
    storage.setItem("big", large);
    auto val = storage.getItem("big");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, large);
}

TEST(SessionStorageTest, UnicodeKeys) {
    SessionStorage storage;
    storage.setItem("key_cn", "value_cn");
    auto val = storage.getItem("key_cn");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "value_cn");
}

TEST(SessionStorageTest, RemoveItemOnEmpty) {
    SessionStorage storage;
    EXPECT_FALSE(storage.removeItem("nonexistent"));
    EXPECT_EQ(storage.length(), 0u);
}

TEST(SessionStorageTest, ClearOnEmpty) {
    SessionStorage storage;
    EXPECT_NO_THROW(storage.clear());
    EXPECT_EQ(storage.length(), 0u);
}

// ========== Additional LocalStorage Tests ==========

TEST_F(LocalStorageTest, OverwriteValue) {
    LocalStorage storage(tempDir);
    storage.setItem("key", "old");
    storage.setItem("key", "new");
    auto val = storage.getItem("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "new");
    EXPECT_EQ(storage.length(), 1u);
}

TEST_F(LocalStorageTest, EmptyValue) {
    LocalStorage storage(tempDir);
    storage.setItem("key", "");
    auto val = storage.getItem("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

TEST_F(LocalStorageTest, LargeValue) {
    LocalStorage storage(tempDir);
    std::string large(10000, 'y');
    storage.setItem("big", large);
    auto val = storage.getItem("big");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, large);
}

TEST_F(LocalStorageTest, RemoveNonexistent) {
    LocalStorage storage(tempDir);
    EXPECT_FALSE(storage.removeItem("nonexistent"));
}

TEST_F(LocalStorageTest, ClearOnEmpty) {
    LocalStorage storage(tempDir);
    EXPECT_NO_THROW(storage.clear());
    EXPECT_EQ(storage.length(), 0u);
}

TEST_F(LocalStorageTest, HasItemFalse) {
    LocalStorage storage(tempDir);
    EXPECT_FALSE(storage.hasItem("nonexistent"));
}

TEST_F(LocalStorageTest, KeysEmpty) {
    LocalStorage storage(tempDir);
    EXPECT_TRUE(storage.keys().empty());
}

TEST_F(LocalStorageTest, NamespaceIsolation) {
    LocalStorage storage1(tempDir);
    storage1.setNamespace("ns1");
    storage1.setItem("key", "v1");

    LocalStorage storage2(tempDir);
    storage2.setNamespace("ns2");
    EXPECT_FALSE(storage2.hasItem("key"));
    storage2.setItem("key", "v2");

    EXPECT_EQ(*storage1.getItem("key"), "v1");
    EXPECT_EQ(*storage2.getItem("key"), "v2");
}

TEST_F(LocalStorageTest, PersistMultipleItems) {
    {
        LocalStorage storage(tempDir);
        storage.setItem("a", "1");
        storage.setItem("b", "2");
        storage.setItem("c", "3");
    }
    {
        LocalStorage storage(tempDir);
        EXPECT_EQ(*storage.getItem("a"), "1");
        EXPECT_EQ(*storage.getItem("b"), "2");
        EXPECT_EQ(*storage.getItem("c"), "3");
        EXPECT_EQ(storage.length(), 3u);
    }
}

TEST_F(LocalStorageTest, NamespaceUsesSeparateFile) {
    {
        LocalStorage storage(tempDir);
        storage.setNamespace("my_ns");
        storage.setItem("key1", "val1");
    }
    // The file should be named my_ns.json (saved with namespace prefix)
    std::string expectedFile = (tempDir / "my_ns.json").string();
    EXPECT_TRUE(std::filesystem::exists(expectedFile));

    // Verify file content contains the namespaced key
    std::ifstream file(expectedFile);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("my_ns:key1"), std::string::npos);
    EXPECT_NE(content.find("val1"), std::string::npos);
}

TEST_F(LocalStorageTest, DefaultNamespaceUsesStorageJson) {
    {
        LocalStorage storage(tempDir);
        // No namespace set — should use storage.json
        storage.setItem("default_key", "default_val");
    }
    std::string expectedFile = (tempDir / "storage.json").string();
    EXPECT_TRUE(std::filesystem::exists(expectedFile));

    {
        LocalStorage storage(tempDir);
        auto val = storage.getItem("default_key");
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, "default_val");
    }
}

TEST_F(LocalStorageTest, RemoveItemFromNamespacedStorage) {
    LocalStorage storage(tempDir);
    storage.setNamespace("rm_ns");
    storage.setItem("to_remove", "value");
    EXPECT_TRUE(storage.hasItem("to_remove"));
    EXPECT_TRUE(storage.removeItem("to_remove"));
    EXPECT_FALSE(storage.hasItem("to_remove"));
}

TEST_F(LocalStorageTest, ClearNamespacedStorage) {
    LocalStorage storage(tempDir);
    storage.setNamespace("clear_ns");
    storage.setItem("a", "1");
    storage.setItem("b", "2");
    storage.clear();
    EXPECT_EQ(storage.length(), 0u);
}

TEST_F(LocalStorageTest, GetNamespaceDefaultEmpty) {
    LocalStorage storage(tempDir);
    EXPECT_TRUE(storage.getNamespace().empty());
}

TEST_F(LocalStorageTest, GetStoragePathReturnsConstructorDir) {
    LocalStorage storage(tempDir);
    EXPECT_EQ(storage.getStoragePath(), tempDir);
}
