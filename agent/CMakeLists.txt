cmake_minimum_required(VERSION 3.20)

# ========== Wingman Agent ==========
# C++ Agent - 支持主动/被动/单机三种模式

project(wingman_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Agent 源文件
set(AGENT_SOURCES
    src/main.cpp
    src/agent.cpp
    src/active_mode.cpp
    src/passive_mode.cpp
    src/standalone_mode.cpp
    src/agent_config.cpp
)

# 创建可执行文件
add_executable(wingman-agent ${AGENT_SOURCES})

# 包含目录
target_include_directories(wingman-agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 链接库
target_link_libraries(wingman-agent PRIVATE
    wingman::proto
    wingman::transport
    wingman::core
    wingman::lua
    wingman::debug
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# 复制配置文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/agent.toml
    ${CMAKE_BINARY_DIR}/agent.toml
    COPYONLY
)

# Windows 特定配置
if(WIN32)
    # 设置为 Windows 应用程序（可选）
    # set_target_properties(wingman-agent PROPERTIES WIN32_EXECUTABLE ON)

    # 添加资源文件
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/wingman.rc")
        target_sources(wingman-agent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/wingman.rc")
    endif()
endif()

# ========== 子模块测试 ==========

option(WINGMAN_BUILD_AGENT_TESTS "Build Agent tests" OFF)
if(WINGMAN_BUILD_AGENT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
