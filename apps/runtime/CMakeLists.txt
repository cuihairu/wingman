cmake_minimum_required(VERSION 3.20)

# ========== Wingman Runtime ==========
# CLI runtime agent, connects outbound to orchestrator

project(wingman_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Runtime 源文件
set(RUNTIME_SOURCES
    src/cli.cpp
    src/main.cpp
    src/agent.cpp
    src/standalone_mode.cpp
    src/notify_bridge.cpp
    src/local_ipc_server.cpp
    src/agent_config.cpp
    src/packer.cpp
    src/resource_loader.cpp
    src/runtime_context.cpp
    src/crash_setup.cpp
    # 命令实现
    src/commands/start_command.cpp
    src/commands/stop_command.cpp
    src/commands/script_command.cpp
    src/commands/build_command.cpp
    src/commands/crash_test_command.cpp
    # RPC handlers
    src/rpc/handlers/script_handler.cpp
    src/rpc/handlers/system_handler.cpp
    src/rpc/handlers/config_handler.cpp
    src/rpc/handlers/screenshot_handler.cpp
    src/rpc/handlers/event_handler.cpp
    src/rpc/handlers/macro_handler.cpp
)

# 创建可执行文件
add_executable(wingman-runtime ${RUNTIME_SOURCES})

# 包含目录
target_include_directories(wingman-runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${WINGMAN_GENERATED_DIR}
)

# 链接库
target_link_libraries(wingman-runtime PRIVATE
    wingman::agentcore
    wingman::transport
    wingman
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
)

if(TARGET wingman::lua)
    target_link_libraries(wingman-runtime PRIVATE wingman::lua)
    target_compile_definitions(wingman-runtime PRIVATE WINGMAN_HAS_LUA)
endif()

# Crashpad 崩溃采集：链接封装库（WINGMAN_HAS_CRASHPAD 门随其传递），
# handler 可执行文件随产物分发——与本可执行文件同目录（crash_setup 按
# argv[0] 同目录解析）。
if(TARGET wingman::crash)
    target_link_libraries(wingman-runtime PRIVATE wingman::crash)
    add_custom_command(TARGET wingman-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:crashpad_handler>
            $<TARGET_FILE_DIR:wingman-runtime>/crashpad_handler
        COMMENT "Copying crashpad_handler next to wingman-runtime"
        VERBATIM)
endif()

if(WINGMAN_HAS_OPENCV)
    target_compile_definitions(wingman-runtime PRIVATE WINGMAN_ENABLE_VISION)
endif()

# Python 引擎（可选）
if(WINGMAN_ENABLE_PYTHON)
    target_link_libraries(wingman-runtime PRIVATE wingman::python)
endif()

# 复制配置文件到输出目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/agent.toml
    ${CMAKE_BINARY_DIR}/agent.toml
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
