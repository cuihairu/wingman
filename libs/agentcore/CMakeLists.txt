# ========== wingman::agentcore ==========
#
# Agent 链路核心（transport 之上、应用之下的共享层）：
#   - RemoteClient      出站 TCP 长连接：注册/心跳/重连退避/有界 outbox/命令分发
#   - RemoteClientConfig 远程链路配置（自 apps/runtime/config.hpp 下沉）
#   - EventBuffer       本地 UI 事件缓冲（events.drain pull 模型）
#
# 消费方：apps/runtime（桌面）与 apps/android（NDK，C++ 核心源码级子集）。
# 依赖约束：不得依赖 lib/wingman 本体（Android 构建不引入 core，见
# apps/android/cpp/CMakeLists.txt），仅允许 transport + vcpkg 三方库。
# 命名空间保持 wingman::runtime（与文档/既有调用方一致）。

cmake_minimum_required(VERSION 3.20)
project(wingman_agentcore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# transport 需由消费方先 add_subdirectory（根 CMakeLists 与
# apps/android/cpp 均已如此）；此处只断言目标存在。
if(NOT TARGET wingman_transport)
    message(FATAL_ERROR "wingman_agentcore requires wingman_transport target; add_subdirectory(libs/transport) first")
endif()

add_library(wingman_agentcore STATIC
    src/remote_client.cpp
    src/event_buffer.cpp
)
add_library(wingman::agentcore ALIAS wingman_agentcore)

target_include_directories(wingman_agentcore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(wingman_agentcore PUBLIC
    wingman::transport
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

target_compile_features(wingman_agentcore PUBLIC cxx_std_23)
