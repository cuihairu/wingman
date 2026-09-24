// 第十批补测：ScriptManager 配置加载与热重载检查的分支缺口——
// JSON 非对象根拒绝（loadJsonConfig）、autoReload 关闭早退
// （checkReload_Locked）、脚本文件删除后 stat 失败（getFileModifiedTime）。
#include <gtest/gtest.h>
#include "wingman/script_manager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace wingman;

namespace {

class ScriptManagerConfigReloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path().string() + "/wingman_sm_cfg_" +
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "_" +
            std::to_string(++instanceCounter);
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    std::string writeFile(const std::string& name, const std::string& content) {
        std::string path = tempDir + "/" + name;
        std::ofstream(path) << content;
        return path;
    }

    std::string tempDir;
    static std::atomic<int> instanceCounter;
};

std::atomic<int> ScriptManagerConfigReloadTest::instanceCounter{0};

} // namespace

TEST_F(ScriptManagerConfigReloadTest, LoadJsonConfigRejectsNonObjectRoot) {
    // loadJsonConfig 的 !j.is_object() 分支：顶层数组/字符串/数字一律拒绝
    ScriptManager mgr;
    const auto arrPath = writeFile("arr.json", "[1, 2, 3]");
    const auto strPath = writeFile("str.json", "\"just-a-string\"");
    const auto numPath = writeFile("num.json", "42");
    const auto badPath = writeFile("bad.json", "{not valid json");

    EXPECT_FALSE(mgr.loadConfig(arrPath));
    EXPECT_FALSE(mgr.loadConfig(strPath));
    EXPECT_FALSE(mgr.loadConfig(numPath));
    EXPECT_FALSE(mgr.loadConfig(badPath));

    // 对照组：合法对象根照常载入
    const auto okPath = writeFile("ok.json", R"({"k":"v"})");
    EXPECT_TRUE(mgr.loadConfig(okPath));
    EXPECT_EQ(mgr.getConfig("k", ""), "v");
}

TEST_F(ScriptManagerConfigReloadTest, CheckReloadSkipsWhenAutoReloadDisabled) {
    // checkReload_Locked 的 !autoReload && !globalAutoReload 早退分支：
    // 默认配置下两者皆关，文件 mtime 前移也不触发 reload
    ScriptManager mgr;
    const std::string path = writeFile("noreload.lua", "print('v1')");
    ASSERT_TRUE(mgr.loadScript("noreload", path));

    // 重写文件使 mtime 前移（Linux st_mtim 纳秒精度，20ms 足够）
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    { std::ofstream(path) << "print('v2')"; }

    // 两个开关皆关 → 早退，mtime 变化被无视
    EXPECT_FALSE(mgr.checkReload("noreload"));

    // 反向证明：全局开关打开后，同一变化立即触发 reload（返回 true，
    // 仅刷新 lastModified/state，不建引擎）
    mgr.setGlobalAutoReload(true);
    EXPECT_TRUE(mgr.checkReload("noreload"));
    // 再查一次：lastModified 已刷新，无新变化 → false
    EXPECT_FALSE(mgr.checkReload("noreload"));
}

TEST_F(ScriptManagerConfigReloadTest, CheckReloadAfterFileRemovedStatFails) {
    // getFileModifiedTime 的 stat 失败分支：脚本文件被删后 checkReload
    // 返回 0，不大于 lastModified → 不误触发 reload
    ScriptManager mgr;
    const std::string path = writeFile("vanish.lua", "print('gone')");
    ASSERT_TRUE(mgr.loadScript("vanish", path));
    mgr.setAutoReload("vanish", true);

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ASSERT_FALSE(std::filesystem::exists(path));

    EXPECT_FALSE(mgr.checkReload("vanish"));
    EXPECT_TRUE(mgr.hasScript("vanish"));  // 仅 stat 失败，注册表不受影响
}
