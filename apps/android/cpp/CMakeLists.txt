# Android NDK 构建：libwingman_agent.so（A1 §6.2；A2 能力闭环 §5.6）
#
# 由 AGP externalNativeBuild 调起，vcpkg 工具链经 gradle 参数叠加：
#   -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#   -DVCPKG_TARGET_TRIPLET=arm64-android
# 依赖（vcpkg arm64-android：asio lua sol2 spdlog nlohmann-json opencv4）与
# 桌面同源。本目录只保留 Android 壳（JNI 桥 + agent 装配 + 截图命令）：
#   libs/agentcore                       远程链路（桌面 runtime 同源链接）
#   libs/androidagent                    ScriptRunner + 脚本能力 API + core 子集
#   libs/transport                       帧协议 + 出站 TCP 客户端

cmake_minimum_required(VERSION 3.22)
project(wingman_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 仓库根（gradle 传入；缺省按 apps/android/cpp 的三层上级推算）
if(NOT WINGMAN_REPO_ROOT)
    get_filename_component(WINGMAN_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

find_package(sol2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-lua CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

# vcpkg 的 spdlog port 在 INTERFACE_COMPILE_DEFINITIONS 硬编码
# SPDLOG_FWRITE_UNLOCKED（glibc 扩展 ::fwrite_unlocked），bionic 没有
# 该函数——Android 上剥离宏，spdlog 回退到 std::fwrite（语义等价）
foreach(_spd_target spdlog::spdlog spdlog::spdlog_header_only)
    if(TARGET ${_spd_target})
        get_target_property(_spd_defs ${_spd_target} INTERFACE_COMPILE_DEFINITIONS)
        if(_spd_defs)
            list(REMOVE_ITEM _spd_defs SPDLOG_FWRITE_UNLOCKED)
            set_target_properties(${_spd_target} PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${_spd_defs}")
        endif()
    endif()
endforeach()

# transport（独立库自带 platform 薄层，§8.4；Android 走 socket_compat POSIX 分支）
# agentcore 断言其先于自身出现
set(BUILD_TRANSPORT_TESTS OFF)
add_subdirectory("${WINGMAN_REPO_ROOT}/libs/transport" transport_build)
add_subdirectory("${WINGMAN_REPO_ROOT}/libs/agentcore" agentcore_build)
add_subdirectory("${WINGMAN_REPO_ROOT}/libs/androidagent" androidagent_build)

add_library(wingman_agent SHARED
    jni_bridge.cpp
    agent/android_agent.cpp
    agent/android_screenshot.cpp
)

target_include_directories(wingman_agent PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(wingman_agent PRIVATE
    wingman::agentcore
    wingman::androidagent
    wingman::transport
    sol2::sol2
    unofficial::lua::lua
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
)

target_compile_options(wingman_agent PRIVATE -Wall -Wextra)
