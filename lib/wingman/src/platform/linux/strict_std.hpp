#pragma once

// `linux`（以及 `unix`）是 GCC/Clang 在非 strict 模式（gnu++*）下的预定义宏，
// 会把 namespace wingman::platform::linux 预处理成非法的 `namespace ...::1`。
// 本库要求 strict 标准模式：CMake 侧已设 CMAKE_CXX_EXTENSIONS OFF（-std=c++23），
// 若此处的 #error 触发，说明当前编译启用了 GNU 扩展模式。
// 规范依据：docs/platform-abstraction-design.md §8.2。
#ifdef linux
#error "wingman requires strict -std (e.g. -std=c++23 / CMAKE_CXX_EXTENSIONS OFF): 'linux' is a predefined macro under gnu++* that breaks namespace wingman::platform::linux"
#endif
