#pragma once
#if defined(__linux__)

#include <X11/Xlib.h>

#include <chrono>
#include <thread>

// 本地（Linux/X11）内部工具：XOpenDisplay 瞬态失败重试。
//
// 根因（2026-09-27 platform_x11_test flake 根治轮，strace 客户端+Xvfb 双侧实证）：
// X server 在「前一个本地客户端刚断开 → 新连接立刻到达」的窗口里，会在 accept
// 阶段直接掐断新连接——读完 SO_PEERCRED 与 /proc/<pid>/cmdline 后连 setup 请求都
// 不读就 shutdown（对应 Xorg os/access.c 的按 pid 缓存的本地凭据/ComputeLocalClient
// 路径在前一客户端断开后的清理竞态）。客户端表现就是 XOpenDisplay 返回 NULL，
// 紧随其后的再次 open 立即成功。CPU 超售放大命中概率：裸 open→close 循环在
// load≈40 实测 11/3000 失败，11/11 重试即成功；单测试（探测连接关闭后紧跟门面
// open）约 1/4 红。
//
// 这不是调用方时序错误，同一进程内任何一次 open 都可能撞上，且失败是瞬态的——
// 重试是唯一确定性修法。默认最坏 6×20ms，只在真失败路径付出。
namespace wingman::platform::linux {

inline Display* openX11Display(const char* display_name = nullptr,
                               int attempts = 6,
                               int backoff_us = 20000) {
    for (int i = 0; i < attempts; ++i) {
        if (Display* display = XOpenDisplay(display_name)) {
            return display;
        }
        if (i + 1 < attempts) {
            std::this_thread::sleep_for(std::chrono::microseconds(backoff_us));
        }
    }
    return nullptr;
}

} // namespace wingman::platform::linux

#endif // __linux__
