#include <gtest/gtest.h>
#include "wingman/process.hpp"

using namespace wingman;

class ProcessTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Process Tests
// ============================================================================

TEST_F(ProcessTest, GetCurrentId) {
    ProcessId pid = Process::getCurrentId();
    EXPECT_GT(pid, 0);
}

TEST_F(ProcessTest, FindExistingProcess) {
    // Try to find a process that should exist (like explorer.exe)
    ProcessId pid = Process::find("explorer.exe");
    // On Windows, explorer.exe should always be running
    EXPECT_GT(pid, 0);
}

TEST_F(ProcessTest, CheckProcessExists) {
    // Current process should exist
    ProcessId currentPid = Process::getCurrentId();
    EXPECT_TRUE(Process::exists(currentPid));
}

TEST_F(ProcessTest, GetProcessName) {
    ProcessId currentPid = Process::getCurrentId();
    std::string name = Process::getName(currentPid);
    EXPECT_FALSE(name.empty());
}

TEST_F(ProcessTest, GetProcessPath) {
    ProcessId currentPid = Process::getCurrentId();
    std::string path = Process::getPath(currentPid);
    EXPECT_FALSE(path.empty());
}

TEST_F(ProcessTest, EnumerateProcesses) {
    auto processes = Process::enumerate();
    EXPECT_GT(processes.size(), 0);
}
