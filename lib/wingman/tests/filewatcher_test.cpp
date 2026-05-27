#include <gtest/gtest.h>
#include "wingman/filewatcher.hpp"
#include "wingman/platform/ifilewatcher.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

using namespace wingman;
using wingman::platform::FileChange;
using wingman::platform::FileChangeCallback;
using wingman::platform::FileChangeType;

#ifdef _WIN32

class FileWatcherTest : public ::testing::Test {
protected:
    std::string testDir;
    std::vector<FileChange> changes;
    std::mutex changesMutex;

    void SetUp() override {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        testDir = std::string(tempPath) + "wingman_test_" + std::to_string(GetTickCount64());
        fs::create_directories(testDir);

        std::lock_guard<std::mutex> lock(changesMutex);
        changes.clear();
    }

    void TearDown() override {
        FileWatcher::unwatchPath(testDir);
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    FileChangeCallback getCallback() {
        return [this](const FileChange& change) {
            std::lock_guard<std::mutex> lock(changesMutex);
            changes.push_back(change);
        };
    }

    void clearChanges() {
        std::lock_guard<std::mutex> lock(changesMutex);
        changes.clear();
    }

    std::vector<FileChange> snapshotChanges() {
        std::lock_guard<std::mutex> lock(changesMutex);
        return changes;
    }
};

TEST_F(FileWatcherTest, WatchDirectory) {
    const uint64_t watchId = FileWatcher::watch(testDir, getCallback(), false);

    ASSERT_NE(watchId, 0U);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1U);
    EXPECT_TRUE(FileWatcher::hasWatches());

    FileWatcher::unwatch(watchId);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0U);
}

TEST_F(FileWatcherTest, FileCreated) {
    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);

    const std::string filePath = testDir + "\\test_file.txt";
    std::ofstream(filePath) << "test content";

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool found = false;
    for (const auto& change : snapshotChanges()) {
        if (change.type == FileChangeType::Added &&
            change.path.find("test_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file added event";
}

TEST_F(FileWatcherTest, FileModified) {
    const std::string filePath = testDir + "\\test_modify.txt";
    {
        std::ofstream(filePath) << "initial content";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    clearChanges();
    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);

    {
        std::ofstream(filePath, std::ios::app) << " - more content";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool found = false;
    for (const auto& change : snapshotChanges()) {
        if (change.type == FileChangeType::Modified &&
            change.path.find("test_modify.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file modified event";
}

TEST_F(FileWatcherTest, FileDeleted) {
    const std::string filePath = testDir + "\\test_delete.txt";
    {
        std::ofstream(filePath) << "to be deleted";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    clearChanges();
    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);

    fs::remove(filePath);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool found = false;
    for (const auto& change : snapshotChanges()) {
        if (change.type == FileChangeType::Removed &&
            change.path.find("test_delete.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive file removed event";
}

TEST_F(FileWatcherTest, RecursiveWatch) {
    const std::string subDir = testDir + "\\subdir";
    fs::create_directories(subDir);

    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), true), 0U);

    const std::string filePath = subDir + "\\nested_file.txt";
    std::ofstream(filePath) << "nested content";

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    bool found = false;
    for (const auto& change : snapshotChanges()) {
        if (change.type == FileChangeType::Added &&
            change.path.find("nested_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected to receive nested file event";
}

TEST_F(FileWatcherTest, NonRecursiveWatch) {
    const std::string subDir = testDir + "\\subdir";
    fs::create_directories(subDir);

    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);

    const std::string filePath = subDir + "\\nested_file.txt";
    std::ofstream(filePath) << "nested content";

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool found = false;
    for (const auto& change : snapshotChanges()) {
        if (change.path.find("nested_file.txt") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_FALSE(found) << "Should not receive nested file event in non-recursive mode";
}

TEST_F(FileWatcherTest, MultipleWatches) {
    const std::string dir2 = testDir + "\\dir2";
    fs::create_directories(dir2);

    const uint64_t id1 = FileWatcher::watch(testDir, getCallback(), false);
    const uint64_t id2 = FileWatcher::watch(dir2, getCallback(), false);

    EXPECT_NE(id1, 0U);
    EXPECT_NE(id2, 0U);
    EXPECT_EQ(FileWatcher::getWatchCount(), 2U);

    FileWatcher::unwatch(id1);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1U);

    FileWatcher::unwatch(id2);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0U);
}

TEST_F(FileWatcherTest, UnwatchPath) {
    const std::string dir2 = testDir + "\\dir2";
    fs::create_directories(dir2);

    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);
    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);
    ASSERT_NE(FileWatcher::watch(dir2, getCallback(), false), 0U);

    EXPECT_EQ(FileWatcher::getWatchCount(), 3U);

    const size_t removed = FileWatcher::unwatchPath(testDir);
    EXPECT_EQ(removed, 2U);
    EXPECT_EQ(FileWatcher::getWatchCount(), 1U);
}

TEST_F(FileWatcherTest, WatchNonExistentPath) {
    const std::string nonExistent = testDir + "\\definitely_nonexistent_dir_"
        + std::to_string(GetTickCount64());

    ASSERT_FALSE(fs::exists(nonExistent)) << "Path should not exist: " << nonExistent;

    const uint64_t watchId = FileWatcher::watch(nonExistent, getCallback(), false);

    EXPECT_EQ(watchId, 0U);
    EXPECT_EQ(FileWatcher::getWatchCount(), 0U);
}

TEST_F(FileWatcherTest, WatchFileInsteadOfDirectory) {
    const std::string filePath = testDir + "\\test.txt";
    std::ofstream(filePath) << "content";

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const uint64_t watchId = FileWatcher::watch(filePath, getCallback(), false);
    EXPECT_NE(watchId, 0U);
}

TEST_F(FileWatcherTest, MultipleRapidChanges) {
    ASSERT_NE(FileWatcher::watch(testDir, getCallback(), false), 0U);

    for (int i = 0; i < 10; ++i) {
        const std::string filePath = testDir + "\\rapid_" + std::to_string(i) + ".txt";
        std::ofstream(filePath) << "file " << i;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(snapshotChanges().size(), 5U);
}

#else

TEST(FileWatcherTest, DisabledOnNonWindows) {
    GTEST_SKIP() << "Windows-only file watcher tests";
}

#endif
