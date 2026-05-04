#include <gtest/gtest.h>
#include "wingman/system.hpp"

using namespace wingman;

class SystemTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// System Tests
// ============================================================================

TEST_F(SystemTest, GetCpuInfo) {
    CpuInfo info = System::getCpuInfo();
    EXPECT_FALSE(info.brand.empty());
    EXPECT_GT(info.cores, 0);
    EXPECT_GE(info.usage, 0);
}

TEST_F(SystemTest, GetCpuUsage) {
    int usage = System::getCpuUsage();
    EXPECT_GE(usage, 0);
    EXPECT_LE(usage, 100);
}

TEST_F(SystemTest, GetMemoryInfo) {
    MemoryInfo info = System::getMemoryInfo();
    EXPECT_GT(info.total, 0);
    EXPECT_GT(info.available, 0);
}

TEST_F(SystemTest, GetOsInfo) {
    OsInfo info = System::getOsInfo();
    EXPECT_FALSE(info.platform.empty());
    EXPECT_FALSE(info.version.empty());
}

TEST_F(SystemTest, GetUptime) {
    int uptime = System::getUptime();
    EXPECT_GT(uptime, 0);
}

TEST_F(SystemTest, GetDateTime) {
    std::string datetime = System::getDateTime();
    EXPECT_FALSE(datetime.empty());
}

TEST_F(SystemTest, GetTimeZone) {
    std::string timezone = System::getTimeZone();
    EXPECT_FALSE(timezone.empty());
}

TEST_F(SystemTest, GetProcessCount) {
    int count = System::getProcessCount();
    EXPECT_GT(count, 0);
}

TEST_F(SystemTest, GetThreadCount) {
    int count = System::getThreadCount();
    EXPECT_GT(count, 0);
}

TEST_F(SystemTest, GetNetworkAdapters) {
    auto adapters = System::getNetworkAdapters();
    EXPECT_TRUE(adapters.size() > 0);
}
