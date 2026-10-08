# Crashpad 构建接入（crashpad-cmake 包装，钉版子模块直供源码）。
#
# 不走包装仓根 CMakeLists 的 FetchContent 路径（其 GIT_REPOSITORY 会在配置期触网，
# 且单参 FetchContent_Populate 在 CMake 4.x 已弃用），改为直接设置其模块文件
# 依赖的 *_git_SOURCE_DIR 变量后 include 模块层，与包装仓根文件的包含顺序一致。
# 子模块钉版见 .gitmodules；升级走子模块指针更新。

find_package(ZLIB 1.2.8 REQUIRED)  # vcpkg manifest 提供
find_package(Threads REQUIRED)

# 非 MSVC 平台的 .S 源（Linux ELF note / mac capture context）需要 ASM 语言；
# 未启用时 CMake 会把 .S 当头文件静默跳过，链接期才炸 undefined reference。
if(NOT MSVC)
    enable_language(ASM)
endif()

# 包装仓根文件在 APPLE 下填充框架，其余平台保持空接口目标。
add_library(AppleFrameworks INTERFACE)

set(mini_chromium_git_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mini_chromium")
set(crashpad_git_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad")
set(lss_git_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lss")

# lss 头文件拷入 crashpad 树（等价于上游 gn 构建的 dependency 拷贝步骤）。
file(COPY "${lss_git_SOURCE_DIR}/linux_syscall_support.h"
     DESTINATION "${crashpad_git_SOURCE_DIR}/third_party/lss")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/crashpad-cmake/cmake")

include(crashpad-common)
include(minichromium)
include(crashpad-compat)
include(crashpad-tools)
include(crashpad-util)
include(crashpad-client)
include(crashpad-minidump)
include(crashpad-snapshot)
include(crashpad-handler)

# 钉版源码冻结于 2021，两条对新工具链的豁免（GCC 15 已验证需要）：
# 1) -Werror 会把新版编译器的新增告警变成硬错，第三方代码告警不作为本仓构建闸门；
# 2) GCC 13+ 不再传递包含 <cstdint>，钉版 chromium 系代码普遍缺失该 include
#    （限定 CXX：-include cstdint 传给 .S 汇编编译会因无 C++ 头路径而报错）。
if(NOT MSVC)
    target_compile_options(crashpad_common INTERFACE
        -Wno-error
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>"
    )
endif()
