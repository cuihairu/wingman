// MacroRecorder 平台无关状态机离线测试：recordEvent 直接注入事件驱动
// 去重/持久化/回放/暂停恢复全链路，不依赖 XRecord（Xvfb 下 XRecord 链路
// 由 RecorderX11E2E.* 覆盖——EnableContext 必然失败、真桌面人工验证）。
// 覆盖 x11_recorder.cpp 的 saveToLua 六类型分支 / saveToJSON / loadFromJSON
// 错误三分支 / playback 真实 XTest 注入 / Xvfb 下 start 失败路径。
#if defined(__linux__) && !defined(__APPLE__)

#include <gtest/gtest.h>
#include "wingman/recorder.hpp"
#include "wingman/platform/input_factory.hpp"

#include <X11/Xlib.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using wingman::MacroRecorder;
using wingman::RecordedEvent;
using wingman::RecordedEventType;

namespace {

bool x11Available() {
    static const bool cached = [] {
        Display* d = XOpenDisplay(nullptr);
        if (!d) return false;
        XCloseDisplay(d);
        return true;
    }();
    return cached;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

RecordedEvent makeEvent(RecordedEventType type, int x = 0, int y = 0,
                        unsigned long ts = 0, int button = 0, int keyCode = 0,
                        const std::string& text = "", int delay = 0) {
    RecordedEvent e;
    e.type = type;
    e.timestamp = ts;
    e.x = x;
    e.y = y;
    e.button = button;
    e.keyCode = keyCode;
    e.text = text;
    e.delay = delay;
    return e;
}

std::string tempPath(const char* tag) {
    char path[96];
    std::snprintf(path, sizeof(path), "%s/wingman_rec_%d_%s",
                  ::testing::TempDir().c_str(), static_cast<int>(::getpid()), tag);
    return path;
}

} // namespace

TEST(MacroRecorderOffline, RecordEventDedupsConsecutiveMoves) {
    MacroRecorder rec;
    EXPECT_EQ(rec.getEventCount(), 0u);

    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 10, 10));
    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 20, 20));
    EXPECT_EQ(rec.getEventCount(), 1u);  // 连续 Move 合并替换

    rec.recordEvent(makeEvent(RecordedEventType::KeyDown, 0, 0, 5, 0, 65));
    EXPECT_EQ(rec.getEventCount(), 2u);

    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 30, 30));
    EXPECT_EQ(rec.getEventCount(), 3u);  // Move 前驱是 KeyDown，不去重

    rec.clear();
    EXPECT_EQ(rec.getEventCount(), 0u);
}

TEST(MacroRecorderOffline, SaveToLuaEmitsAllEventTypes) {
    MacroRecorder rec;
    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 100, 200));
    rec.recordEvent(makeEvent(RecordedEventType::MouseClick, 100, 200, 10, 1));
    rec.recordEvent(makeEvent(RecordedEventType::Scroll, 100, 200, 20, 0, 0, "", 3));
    rec.recordEvent(makeEvent(RecordedEventType::KeyDown, 0, 0, 30, 0, 65));
    rec.recordEvent(makeEvent(RecordedEventType::Type, 0, 0, 40, 0, 0, "hi", 5));
    rec.recordEvent(makeEvent(RecordedEventType::Delay, 0, 0, 50, 0, 0, "", 100));

    const auto path = tempPath("lua");
    ASSERT_TRUE(rec.saveToLua(path));
    const std::string lua = readFile(path);
    EXPECT_NE(lua.find("input.move(100, 200)"), std::string::npos);
    EXPECT_NE(lua.find("input.click(100, 200, 1)"), std::string::npos);
    EXPECT_NE(lua.find("input.scroll(100, 200, 3)"), std::string::npos);
    EXPECT_NE(lua.find("input.key(65)"), std::string::npos);
    EXPECT_NE(lua.find("input.type(\"hi\", 5)"), std::string::npos);
    EXPECT_NE(lua.find("util.sleep(100)"), std::string::npos);
    std::remove(path.c_str());

    // 不可写路径优雅 false
    EXPECT_FALSE(rec.saveToLua("/nonexistent-dir-wm/macro.lua"));
}

TEST(MacroRecorderOffline, SaveLoadJsonRoundtripAndErrorBranches) {
    MacroRecorder rec;
    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 5, 6, 100));
    rec.recordEvent(makeEvent(RecordedEventType::KeyDown, 0, 0, 200, 0, 66, "", 7));

    const auto path = tempPath("json");
    ASSERT_TRUE(rec.saveToJSON(path));
    EXPECT_NE(readFile(path).find("\"keyCode\": 66"), std::string::npos);

    MacroRecorder loaded;
    ASSERT_TRUE(loaded.loadFromJSON(path));
    EXPECT_EQ(loaded.getEventCount(), 2u);

    const auto path2 = tempPath("json2");
    ASSERT_TRUE(loaded.saveToJSON(path2));
    EXPECT_EQ(readFile(path), readFile(path2));  // 往返保真
    std::remove(path.c_str());
    std::remove(path2.c_str());

    // 错误三分支：文件不存在 / 非法 JSON / 缺 events 数组
    EXPECT_FALSE(loaded.loadFromJSON("/nonexistent-wm/rec.json"));
    const auto bad = tempPath("bad");
    { std::ofstream out(bad); out << "{not valid json"; }
    EXPECT_FALSE(loaded.loadFromJSON(bad));
    { std::ofstream out(bad); out << "{\"other\": []}"; }
    EXPECT_FALSE(loaded.loadFromJSON(bad));
    std::remove(bad.c_str());

    EXPECT_FALSE(rec.saveToJSON("/nonexistent-dir-wm/rec.json"));
}

TEST(MacroRecorderOffline, PauseResumeAndEmptyPlayback) {
    MacroRecorder rec;
    EXPECT_FALSE(rec.isRecording());
    EXPECT_FALSE(rec.isPaused());

    rec.pause();
    EXPECT_TRUE(rec.isPaused());
    rec.resume();
    EXPECT_FALSE(rec.isPaused());

    // 空事件 playback 立即返回（不触碰输入子系统）
    rec.playback(100, 1);
    EXPECT_EQ(rec.getEventCount(), 0u);
}

TEST(MacroRecorderOffline, PlaybackInjectsThroughXTest) {
    if (!x11Available()) {
        GTEST_SKIP() << "X display unavailable";
    }
    MacroRecorder rec;
    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 320, 240));
    rec.recordEvent(makeEvent(RecordedEventType::KeyDown, 0, 0, 0, 0, 0));  // 占位防 Move 合并
    rec.recordEvent(makeEvent(RecordedEventType::MouseMove, 640, 480));
    ASSERT_EQ(rec.getEventCount(), 3u);
    rec.playback(100, 1);  // timestamp 相同 → 无 sleep

    // 终点位置真实注入（Xvfb XQueryPointer 回读位置可信；按钮掩码不可信，
    // 故只注入 Move 驱动）
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);
    const auto pos = input->getMousePosition();
    EXPECT_LE(std::abs(pos.x - 640), 2);
    EXPECT_LE(std::abs(pos.y - 480), 2);
}

TEST(MacroRecorderOffline, StartFailsGracefullyWithoutRecordExtension) {
    MacroRecorder rec;
    rec.start();
    if (rec.isRecording()) {
        // 真实桌面（有 RECORD 扩展）：正向链路由 RecorderX11E2E.* 人工验证，
        // 此处只保证 stop 幂等收敛
        rec.stop();
        GTEST_SKIP() << "RECORD extension available (real desktop) — e2e case covers it";
    }
    // Xvfb：XRecordCreateContext 失败 → 优雅回到未录制态
    EXPECT_FALSE(rec.isRecording());
    rec.stop();  // 未录制时 stop 幂等
    EXPECT_EQ(rec.getEventCount(), 0u);
}

#endif // __linux__ && !__APPLE__
