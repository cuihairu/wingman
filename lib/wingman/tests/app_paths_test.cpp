// app_paths（应用数据目录/平台名）行为测试：目录创建、命名约定、平台名枚举。
// CI 各平台都有 HOME/用户 Profile，appDataDir 正常路径应非空且已存在。

#include "wingman/platform/app_paths.hpp"

#include <gtest/gtest.h>

#include <filesystem>

TEST(AppPathsTest, AppDataDirCreatedAndNamed) {
    const auto dir = wingman::platform::appDataDir();

    ASSERT_FALSE(dir.empty());
    EXPECT_EQ(dir.filename().string(), "wingman");
    EXPECT_TRUE(std::filesystem::exists(dir));
    EXPECT_TRUE(std::filesystem::is_directory(dir));
}

TEST(AppPathsTest, AppDataDirIdempotent) {
    const auto first = wingman::platform::appDataDir();
    const auto second = wingman::platform::appDataDir();
    EXPECT_EQ(first, second);
}

TEST(AppPathsTest, PlatformNameIsKnownValue) {
    const auto name = wingman::platform::platformName();
    EXPECT_TRUE(name == "windows" || name == "linux" || name == "macos")
        << "unexpected platform name: " << name;
}
