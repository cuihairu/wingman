#include "rpc_ipc_test_common.hpp"

// ========== macro.* ==========

class MacroHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        wingman::rpc::registerMacroHandlers(dispatcher_, recorder_);
    }

    wingman::MacroRecorder recorder_;
    RpcDispatcher dispatcher_;
};

TEST_F(MacroHandlerTest, StatusInitiallyIdle) {
    const auto response = dispatchCall(dispatcher_, "macro.status");
    const auto& result = response["data"]["result"];
    EXPECT_FALSE(result["recording"].get<bool>());
    EXPECT_FALSE(result["paused"].get<bool>());
    EXPECT_EQ(result["eventCount"].get<std::size_t>(), 0u);
}

TEST_F(MacroHandlerTest, ClearSucceedsAndResetsEventCount) {
    const auto cleared = dispatchCall(dispatcher_, "macro.clear");
    EXPECT_TRUE(cleared["data"]["success"].get<bool>());

    const auto status = dispatchCall(dispatcher_, "macro.status");
    EXPECT_EQ(status["data"]["result"]["eventCount"].get<std::size_t>(), 0u);
}

TEST_F(MacroHandlerTest, SaveWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "macro.save");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(MacroHandlerTest, SaveToUnwritableDirectoryFails) {
    const auto response = dispatchCall(dispatcher_, "macro.save",
        json{{"path", tempPath("no-such-dir") + "/macro.json"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to save macro");
}

TEST_F(MacroHandlerTest, LoadWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "macro.load");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(MacroHandlerTest, LoadNonexistentFileFails) {
    const auto response = dispatchCall(
        dispatcher_, "macro.load", json{{"path", tempPath("missing-macro")}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load macro");
}

TEST_F(MacroHandlerTest, LoadGarbageFileFails) {
    const auto path = tempPath("garbage-macro");
    {
        std::ofstream file(path);
        file << "not-json{{{";
    }
    const auto response = dispatchCall(dispatcher_, "macro.load", json{{"path", path}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load macro");
    std::error_code ec;
    fs::remove(path, ec);
}

TEST_F(MacroHandlerTest, SaveLoadRoundTripPreservesEventCount) {
    const auto macroPath = writeTwoTickMacro(tempPath("roundtrip-macro"));

    const auto loaded = dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}});
    ASSERT_TRUE(loaded["data"]["success"].get<bool>()) << loaded.dump();
    // 带 success 键的 handler 返回体不包 result 层，直接作为 data
    EXPECT_EQ(loaded["data"]["eventCount"].get<std::size_t>(), 2u);

    const auto status = dispatchCall(dispatcher_, "macro.status");
    EXPECT_EQ(status["data"]["result"]["eventCount"].get<std::size_t>(), 2u);

    const auto savedPath = tempPath("saved-macro");
    const auto saved = dispatchCall(dispatcher_, "macro.save", json{{"path", savedPath}});
    ASSERT_TRUE(saved["data"]["success"].get<bool>()) << saved.dump();
    EXPECT_EQ(saved["data"]["eventCount"].get<std::size_t>(), 2u);

    std::error_code ec;
    fs::remove(savedPath, ec);
    fs::remove(macroPath, ec);
}

TEST_F(MacroHandlerTest, PlayWithEmptyQueueSucceeds) {
    const auto response = dispatchCall(dispatcher_, "macro.play");
    EXPECT_TRUE(response["data"]["success"].get<bool>());
}

TEST_F(MacroHandlerTest, PlayWithNonPositiveRepeatClampsToOne) {
    const auto macroPath = writeTwoTickMacro(tempPath("repeat-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    // repeat<=0 原语义为「循环 0 次」，钳到 1 后保持有回放语义；无副作用事件
    //（type=Delay）保证该用例瞬时完成
    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", 100}, {"repeat", 0}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

// 缺陷回归守卫（本轮修复）：speed=0 直达 playback 的整数除法触发 SIGFPE，
// 进程级崩溃；修复在 RPC 边界钳制 speed>=1。
TEST_F(MacroHandlerTest, PlayWithZeroSpeedIsClampedInsteadOfCrashing) {
    const auto macroPath = writeTwoTickMacro(tempPath("zerofpe-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", 0}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

// 缺陷回归守卫（本轮修复）：负 speed 使 playback 内 (delay*100)/speed 得负值，
// 赋给 unsigned long 下溢为巨量毫秒 → sleepMs 挂死 runtime。钳制后 speed=1，
// 相邻 timestamp 仅产生 100ms 延迟；修复前该用例会挂死直至测试超时。
TEST_F(MacroHandlerTest, PlayWithNegativeSpeedIsClampedInsteadOfHanging) {
    const auto macroPath = writeTwoTickMacro(tempPath("negspeed-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", -7}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

#if defined(__linux__)
// 平台钩子录制（X11/XRecord）依赖显示环境：断言信封与状态一致性，
// 不断言 recording 必为 true（无 X 环境优雅失败）
TEST_F(MacroHandlerTest, StartStopEnvelopesAreConsistent) {
    const auto started = dispatchCall(dispatcher_, "macro.start");
    EXPECT_TRUE(started["data"]["success"].get<bool>());
    EXPECT_EQ(started["data"]["recording"].get<bool>(), recorder_.isRecording());

    const auto stopped = dispatchCall(dispatcher_, "macro.stop");
    EXPECT_TRUE(stopped["data"]["success"].get<bool>());
    EXPECT_FALSE(stopped["data"]["recording"].get<bool>());
    EXPECT_EQ(stopped["data"]["eventCount"].get<std::size_t>(),
        recorder_.getEventCount());
    EXPECT_FALSE(recorder_.isRecording());
}
#endif

