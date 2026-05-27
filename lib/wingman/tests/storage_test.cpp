#include <gtest/gtest.h>
#include "wingman/storage/session_storage.hpp"
#include "wingman/storage/local_storage.hpp"
#include "wingman/storage/storage_all.hpp"

#include <filesystem>

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
