// fd 耗尽故障注入（RLIMIT_NOFILE 压限 + 占满）——供 POSIX 平台测试复用。
// 纪律与 clipboard_fault_coverage_test 相同：压限基于当前占用计数（默认
// 百万额度逐个占满是秒级窗口），前提探测不成立（socket() 仍成功）一律由
// 调用方 GTEST_SKIP，不误报为代码失败；析构 RAII 恢复限额并释放占位 fd。
// 注入语义是「进程级 fd 表占满」：并发线程释放 fd 会打破注入，跨线程注入
// 不可靠，仅在注入与触发的同一同步路径上使用。
#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <vector>

namespace wingman::testutils {

class FdTableFiller {
public:
    FdTableFiller() {
        if (::getrlimit(RLIMIT_NOFILE, &saved_) != 0) return;
        const long inUse = countOpenFds();
        if (inUse < 0) return;
        const rlim_t cap = static_cast<rlim_t>(inUse) + 4;  // 探测/护栏余量
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

    // 注入语义前提：此刻新建 socket 仍失败（并发释放 fd 会打破注入）
    bool socketStillFails(int domain = AF_UNIX, int type = SOCK_STREAM) {
        const int probe = ::socket(domain, type, 0);
        if (probe == -1) return true;
        ::close(probe);
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

} // namespace wingman::testutils
