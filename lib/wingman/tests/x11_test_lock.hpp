#pragma once

// X11 窗口测试的跨进程串行化守卫。
//
// 窗口枚举依赖根窗口的 EWMH 属性（_NET_CLIENT_LIST / _NET_ACTIVE_WINDOW），
// 它们是 X server 全局状态：ctest -j 并行跑窗口用例时，各测试进程各自写同一
// 根属性会互相覆盖（枚举结果/前台窗口断言被插队）。焦点等 server 级状态同理。
//
// 用 flock 锁文件把「触碰 X server 全局窗口状态」的测试段在所有测试进程间
// 串行化（与 clipboard_lock_guard.hpp 同一模式，锁面不同——剪贴板锁 selection
// 所有权，本守卫锁根属性/焦点）。Windows 下无此测试面，为空实现。

#if defined(_WIN32)

class X11ServerLockGuard {};

#else

#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

class X11ServerLockGuard {
public:
    X11ServerLockGuard() {
        fd_ = ::open("/tmp/wingman_test_x11_window.lock", O_RDONLY | O_CREAT, 0666);
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

    ~X11ServerLockGuard() {
        if (fd_ != -1) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }

    X11ServerLockGuard(const X11ServerLockGuard&) = delete;
    X11ServerLockGuard& operator=(const X11ServerLockGuard&) = delete;

private:
    int fd_ = -1;
};

#endif // _WIN32
