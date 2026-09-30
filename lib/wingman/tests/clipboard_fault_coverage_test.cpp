// 剪贴板平台链故障注入补测：clipboard.cpp / x11_clipboard.cpp 缺口。
//
// X11Clipboard 的 fork+xclip 链路有三条父进程侧降级路径，此前零触达：
// ① 初始化失败（坏 DISPLAY：工厂无视 initialize 成败都返回实例，未初始化
//    实例的全接口必须按降级契约行事——写 false / 读空 / isEmpty true）；
// ② pipe() EMFILE（fd 表填满注入——setText/getText 优雅 false/空，不崩）；
// ③ fork() EAGAIN（RLIMIT_NPROC 按 uid 计数注入，进程探测确认前提成立）。
// 注入前提不成立（共享机环境干扰）一律 GTEST_SKIP，不误报代码失败。
//
// 登记不可覆盖（见 CHANGELOG 本轮条目）：子进程分支（close/dup2/devnull/
// execlp/_exit）gcov 结构性不可观测——exec 替换进程镜像、_exit 跳过 gcov
// 刷盘；clipboard.cpp 的 NullClipboard（工厂 null 回退）在全平台均不可达
// （工厂恒返回实例）。
//
// 仅 Linux 编译（x11_clipboard.cpp 仅 Linux 侧编译）；无 X 环境用例自身
// 语义不变（坏 DISPLAY 注入本就不依赖真 X；fd/fork 注入走已初始化单例，
// 不触 X）。跨进程剪贴板锁：本文件用例都在 fork 失败/早退路径上，不会
// 真正触碰 selection，但 fork 失败注入的 setText 理论上可能注入失效而
// 走通成功路径 fork 出 xclip——按纪律统一持锁。
#if defined(__linux__)

#include <gtest/gtest.h>

#include "wingman/clipboard.hpp"
#include "wingman/platform/iclipboard.hpp"
#include "clipboard_lock_guard.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

// 工厂直连声明（同 clipboard.cpp；gcc 在 Linux 把 `linux` 定义为 1，先 undo）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<IClipboard> createX11Clipboard();
}

namespace {

// 坏 DISPLAY 注入的 RAII：析构恢复原值（原环境无 DISPLAY 则移除）
class DisplayOverride {
public:
    explicit DisplayOverride(const char* value) {
        const char* old = ::getenv("DISPLAY");
        hadOld_ = old != nullptr;
        if (hadOld_) old_.assign(old);
        ::setenv("DISPLAY", value, 1);
    }
    ~DisplayOverride() {
        if (hadOld_) ::setenv("DISPLAY", old_.c_str(), 1);
        else ::unsetenv("DISPLAY");
    }
    DisplayOverride(const DisplayOverride&) = delete;
    DisplayOverride& operator=(const DisplayOverride&) = delete;

private:
    bool hadOld_ = false;
    std::string old_;
};

// fd 表填满的 RAII：先把 soft 限额压到「当前占用 + 少量余量」再逐个占满——
// 默认 soft 限额动辄百万，直接逐个 open 是秒级窗口（全量跑实测 1.4s）；
// 压限后整个注入窗口毫秒级，并发释放 fd 打破注入的竞争窗口同步收窄。
// 析构全关 + 恢复原限额。
class FdTableFiller {
public:
    FdTableFiller() {
        if (::getrlimit(RLIMIT_NOFILE, &saved_) != 0) return;
        const long inUse = countOpenFds();
        if (inUse < 0) return;
        const rlim_t cap = static_cast<rlim_t>(inUse) + 4;  // pipe 需 2 个，余量取 4
        if (cap > saved_.rlim_max) return;                  // 限额压不下去（罕见）→ 前提探测兜底
        const rlimit capped{cap, saved_.rlim_max};
        if (::setrlimit(RLIMIT_NOFILE, &capped) != 0) return;
        applied_ = true;
        int fd;
        while ((fd = ::open("/dev/null", O_RDONLY)) != -1) held_.push_back(fd);
    }
    ~FdTableFiller() {
        for (int fd : held_) ::close(fd);
        if (applied_) ::setrlimit(RLIMIT_NOFILE, &saved_);
    }
    FdTableFiller(const FdTableFiller&) = delete;
    FdTableFiller& operator=(const FdTableFiller&) = delete;

    // 注入语义前提：此刻 pipe 仍失败（并发释放 fd 会打破注入，调用方 skip）
    bool pipeStillFails() {
        int probe[2];
        if (::pipe(probe) == -1) return true;
        ::close(probe[0]);
        ::close(probe[1]);
        return false;
    }

private:
    // /proc/self/fd 计数含 . 与 .. 与扫描自身的 DIR fd——偏大无害，
    // cap 随之偏大仅多占几个 fd，占满语义不变
    static long countOpenFds() {
        DIR* dir = ::opendir("/proc/self/fd");
        if (!dir) return -1;
        long count = 0;
        while (::readdir(dir) != nullptr) ++count;
        ::closedir(dir);
        return count;
    }

    bool applied_ = false;
    rlimit saved_{};
    std::vector<int> held_;
};

// 本 uid 现有进程数（RLIMIT_NPROC 按 uid 计数，需以 uid 总数为锚）
long countUidProcesses() {
    const uid_t self = ::getuid();
    long count = 0;
    DIR* proc = ::opendir("/proc");
    if (!proc) return -1;
    while (dirent* entry = ::readdir(proc)) {
        // 只看数字目录（进程）
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        std::ifstream status(std::string("/proc/") + entry->d_name + "/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.compare(0, 4, "Uid:") != 0) continue;
            // "Uid:\t<real>\t<effective>\t..."——real uid 命中即计
            const size_t first = line.find_first_of("0123456789");
            if (first != std::string::npos &&
                static_cast<uid_t>(std::strtol(line.c_str() + first, nullptr, 10)) == self) {
                ++count;
            }
            break;
        }
    }
    ::closedir(proc);
    return count;
}

// RLIMIT_NPROC 压制的 RAII：析构恢复
class NprocCap {
public:
    NprocCap() = default;
    bool apply(long processes) {
        if (::getrlimit(RLIMIT_NPROC, &saved_) != 0) return false;
        capped_ = rlimit{static_cast<rlim_t>(processes), saved_.rlim_max};
        if (::setrlimit(RLIMIT_NPROC, &capped_) != 0) return false;
        applied_ = true;
        return true;
    }
    // 只在成功 apply 后恢复——未 apply 时 saved_ 不可信，绝不能把限额写成垃圾值
    ~NprocCap() {
        if (applied_) ::setrlimit(RLIMIT_NPROC, &saved_);
    }
    NprocCap(const NprocCap&) = delete;
    NprocCap& operator=(const NprocCap&) = delete;

    // 注入语义前提：本进程此刻 fork 确实 EAGAIN（uid 进程数已回落则不成立）
    bool forkStillFails() {
        const pid_t pid = ::fork();
        if (pid == -1) return true;  // EAGAIN（或其他失败）注入仍有效
        if (pid == 0) ::_exit(0);    // 探测子进程：立即退出
        int status = 0;
        ::waitpid(pid, &status, 0);
        return false;                // fork 成功 → 注入失效
    }

private:
    bool applied_ = false;
    rlimit saved_{};
    rlimit capped_{};
};

} // namespace

// ① 初始化失败：坏 DISPLAY 下工厂返回未初始化实例，全接口按降级契约行事。
// 同时覆盖 x11_clipboard.cpp 的 initialize 失败分支（openX11Display 重试
// 耗尽 → spdlog error → return false）。
TEST(ClipboardBackendFaultTest, UninitializedBackendDegradesByContract) {
    DisplayOverride brokenDisplay(":9999");  // 不存在的 display 号
    auto backend = wingman::platform::linux::createX11Clipboard();
    ASSERT_NE(backend, nullptr);  // 工厂无视 initialize 成败都返回实例

    // 后端元数据如实上报未初始化
    EXPECT_FALSE(backend->getBackendInfo().isInitialized);
    EXPECT_EQ(backend->getBackendName(), "X11/xclip");

    // 写操作一律 false
    EXPECT_FALSE(backend->setText("x"));
    EXPECT_FALSE(backend->setHTML("<b>x</b>"));
    EXPECT_FALSE(backend->setImage({1, 2, 3, 4}, 1, 1));
    EXPECT_FALSE(backend->setFiles({"/tmp/wingman-fault.txt"}));  // 非空列表过空检查后仍 false

    // 读操作一律空值
    EXPECT_EQ(backend->getText(), "");
    EXPECT_FALSE(backend->hasText());
    EXPECT_EQ(backend->getHTML(), "");
    EXPECT_FALSE(backend->hasHTML());
    EXPECT_TRUE(backend->getImage(nullptr, nullptr).empty());
    EXPECT_FALSE(backend->hasImage());
    EXPECT_TRUE(backend->getFiles().empty());
    EXPECT_FALSE(backend->hasFiles());
    EXPECT_TRUE(backend->getAvailableFormats().empty());

    // 无返回值操作不崩；isEmpty 在未初始化时按降级语义为 true
    backend->clear();
    EXPECT_TRUE(backend->isEmpty());
}

// ② pipe() EMFILE：fd 表填满后 setText/getText 必须优雅失败（false / 空），
// 覆盖 x11_clipboard.cpp 两条 pipe 失败防御分支
TEST(ClipboardBackendFaultTest, PipeExhaustionFailsGracefully) {
    ClipboardLockGuard lock;  // 注入失效时 setText 可能走通成功路径 fork xclip
    FdTableFiller filler;
    if (!filler.pipeStillFails()) {
        GTEST_SKIP() << "fd 注入前提不成立（填充后 pipe 仍可用）— 环境干扰，不误报";
    }

    EXPECT_FALSE(wingman::Clipboard::setText("wingman-fd-exhausted"));
    if (filler.pipeStillFails()) {  // setText 意外成功时复核注入是否仍在
        EXPECT_EQ(wingman::Clipboard::getText(), "");
    }
}

// ③ fork() EAGAIN：RLIMIT_NPROC 压到 uid 现有进程数后 fork 失败，
// setText/getText 关闭管道两端优雅返回，覆盖两条 fork 失败防御分支
TEST(ClipboardBackendFaultTest, ForkFailureFailsGracefully) {
    ClipboardLockGuard lock;  // 同上：注入失效路径的纪律
    const long processes = countUidProcesses();
    if (processes <= 0) {
        GTEST_SKIP() << "无法统计 uid 进程数（/proc 不可读）— 跳过注入";
    }
    NprocCap cap;
    if (!cap.apply(processes)) {
        GTEST_SKIP() << "setrlimit(RLIMIT_NPROC) 不可用（容器/权限）— 跳过注入";
    }
    if (!cap.forkStillFails()) {
        GTEST_SKIP() << "fork 注入前提不成立（uid 进程数已回落）— 环境干扰，不误报";
    }

    EXPECT_FALSE(wingman::Clipboard::setText("wingman-fork-capped"));
    if (cap.forkStillFails()) {
        EXPECT_EQ(wingman::Clipboard::getText(), "");
    }
}

#endif // __linux__
