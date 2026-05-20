#include <gtest/gtest.h>
#include "wingman/filewatcher.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

using namespace wingman;

class FileWatcherTest : public ::testing::Test {
protected:
    std::string testDir;
    std::vector<FileChange> changes;

    void SetUp() override {
        // 创建临时测试目录
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        testDir = std::string(tempPath) + "wingman_test_" + std::to_string(GetTickCount64());
        fs::create_directories(testDir);
        changes.clear();
    }

    void TearDown() override {
        // 停止所有监控
        FileWatcher::unwatchPath(testDir);

        // 清理测试目录
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    FileChangeCallback getCallback() {
        return [this](const FileChange& change) {
            changes.push_back(change);
        };
    }
};

// ========== 基本测试 ==========

TEST_F(FileWatcherTest, WatchDirectory) {
    uint64_t watchId = FileWatcher::watch(testDir, getCallback(), false);

    ASSERT_NE(watchId, 0);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1);
    EXPECT_TRUE(FileWatcher::hasWatches());

    FileWatcher::unwatch(watchId);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0);
}

TEST_F(FileWatcherTest, FileCreated) {
    FileWatcher::watch(testDir, getCallback(), false);

    // 创建文件
    std::string filePath = testDir + "\\test_file.txt";
    std::ofstream(filePath) << "test content";

    // 等待事件
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 验证收到事件
    bool found = false;
    for (const auto& change : changes) {
        if (change.type == FileChangeType::Added &&
            change.path.find("test_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file added event";
}

TEST_F(FileWatcherTest, FileModified) {
    // 先创建文件
    std::string filePath = testDir + "\\test_modify.txt";
    {
        std::ofstream(filePath) << "initial content";
    }

    // 等待文件系统稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    changes.clear();
    FileWatcher::watch(testDir, getCallback(), false);

    // 修改文件
    {
        std::ofstream(filePath, std::ios::app) << " - more content";
    }

    // 等待事件
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 验证收到事件
    bool found = false;
    for (const auto& change : changes) {
        if (change.type == FileChangeType::Modified &&
            change.path.find("test_modify.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file modified event";
}

TEST_F(FileWatcherTest, FileDeleted) {
    // 先创建文件
    std::string filePath = testDir + "\\test_delete.txt";
    {
        std::ofstream(filePath) << "to be deleted";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    changes.clear();
    FileWatcher::watch(testDir, getCallback(), false);

    // 删除文件
    fs::remove(filePath);

    // 等待事件
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 验证收到事件
    bool found = false;
    for (const auto& change : changes) {
        if ((change.type == FileChangeType::Removed) &&
            change.path.find("test_delete.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file removed event";
}

// ========== 子目录测试 ==========

TEST_F(FileWatcherTest, RecursiveWatch) {
    // 创建子目录
    std::string subDir = testDir + "\\subdir";
    fs::create_directories(subDir);

    FileWatcher::watch(testDir, getCallback(), true);

    // 在子目录创建文件
    std::string filePath = subDir + "\\nested_file.txt";
    std::ofstream(filePath) << "nested content";

    // 等待事件
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证收到事件
    bool found = false;
    for (const auto& change : changes) {
        if (change.type == FileChangeType::Added &&
            change.path.find("nested_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive nested file event";
}

TEST_F(FileWatcherTest, NonRecursiveWatch) {
    // 创建子目录
    std::string subDir = testDir + "\\subdir";
    fs::create_directories(subDir);

    FileWatcher::watch(testDir, getCallback(), false);

    // 在子目录创建文件
    std::string filePath = subDir + "\\nested_file.txt";
    std::ofstream(filePath) << "nested content";

    // 等待
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 非递归监控不应收到子目录的事件
    bool found = false;
    for (const auto& change : changes) {
        if (change.path.find("nested_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_FALSE(found) << "Should not receive nested file event in non-recursive mode";
}

// ========== 多个监控测试 ==========

TEST_F(FileWatcherTest, MultipleWatches) {
    std::string dir2 = testDir + "\\dir2";
    fs::create_directories(dir2);

    uint64_t id1 = FileWatcher::watch(testDir, getCallback(), false);
    uint64_t id2 = FileWatcher::watch(dir2, getCallback(), false);

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_EQ(FileWatcher::getWatchCount(), 2);

    FileWatcher::unwatch(id1);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1);

    FileWatcher::unwatch(id2);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0);
}

TEST_F(FileWatcherTest, UnwatchPath) {
    std::string dir2 = testDir + "\\dir2";
    fs::create_directories(dir2);

    FileWatcher::watch(testDir, getCallback(), false);
    FileWatcher::watch(testDir, getCallback(), false);  // 同一路径，两个监控
    FileWatcher::watch(dir2, getCallback(), false);

    EXPECT_EQ(FileWatcher::getWatchCount(), 3);

    size_t removed = FileWatcher::unwatchPath(testDir);
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1);
}

// ========== 边界条件测试 ==========

TEST_F(FileWatcherTest, WatchNonExistentPath) {
    std::string nonExistent = testDir + "\\nonexistent";

    uint64_t watchId = FileWatcher::watch(nonExistent, getCallback(), false);

    EXPECT_EQ(watchId, 0);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0);
}

TEST_F(FileWatcherTest, WatchFileInsteadOfDirectory) {
    // 创建文件
    std::string filePath = testDir + "\\test.txt";
    std::ofstream(filePath) << "content";

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 监控文件（应该监控其所在目录）
    uint64_t watchId = FileWatcher::watch(filePath, getCallback(), false);

    EXPECT_NE(watchId, 0);
}

TEST_F(FileWatcherTest, MultipleRapidChanges) {
    FileWatcher::watch(testDir, getCallback(), false);

    // 快速创建多个文件
    for (int i = 0; i < 10; ++i) {
        std::string filePath = testDir + "\\rapid_" + std::to_string(i) + ".txt";
        std::ofstream(filePath) << "file " << i;
    }

    // 等待所有事件
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 应该收到多个事件
    EXPECT_GE(changes.size(), 5);  // 至少收到部分事件
}
