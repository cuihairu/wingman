#include <gtest/gtest.h>
#include "wingman/storage/storage_all.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <fstream>

using namespace wingman;

class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试使用独立的存储目录
        testDir = "test_storage_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    void TearDown() override {
        // 清理测试目录
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }

    std::filesystem::path testDir;
};


// ========== SessionStorage Tests ==========

TEST_F(StorageTest, SessionStorage_BasicOperations) {
    auto storage = StorageFactory::createSession();

    // 初始状态
    EXPECT_EQ(storage->length(), 0);
    EXPECT_FALSE(storage->hasItem("key1"));

    // 设置值
    EXPECT_TRUE(storage->setItem("key1", "value1"));
    EXPECT_EQ(storage->length(), 1);
    EXPECT_TRUE(storage->hasItem("key1"));

    // 获取值
    auto value = storage->getItem("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value1");

    // 获取不存在的键
    EXPECT_FALSE(storage->getItem("nonexistent").has_value());

    // 删除值
    EXPECT_TRUE(storage->removeItem("key1"));
    EXPECT_EQ(storage->length(), 0);
    EXPECT_FALSE(storage->hasItem("key1"));

    // 删除不存在的键
    EXPECT_FALSE(storage->removeItem("nonexistent"));
}

TEST_F(StorageTest, SessionStorage_MultipleKeys) {
    auto storage = StorageFactory::createSession();

    storage->setItem("key1", "value1");
    storage->setItem("key2", "value2");
    storage->setItem("key3", "value3");

    EXPECT_EQ(storage->length(), 3);

    auto keys = storage->keys();
    EXPECT_EQ(keys.size(), 3);

    // 清空
    storage->clear();
    EXPECT_EQ(storage->length(), 0);
}


// ========== LocalStorage Tests ==========

TEST_F(StorageTest, LocalStorage_BasicOperations) {
    auto storage = StorageFactory::createLocal(testDir);

    // 初始状态
    EXPECT_EQ(storage->length(), 0);
    EXPECT_FALSE(storage->hasItem("key1"));

    // 设置值
    EXPECT_TRUE(storage->setItem("key1", "value1"));
    EXPECT_EQ(storage->length(), 1);
    EXPECT_TRUE(storage->hasItem("key1"));

    // 获取值
    auto value = storage->getItem("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value1");

    // 删除值
    EXPECT_TRUE(storage->removeItem("key1"));
    EXPECT_EQ(storage->length(), 0);
}

TEST_F(StorageTest, LocalStorage_Persistence) {
    {
        // 第一个实例写入数据
        auto storage1 = StorageFactory::createLocal(testDir);
        storage1->setItem("persist_key", "persist_value");
        storage1->setItem("number", "123");
    }  // 析构时自动保存

    {
        // 第二个实例读取数据（从文件加载）
        auto storage2 = StorageFactory::createLocal(testDir);

        EXPECT_EQ(storage2->length(), 2);
        auto value = storage2->getItem("persist_key");
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), "persist_value");

        auto number = storage2->getItem("number");
        ASSERT_TRUE(number.has_value());
        EXPECT_EQ(number.value(), "123");
    }
}

TEST_F(StorageTest, LocalStorage_Namespace) {
    auto storage = StorageFactory::createLocal(testDir);

    storage->setNamespace("user");
    storage->setItem("name", "Alice");
    storage->setItem("age", "25");

    storage->setNamespace("config");
    storage->setItem("theme", "dark");
    storage->setItem("lang", "en");

    // 切换回 user 命名空间
    storage->setNamespace("user");
    auto name = storage->getItem("name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name.value(), "Alice");

    // config 命名空间的数据在 user 中看不到
    EXPECT_FALSE(storage->getItem("theme").has_value());
}


// ========== Test JSON File Creation ==========

TEST_F(StorageTest, LocalStorage_FileCreation) {
    auto storage = StorageFactory::createLocal(testDir);
    storage->setItem("test", "data");

    // 检查文件是否创建
    std::filesystem::path storageFile = testDir / "storage.json";
    EXPECT_TRUE(std::filesystem::exists(storageFile));

    // 析构以确保数据写入
    storage.reset();

    // 检查文件内容
    std::ifstream file(storageFile);
    ASSERT_TRUE(file.is_open());

    nlohmann::json j;
    file >> j;

    EXPECT_TRUE(j.contains("test"));
    EXPECT_EQ(j["test"].get<std::string>(), "data");
}

TEST_F(StorageTest, LocalStorage_NamespaceFiles) {
    auto storage = StorageFactory::createLocal(testDir);

    storage->setNamespace("ns1");
    storage->setItem("key", "value1");
    storage.reset();  // 触发保存

    // 检查独立的命名空间文件
    std::filesystem::path ns1File = testDir / "ns1.json";
    EXPECT_TRUE(std::filesystem::exists(ns1File));

    std::filesystem::path defaultFile = testDir / "storage.json";
    EXPECT_FALSE(std::filesystem::exists(defaultFile));
}


// ========== Performance Tests ==========

TEST_F(StorageTest, SessionStorage_Performance) {
    auto storage = StorageFactory::createSession();

    const int count = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        storage->setItem("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(storage->length(), count);
    EXPECT_LT(duration.count(), 100);  // 应该在 100ms 内完成
}

TEST_F(StorageTest, LocalStorage_WritePerformance) {
    auto storage = StorageFactory::createLocal(testDir);

    const int count = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        storage->setItem("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(storage->length(), count);
    // 文件写入较慢，允许更长时间
    EXPECT_LT(duration.count(), 1000);
}


// ========== Edge Cases ==========

TEST_F(StorageTest, SessionStorage_EmptyKey) {
    auto storage = StorageFactory::createSession();

    // 空键也应该能工作
    EXPECT_TRUE(storage->setItem("", "empty_key_value"));
    EXPECT_TRUE(storage->hasItem(""));
    auto value = storage->getItem("");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "empty_key_value");
}

TEST_F(StorageTest, SessionStorage_SpecialCharacters) {
    auto storage = StorageFactory::createSession();

    std::string specialKey = "key:with:special/chars\\and\"quotes'";
    std::string specialValue = "value\nwith\nnewlines\tand\ttabs";

    EXPECT_TRUE(storage->setItem(specialKey, specialValue));
    auto value = storage->getItem(specialKey);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), specialValue);
}

TEST_F(StorageTest, SessionStorage_Overwrite) {
    auto storage = StorageFactory::createSession();

    storage->setItem("key", "value1");
    storage->setItem("key", "value2");

    auto value = storage->getItem("key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value(), "value2");  // 应该覆盖旧值
    EXPECT_EQ(storage->length(), 1);     // 长度应该是 1
}

TEST_F(StorageTest, LocalStorage_LargeValues) {
    auto storage = StorageFactory::createLocal(testDir);

    // 创建一个大值 (1MB)
    std::string largeValue(1024 * 1024, 'X');

    EXPECT_TRUE(storage->setItem("large", largeValue));

    auto value = storage->getItem("large");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value().size(), 1024 * 1024);
}
