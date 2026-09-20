# AGP 的 CMAKE_TOOLCHAIN_FILE 入口：vcpkg + NDK 工具链叠加。
#
# vcpkg 的 toolchain 从不自动加载 NDK 的 android.toolchain.cmake
# （官方 triplet 也不设 VCPKG_CHAINLOAD_TOOLCHAIN_FILE，需使用方自行
# 接线，见 vcpkg 官方文档 Android 页的 helper 模式）。漏接的后果是
# NDK 变量全部失效、无 sysroot、CMake 不产出 File API toolchains
# 对象，AGP 解析 reply 时报 "sysroot has not been initialized"。
#
# 顺序关键：必须先设 VCPKG_CHAINLOAD_TOOLCHAIN_FILE 再 include
# vcpkg.cmake（后者在加载段直接 include 该变量指向的文件）。

if(NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
    # AGP 经 -DANDROID_NDK=<路径> 传入 NDK；命令行兜底环境变量
    if(DEFINED ANDROID_NDK)
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    else()
        message(FATAL_ERROR "vcpkg-android.cmake: 需要 ANDROID_NDK（AGP 自动传入）或环境变量 ANDROID_NDK_HOME")
    endif()
endif()

if(NOT DEFINED WINGMAN_VCPKG_ROOT)
    if(DEFINED ENV{VCPKG_ROOT})
        set(WINGMAN_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    else()
        message(FATAL_ERROR "vcpkg-android.cmake: 需要 -DWINGMAN_VCPKG_ROOT=<vcpkg 安装根>（gradle.properties 的 wingmanVcpkgRoot）或环境变量 VCPKG_ROOT")
    endif()
endif()

# vcpkg.cmake 只向 try_compile 传播其固定变量清单，自定义变量必须
# 显式登记，否则 try_compile 重跑本文件时 WINGMAN_VCPKG_ROOT 丢失
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES WINGMAN_VCPKG_ROOT)

include("${WINGMAN_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
