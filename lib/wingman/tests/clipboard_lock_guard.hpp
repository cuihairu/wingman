#pragma once

// 剪贴板测试的跨进程串行化守卫。
//
// 剪贴板是全局共享资源：Windows 的 openClipboard 自带 OS 级互斥，多进程并行
// 测试天然串行；X11 selection 没有等价锁——ctest -j 并行跑剪贴板用例时，
// 各进程 fork 的 xclip daemon 竞争 CLIPBOARD 所有权，写→读断言会被其他进程
// 的写入插队（Linux 首次以 -j4 跑 X11 后端时实测 5 例竞态失败）。
//
// 此守卫用 flock 锁文件把「触碰剪贴板」的测试段在所有测试进程间串行化，
// 语义对齐 Windows 侧的隐式锁。Windows 下为空实现。

#if defined(_WIN32)

class ClipboardLockGuard {};

#else

#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

class ClipboardLockGuard {
public:
    ClipboardLockGuard() {
        fd_ = ::open("/tmp/wingman_test_clipboard.lock", O_RDONLY | O_CREAT, 0666);
        if (fd_ == -1) {
            return;  // 锁文件不可用时退化为无锁（不因测试基建放大失败）
        }
        while (::flock(fd_, LOCK_EX) != 0) {
            if (errno != EINTR) {
                ::close(fd_);
                fd_ = -1;
                return;
            }
        }
    }

    ~ClipboardLockGuard() {
        if (fd_ != -1) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }

    ClipboardLockGuard(const ClipboardLockGuard&) = delete;
    ClipboardLockGuard& operator=(const ClipboardLockGuard&) = delete;

private:
    int fd_ = -1;
};

#endif // _WIN32
