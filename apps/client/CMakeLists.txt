cmake_minimum_required(VERSION 3.20)

# ========== Wingman Client ==========
# C++ Client - 支持主动/被动/单机三种模式

project(wingman_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Client 源文件
set(CLIENT_SOURCES
    src/main.cpp
    src/agent.cpp
    src/active_mode.cpp
    src/passive_mode.cpp
    src/standalone_mode.cpp
    src/agent_config.cpp
)

# GUI 支持
if(WINGMAN_BUILD_GUI)
    find_package(imgui CONFIG REQUIRED)
    list(APPEND CLIENT_SOURCES
        src/gui/app.cpp
        src/gui/config.cpp
        src/gui/file_dialog.cpp
        src/gui/imgui_backend.cpp
        src/gui/spdlog_sink.cpp
        src/gui/panels/log_panel.cpp
        src/gui/panels/preview_panel.cpp
        src/gui/panels/script_panel.cpp
        src/gui/panels/trigger_panel.cpp
    )
    message(STATUS "Client GUI enabled")
endif()

# 创建可执行文件
add_executable(wingman-client ${CLIENT_SOURCES})

# 包含目录
target_include_directories(wingman-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 链接库
target_link_libraries(wingman-client PRIVATE
    wingman::proto
    wingman::transport
    wingman
    wingman::lua
    wingman::debug
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# GUI 链接
if(WINGMAN_BUILD_GUI)
    target_link_libraries(wingman-client PRIVATE imgui::imgui d3d12 dxgi)
    target_include_directories(wingman-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/wingman/client/gui)
endif()

# 复制配置文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/client.toml
    ${CMAKE_BINARY_DIR}/client.toml
    COPYONLY
)

# Windows 特定配置
if(WIN32)
    # 添加资源文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/wingman.rc")
        target_sources(wingman-client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/wingman.rc")
    endif()
endif()

# ========== 子模块测试 ==========

option(WINGMAN_BUILD_CLIENT_TESTS "Build Client tests" OFF)
if(WINGMAN_BUILD_CLIENT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
