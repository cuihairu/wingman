/**
 * ScriptRunner 测试 — Android 端 Lua 执行器（docs/android-agent-design.md §5.2/§9）
 *
 * ScriptRunner 是纯 C++ + sol2 实现（apps/android/cpp/agent/script_runner.cpp），
 * 不依赖 Android API：桌面与 NDK 编译同一份源码。本测试验证 Lua 执行、
 * print 重定向、wingman.sleep 协作中断与死循环 hook 兜底。
 */

#include <gtest/gtest.h>

#include "agent/script_runner.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using wingman::android::ScriptRunner;

namespace {

// 收集输出的线程安全捕获器（用于 start() 异步路径）
class OutputCapture {
public:
    void attach(ScriptRunner& runner) {
        runner.setOutputCallback([this](const std::string& line) {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.push_back(line);
        });
    }

    bool contains(const std::string& needle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& line : lines_) {
            if (line.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
};

} // namespace

TEST(ScriptRunnerTest, OutputViaOutputParameter) {
    ScriptRunner runner;
    std::vector<std::string> lines;
    std::mutex mutex;
    std::atomic<bool> stop{false};

    const bool ok = runner.runSync("t1", "print('hello', 'android') print(42)",
                                   "lua", stop,
                                   [&](const std::string& line) {
                                       std::lock_guard<std::mutex> lock(mutex);
                                       lines.push_back(line);
                                   });

    ASSERT_TRUE(ok);
    // 2 行 print + 1 行结束标记（"[id] finished"，Android 端同样回传）
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "hello\tandroid");
    EXPECT_EQ(lines[1], "42");
    EXPECT_NE(lines[2].find("finished"), std::string::npos);
}

TEST(ScriptRunnerTest, WingmanLogAndSleepAvailable) {
    ScriptRunner runner;
    std::vector<std::string> lines;
    std::mutex mutex;
    std::atomic<bool> stop{false};

    // os.clock() 是 CPU 时间（sleep 期间不增长），墙钟在 C++ 侧度量
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = runner.runSync(
        "t1",
        "wingman.log('via log')\n"
        "wingman.sleep(120)\n"
        "print('after sleep')\n",
        "lua", stop,
        [&](const std::string& line) {
            std::lock_guard<std::mutex> lock(mutex);
            lines.push_back(line);
        });
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0)
            .count();

    ASSERT_TRUE(ok);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "via log");
    EXPECT_EQ(lines[1], "after sleep");
    EXPECT_GE(elapsedMs, 100);
    EXPECT_NE(lines[2].find("finished"), std::string::npos);
}

TEST(ScriptRunnerTest, LuaErrorReportedAndReturnsFalse) {
    ScriptRunner runner;
    std::vector<std::string> lines;
    std::mutex mutex;
    std::atomic<bool> stop{false};

    const bool ok = runner.runSync("t1", "error('boom')", "lua", stop,
                                   [&](const std::string& line) {
                                       std::lock_guard<std::mutex> lock(mutex);
                                       lines.push_back(line);
                                   });

    EXPECT_FALSE(ok);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines.back().find("ERROR"), std::string::npos);
    EXPECT_NE(lines.back().find("boom"), std::string::npos);
}

TEST(ScriptRunnerTest, StopBreaksLongSleep) {
    ScriptRunner runner;
    std::atomic<bool> stop{false};
    const auto t0 = std::chrono::steady_clock::now();

    // 5 秒睡眠在停止标志置位后应 ~150ms 内中断（50ms 片 + 调度余量）
    std::thread killer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stop.store(true);
    });
    const bool ok = runner.runSync("t1", "wingman.sleep(5000)", "lua", stop,
                                   [](const std::string&) {});
    killer.join();
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_FALSE(ok);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
              1500);
}

TEST(ScriptRunnerTest, HookBreaksBusyLoop) {
    ScriptRunner runner;
    std::atomic<bool> stop{false};
    const auto t0 = std::chrono::steady_clock::now();

    // 无 sleep 的死循环：由指令计数 hook 强制中断
    std::thread killer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stop.store(true);
    });
    const bool ok = runner.runSync("t1", "local x = 0 while true do x = x + 1 end",
                                   "lua", stop, [](const std::string&) {});
    killer.join();
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_FALSE(ok);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
              5000);
}

TEST(ScriptRunnerTest, StartRejectsUnsupportedLanguage) {
    ScriptRunner runner;
    EXPECT_FALSE(runner.start("t1", "print(1)", "python"));
}

TEST(ScriptRunnerTest, StartRunsAndCompletes) {
    ScriptRunner runner;
    OutputCapture capture;
    capture.attach(runner);

    ASSERT_TRUE(runner.start("t1", "print('async hello')", "lua"));
    ASSERT_TRUE(runner.isRunning());
    EXPECT_EQ(runner.executionId(), "t1");

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (runner.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_FALSE(runner.isRunning());
    EXPECT_TRUE(capture.contains("async hello"));
    EXPECT_TRUE(capture.contains("finished"));
}

TEST(ScriptRunnerTest, StartRejectsWhileRunning) {
    ScriptRunner runner;
    OutputCapture capture;
    capture.attach(runner);

    ASSERT_TRUE(runner.start("t1", "wingman.sleep(10000)", "lua"));
    EXPECT_FALSE(runner.start("t2", "print('nope')", "lua"));

    runner.stop();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (runner.isRunning() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_FALSE(runner.isRunning());
}
