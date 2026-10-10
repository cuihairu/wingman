#pragma once

// 不透明窗口句柄 ↔ 原生 HWND 互转（唯一收口点）。
// platform::WindowHandle 跨平台统一为 uint64_t 后，所有 Win32 API 调用前
// 必须经此中转：HWND 是不透明指针，经 uintptr_t 中转再转整型，避免
// 指针↔整型直接 reinterpret_cast（MSVC 窄化告警族）并保留 64 位下句柄
// 全部位。opaqueHwnd 反向同理。

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdint>

#include "wingman/platform/platform_types.hpp"

namespace wingman::platform::win {

inline HWND nativeHwnd(WindowHandle handle) {
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(handle));
}

inline WindowHandle opaqueHwnd(HWND hwnd) {
    return static_cast<WindowHandle>(reinterpret_cast<uintptr_t>(hwnd));
}

} // namespace wingman::platform::win
