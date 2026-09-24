// Linux posix_process 全链路集成测试：真实 fork/exec /proc 遍历（非 mock）。
// sleep 进程做受控子进程，SIGTERM/SIGKILL 终止路径都实际收尸验证。
// fork 子进程内的行（execvp 后 _exit）经 _exit 退出不写 gcov 数据，属结构性
// 不可覆盖；fork 失败分支（pid==-1）无法在测试中注入，均不硬凑。
#ifdef __linux__

#include <gtest/gtest.h>
#include "wingman/process.hpp"

#include <chrono>
#include <thread>

using wingman::Process;

namespace {

// 起一个 sleep 子进程并保证测试结束时已收尸（不泄漏到并行用例）
class SleepChild {
public:
    explicit SleepChild(int seconds) {
        pid_ = Process::start("/bin/sleep", std::to_string(seconds));
    }
    ~SleepChild() {
        if (pid_ > 0 && Process::exists(pid_)) {
            Process::terminate(pid_, true);
            Process::wait(pid_, 3000);
        }
    }
    SleepChild(const SleepChild&) = delete;
    SleepChild& operator=(const SleepChild&) = delete;
    ProcessId pid() const { return pid_; }
private:
    ProcessId pid_ = 0;
};

} // namespace

TEST(PosixProcessTest, StartWaitExitLifecycle) {
    SleepChild child(1);
    ASSERT_GT(child.pid(), 0);

    EXPECT_TRUE(Process::exists(child.pid()));
    // fork 返回 ≠ 已 exec：comm 从 core_tests 变 sleep 有窗口，轮询等待
    std::string name;
    for (int i = 0; i < 30; ++i) {
        name = Process::getName(child.pid());
        if (name == "sleep") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(name, "sleep");
    EXPECT_FALSE(Process::getPath(child.pid()).empty());

    // 自然退出（timeoutMs>0 轮询分支 + WIFEXITED）
    EXPECT_TRUE(Process::wait(child.pid(), 5000));
    // waitpid 已收尸 → kill(pid,0) 得 ESRCH
    EXPECT_FALSE(Process::exists(child.pid()));
}

TEST(PosixProcessTest, WaitTimeoutThenTerminate) {
    SleepChild child(30);
    ASSERT_GT(child.pid(), 0);

    // 100ms 内 sleep 不会退出 → 超时分支
    EXPECT_FALSE(Process::wait(child.pid(), 100));

    // SIGTERM 优雅终止
    EXPECT_TRUE(Process::terminate(child.pid(), false));
    EXPECT_TRUE(Process::wait(child.pid(), 3000));

    // SIGKILL 强杀路径
    SleepChild child2(30);
    ASSERT_GT(child2.pid(), 0);
    EXPECT_TRUE(Process::terminate(child2.pid(), true));
    EXPECT_TRUE(Process::wait(child2.pid(), 3000));
}

TEST(PosixProcessTest, InvalidPidBranches) {
    // 不存在的 pid：kill ESRCH
    EXPECT_FALSE(Process::terminate(999999, false));
    EXPECT_FALSE(Process::exists(999999));

    // waitpid 对非子进程返回 -1 → wait 的 result==-1 分支
    EXPECT_FALSE(Process::wait(Process::getCurrentId(), 50));

    // 不存在的 pid 超时轮询：waitpid -1 → false
    EXPECT_FALSE(Process::wait(999999, 150));
}

TEST(PosixProcessTest, StartInvalidProgramExitsImmediately) {
    // execvp 失败子进程 _exit(1)：start 仍返回合法 pid，wait 观察到退出
    ProcessId pid = Process::start("/nonexistent/wingman-program");
    ASSERT_GT(pid, 0);
    EXPECT_TRUE(Process::wait(pid, 3000));
}

TEST(PosixProcessTest, FindAndEnumerate) {
    SleepChild child(3);
    ASSERT_GT(child.pid(), 0);
    // /proc 扫描的 comm 就绪可能略滞后于 fork 返回，轮询等待
    bool found = false;
    for (int i = 0; i < 30 && !found; ++i) {
        for (auto pid : Process::findAll("sleep")) {
            if (pid == child.pid()) { found = true; break; }
        }
        if (!found) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(found);
    EXPECT_EQ(Process::find("no-such-wm-process-xyz"), 0);

    // enumerate 含测试进程自身
    const auto self = Process::getCurrentId();
    bool selfFound = false;
    for (const auto& info : Process::enumerate()) {
        if (info.pid == self) {
            selfFound = true;
            EXPECT_EQ(info.name, Process::getName(self));
        }
    }
    EXPECT_TRUE(selfFound);
}

TEST(PosixProcessTest, WaitForNamePolling) {
    const std::string selfName = Process::getName(Process::getCurrentId());
    ASSERT_FALSE(selfName.empty());

    // waitFor 自身：立即命中
    EXPECT_TRUE(Process::waitFor(selfName, 2000));
    // waitFor 不存在的名字：轮询到超时
    EXPECT_FALSE(Process::waitFor("no-such-wm-process-xyz", 200));
    // waitExit 不存在的名字：立刻为真
    EXPECT_TRUE(Process::waitExit("no-such-wm-process-xyz", 200));
    // waitExit 自身：轮询到超时（进程仍在）
    EXPECT_FALSE(Process::waitExit(selfName, 200));
}

// ========== 第十一批补测：无限等待与未知 pid 的 name/path 空返回 ==========

TEST(PosixProcessTest, WaitForeverBlocksUntilExit) {
    // timeoutMs<=0 走阻塞式 waitpid（250-251）：子进程 2s 后自然退出
    SleepChild child(2);
    EXPECT_TRUE(Process::wait(child.pid(), 0));
    EXPECT_FALSE(Process::exists(child.pid()));
}

TEST(PosixProcessTest, UnknownPidNameAndPathAreEmpty) {
    // /proc/<dead>/comm 打不开 → 空（271-272）；readlink 失败 → 空（309）
    EXPECT_EQ(Process::getName(999999), "");
    EXPECT_EQ(Process::getPath(999999), "");
}

#endif // __linux__
