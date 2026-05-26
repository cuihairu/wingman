#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class TempDirFixture : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir_ = fs::temp_directory_path() / ("wingman_test_" + std::to_string(std::hash<std::string>{}(
            std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))));
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }

    const fs::path& tempDir() const { return tempDir_; }

    std::string tempDirStr() const { return tempDir_.string(); }

private:
    fs::path tempDir_;
};
