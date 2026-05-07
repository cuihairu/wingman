#include <gtest/gtest.h>
#include "wingman/version.hpp"

using namespace wingman;

class VersionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Version Constants Tests
// ============================================================================

TEST_F(VersionTest, VersionConstants) {
    EXPECT_STREQ(version::MAJOR, "0");
    EXPECT_STREQ(version::MINOR, "1");
    EXPECT_STREQ(version::PATCH, "0");
}

// ============================================================================
// Version String Tests
// ============================================================================

TEST_F(VersionTest, GetFullVersion) {
    std::string fullVersion = version::getFullVersion();
    EXPECT_FALSE(fullVersion.empty());

    // Should contain major.minor.patch format
    EXPECT_NE(fullVersion.find('.'), std::string::npos);

    // Check base version
    std::string baseVersion = std::string(version::MAJOR) + "." +
                               version::MINOR + "." +
                               version::PATCH;
    EXPECT_TRUE(fullVersion.find(baseVersion) == 0 ||
                fullVersion == baseVersion);
}

TEST_F(VersionTest, GetSuffix) {
    std::string suffix = version::getSuffix();
    // Suffix may be empty
    SUCCEED();
}

// ============================================================================
// Build Information Tests
// ============================================================================

TEST_F(VersionTest, GetBuildDate) {
    const char* date = version::getBuildDate();
    ASSERT_NE(date, nullptr);
    EXPECT_STRNE(date, "");

    // Should contain typical date format
    std::string dateStr(date);
    EXPECT_TRUE(dateStr.length() >= 11);
}

TEST_F(VersionTest, GetBuildTime) {
    const char* time = version::getBuildTime();
    ASSERT_NE(time, nullptr);
    EXPECT_STRNE(time, "");

    std::string timeStr(time);
    EXPECT_TRUE(timeStr.length() >= 8);
}

TEST_F(VersionTest, GetCompiler) {
    const char* compiler = version::getCompiler();
    ASSERT_NE(compiler, nullptr);
    EXPECT_STRNE(compiler, "");

    std::string compilerStr(compiler);
    // Should be one of: MSVC, GCC, Clang, or Unknown
    bool isValid = compilerStr == "MSVC" ||
                  compilerStr.find("GCC") == 0 ||
                  compilerStr.find("Clang") == 0 ||
                  compilerStr == "Unknown";
    EXPECT_TRUE(isValid);
}

TEST_F(VersionTest, GetCompilerVersion) {
    std::string compilerVer = version::getCompilerVersion();
    EXPECT_FALSE(compilerVer.empty());
}

// ============================================================================
// Version Format Tests
// ============================================================================

TEST_F(VersionTest, VersionFormat) {
    std::string fullVersion = version::getFullVersion();

    // Version should follow semantic versioning format
    // major.minor.patch or major.minor.patch-suffix
    EXPECT_TRUE(fullVersion.find("0.1.0") == 0);
}

TEST_F(VersionTest, VersionComponentsNumeric) {
    // Ensure major, minor, patch are numeric strings
    std::string major = version::MAJOR;
    std::string minor = version::MINOR;
    std::string patch = version::PATCH;

    EXPECT_FALSE(major.empty());
    EXPECT_FALSE(minor.empty());
    EXPECT_FALSE(patch.empty());

    // Check if they can be converted to integers
    EXPECT_NO_THROW(std::stoi(major));
    EXPECT_NO_THROW(std::stoi(minor));
    EXPECT_NO_THROW(std::stoi(patch));
}
