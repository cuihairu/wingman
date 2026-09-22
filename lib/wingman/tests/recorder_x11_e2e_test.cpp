// XRecord 正向录制端到端验证（仅 Linux 编译，同 platform_x11_test.cpp 模式）。
//
// 与 recorder_test.cpp 的状态机用例互补：本文件验证「捕获 → 保存」的正向闭环——
// start 后经 XTest 注入真实按键，断言 XRecord 捕获到事件并可序列化。
//
// 环境要求：真实桌面 X server。Xvfb 的 RECORD 扩展存在但 EnableContext
// 必然失败（XRecordBadContext，见 x11_recorder.cpp 顶部注释），此时
// GTEST_SKIP——本用例只在有真实 X 会话的机器上执行（人工真桌面验证 =
// 跑 core_tests --gtest_filter='RecorderX11E2E.*'）。
//
// 注入的按键会进入当前焦点窗口：优先 F13（绝大多数桌面无副作用），
// 键码不存在时回退普通键 'a' 并在输出中提示。

#if defined(__linux__)

#include <gtest/gtest.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "x11_test_lock.hpp"
#include "wingman/recorder.hpp"

namespace fs = std::filesystem;

namespace {

// XTest 注入一个完整的按键（press + release）。
void injectKeyTap(Display* d, unsigned int keycode) {
    XTestFakeKeyEvent(d, keycode, True, CurrentTime);
    XFlush(d);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    XTestFakeKeyEvent(d, keycode, False, CurrentTime);
    XFlush(d);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

// 选注入键：优先 F13（桌面无副作用），键码 0（映射不存在）回退 'a'。
unsigned int pickInjectKeycode(Display* d, bool& usedFallback) {
    unsigned int kc = static_cast<unsigned int>(XKeysymToKeycode(d, XK_F13));
    if (kc != 0) {
        usedFallback = false;
        return kc;
    }
    usedFallback = true;
    return static_cast<unsigned int>(XKeysymToKeycode(d, XK_a));
}

} // namespace

TEST(RecorderX11E2E, RecordsXTestInjectedKeyEvents) {
    // RECORD context 与 XTest 注入都是 X server 全局操作，与窗口/剪贴板
    // 测试同锁串行化，避免 ctest -j 下的相互干扰。
    X11ServerLockGuard x11Lock;

    wingman::MacroRecorder recorder;
    recorder.start();
    if (!recorder.isRecording()) {
        GTEST_SKIP() << "RECORD extension unavailable (Xvfb/headless) — "
                        "run on a real desktop X server";
    }

    Display* d = XOpenDisplay(nullptr);
    ASSERT_NE(d, nullptr);

    bool usedFallback = false;
    const unsigned int keycode = pickInjectKeycode(d, usedFallback);
    ASSERT_NE(keycode, 0) << "neither F13 nor 'a' has a keycode mapping";
    // usedFallback 时注入的 'a' 会进入当前焦点窗口（真桌面人工验证时可接受）。

    constexpr int kTaps = 3;
    for (int i = 0; i < kTaps; ++i) {
        injectKeyTap(d, keycode);
    }

    // 捕获在 recorder 内部线程异步进行：轮询至多 ~2s。
    bool captured = false;
    for (int i = 0; i < 40; ++i) {
        if (recorder.getEventCount() >= static_cast<size_t>(kTaps)) {
            captured = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    recorder.stop();
    XCloseDisplay(d);

    EXPECT_TRUE(captured) << "XRecord did not capture " << kTaps
                          << " injected key events within 2s (captured="
                          << recorder.getEventCount() << ")";
    EXPECT_GE(recorder.getEventCount(), static_cast<size_t>(kTaps));
}

TEST(RecorderX11E2E, SavesCapturedEventsToJSON) {
    X11ServerLockGuard x11Lock;

    wingman::MacroRecorder recorder;
    recorder.start();
    if (!recorder.isRecording()) {
        GTEST_SKIP() << "RECORD extension unavailable (Xvfb/headless) — "
                        "run on a real desktop X server";
    }

    Display* d = XOpenDisplay(nullptr);
    ASSERT_NE(d, nullptr);

    bool usedFallback = false;
    const unsigned int keycode = pickInjectKeycode(d, usedFallback);
    ASSERT_NE(keycode, 0);
    injectKeyTap(d, keycode);

    for (int i = 0; i < 40 && recorder.getEventCount() < 1; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    recorder.stop();
    XCloseDisplay(d);

    ASSERT_GE(recorder.getEventCount(), 1u);

    const auto path =
        (fs::temp_directory_path() /
         ("test_recorder_e2e_" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()) +
          ".json"))
            .string();
    EXPECT_TRUE(recorder.saveToJSON(path));

    std::ifstream file(path);
    const std::string content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    std::error_code ec;
    fs::remove(path, ec);

    EXPECT_NE(content.find("\"events\""), std::string::npos)
        << "saved JSON lacks events array";
    // 只注入了按键：x11_recorder 回调把 KeyPress 映射为 KeyDown
    // （RecordedEventType 枚举值 5，KeyRelease 丢弃），故 type 应为 5。
    EXPECT_NE(content.find("\"type\": 5"), std::string::npos)
        << "saved JSON lacks a KeyDown event";
    // 精确匹配 keyCode 字段值（裸数字会在 timestamp 里误匹配）。
    EXPECT_NE(content.find("\"keyCode\": " +
                           std::to_string(static_cast<int>(keycode))),
              std::string::npos)
        << "saved JSON lacks the injected keycode " << keycode;
}

#endif // __linux__
