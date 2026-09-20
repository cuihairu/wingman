# Android NDK 构建：libwingman_agent.so（A1，docs/android-agent-design.md §6.2）
#
# 由 AGP externalNativeBuild 调起，vcpkg 工具链经 gradle 参数叠加：
#   -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#   -DVCPKG_TARGET_TRIPLET=arm64-android
# 依赖（vcpkg arm64-android：asio lua sol2 spdlog nlohmann-json）与桌面同源，
# 唯一的仓库组件来自三个目录（均不依赖 lib/wingman）：
#   libs/transport                      帧协议 + 出站 TCP 客户端
#   apps/runtime/src/remote_client.cpp  重连/outbox/心跳/命令分发（桌面同源）
#   apps/android/cpp/agent              ScriptRunner + AndroidAgent（桌面同源单测）

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

# transport（独立库自带 platform 薄层，§8.4；Android 走 socket_compat POSIX 分支）
set(BUILD_TRANSPORT_TESTS OFF)
add_subdirectory("${WINGMAN_REPO_ROOT}/libs/transport" transport_build)

add_library(wingman_agent SHARED
    jni_bridge.cpp
    agent/android_agent.cpp
    agent/script_runner.cpp
    # 桌面 runtime 的远程链路（重连/心跳/outbox/命令分发），Android 同源复用
    "${WINGMAN_REPO_ROOT}/apps/runtime/src/remote_client.cpp"
    "${WINGMAN_REPO_ROOT}/apps/runtime/src/event_buffer.cpp"
)

target_include_directories(wingman_agent PRIVATE
    "${WINGMAN_REPO_ROOT}/apps/runtime/include"
)

target_link_libraries(wingman_agent PRIVATE
    wingman::transport
    sol2::sol2
    unofficial::lua::lua
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

target_compile_options(wingman_agent PRIVATE -Wall -Wextra)
