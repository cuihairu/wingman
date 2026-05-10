cmake_minimum_required(VERSION 3.20)

# ========== Wingman Client ==========
# 无 UI 的命令行客户端，使用 Drogon 提供 WebSocket API

project(wingman_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(clasp CONFIG REQUIRED)  # Clasp 命令行库

# Drogon HTTP 框架
find_package(Drogon CONFIG REQUIRED)

# Client 源文件
set(CLIENT_SOURCES
    src/main.cpp
    src/agent.cpp
    src/active_mode.cpp
    src/passive_mode.cpp
    src/standalone_mode.cpp
    src/agent_config.cpp
    src/packer.cpp
    src/resource_loader.cpp
    # Drogon 控制器
    src/controllers/websocket.cpp
    src/controllers/script.cpp
    src/controllers/system.cpp
    src/controllers/api.cpp
    # 命令实现
    src/commands/start_command.cpp
    src/commands/stop_command.cpp
    src/commands/script_command.cpp
    src/commands/build_command.cpp
    src/commands/serve_command.cpp
)

# 创建可执行文件
add_executable(wingman-runtime ${CLIENT_SOURCES})

# 包含目录
target_include_directories(wingman-runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WINGMAN_GENERATED_DIR}
)

# 链接库
target_link_libraries(wingman-runtime PRIVATE
    wingman::proto
    wingman::transport
    wingman
    wingman::lua
    wingman::debug
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    clasp::clasp
    Drogon::Drogon
)

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
        target_sources(wingman-runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/wingman.rc")
    endif()
endif()

# ========== 子模块测试 ==========

option(WINGMAN_BUILD_CLIENT_TESTS "Build Client tests" OFF)
if(WINGMAN_BUILD_CLIENT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
