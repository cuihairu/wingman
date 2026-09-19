#pragma once

#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// 脚本层 config 模块的单例在首次构造时读取 WINGMAN_CONFIG_DIR（见
// config_module.cpp）。这里在静态初始化阶段把它指到进程专属临时目录，
// 保证任何测试用例通过模块共享单例读写配置时，都不会写穿仓库真实的
// config/config.json。内联变量在每个包含它的 TU 各初始化一次，setenv
// 幂等，重复设置无副作用。
namespace wingman::testing {
inline const std::string kTestConfigDir = [] {
	auto dir = (fs::temp_directory_path() /
	            ("wingman_test_config_" +
	             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
	               .string();
#if defined(_WIN32)
	_putenv_s("WINGMAN_CONFIG_DIR", dir.c_str());
#else
	::setenv("WINGMAN_CONFIG_DIR", dir.c_str(), 1);
#endif
	return dir;
}();
} // namespace wingman::testing

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
