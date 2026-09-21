# Android NDK 构建：libwingman_agent.so（A1 §6.2；A2 能力闭环 §5.6）
#
# 由 AGP externalNativeBuild 调起，vcpkg 工具链经 gradle 参数叠加：
#   -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#   -DVCPKG_TARGET_TRIPLET=arm64-android
# 依赖（vcpkg arm64-android：asio lua sol2 spdlog nlohmann-json opencv4）与
# 桌面同源。仓库组件 cherry-pick 自三个目录：
#   libs/transport                      帧协议 + 出站 TCP 客户端
#   apps/runtime/src/remote_client.cpp  重连/outbox/心跳/命令分发（桌面同源）
#   apps/android/cpp/agent              ScriptRunner + AndroidAgent（桌面同源单测）
#   lib/wingman cherry-pick 子集（A2，不整体 add_subdirectory——本目录外的
#   find_package(OpenSSL REQUIRED) 等重量依赖不可用）：
#     src/bitmap.cpp                    Bitmap 纯核心（零平台宏）
#     src/screen.cpp                    Bitmap::fromFile/save（vision 分支走 OpenCV）
#     src/vision/image_analyzer.cpp     找色找图（bitmap-first，无 OpenCV 依赖）
#     src/platform/android/*            宿主桥抽象 + MediaProjection 采集源

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
set(BUILD_TRANSPORT_TESTS OFF)
add_subdirectory("${WINGMAN_REPO_ROOT}/libs/transport" transport_build)

add_library(wingman_agent SHARED
    jni_bridge.cpp
    agent/android_agent.cpp
    agent/script_runner.cpp
    agent/android_script_api.cpp
    agent/android_screenshot.cpp
    # 桌面 runtime 的远程链路（重连/心跳/outbox/命令分发），Android 同源复用
    "${WINGMAN_REPO_ROOT}/apps/runtime/src/remote_client.cpp"
    "${WINGMAN_REPO_ROOT}/apps/runtime/src/event_buffer.cpp"
    # lib/wingman cherry-pick 子集（A2，见文件头注释）
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/bitmap.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/screen.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/vision/image_analyzer.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_host_bridge.cpp"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src/platform/android/android_capture.cpp"
)

target_include_directories(wingman_agent PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${WINGMAN_REPO_ROOT}/apps/runtime/include"
    "${WINGMAN_REPO_ROOT}/lib/wingman/include"
    "${WINGMAN_REPO_ROOT}/lib/wingman/src"
)

target_compile_definitions(wingman_agent PRIVATE WINGMAN_ENABLE_VISION)

target_link_libraries(wingman_agent PRIVATE
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
