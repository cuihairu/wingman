cmake_minimum_required(VERSION 3.20)

# ========== Wingman Runtime ==========
# CLI runtime agent, connects outbound to orchestrator

project(wingman_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
include(${CMAKE_SOURCE_DIR}/cmake/EnsureSpdlog.cmake)
wingman_ensure_spdlog()
include(${CMAKE_SOURCE_DIR}/cmake/EnsureNlohmannJson.cmake)
wingman_ensure_nlohmann_json()
find_package(clasp CONFIG QUIET)  # Clasp 命令行库（可选）

# 如果找不到 clasp，使用源码树中的本地副本或 FetchContent
if(NOT clasp_FOUND)
    # 尝试使用本地 libs/clasp（绕过 git 镜像问题）
    # 使用 CMAKE_CURRENT_SOURCE_DIR 从 apps/runtime 定位到根目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/clasp/CMakeLists.txt")
        message(STATUS "clasp not found via vcpkg, using local libs/clasp")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../libs/clasp" "${CMAKE_BINARY_DIR}/libs/clasp" EXCLUDE_FROM_ALL)
        set(clasp_FOUND TRUE)
        # 添加别名，保持 target_link_libraries 中的 clasp::clasp 引用有效
        if(NOT TARGET clasp::clasp AND TARGET clasp)
            add_library(clasp::clasp ALIAS clasp)
        endif()
    else()
        message(STATUS "clasp not found via vcpkg or local, using FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            clasp_repo
            GIT_REPOSITORY https://github.com/cuihairu/clasp.git
            GIT_TAG main
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(clasp_repo)
        set(clasp_FOUND TRUE)
    endif()
endif()

# Runtime 源文件
set(RUNTIME_SOURCES
    src/main.cpp
    src/agent.cpp
    src/remote_client.cpp
    src/remote_server.cpp
    src/standalone_mode.cpp
    src/agent_config.cpp
    src/packer.cpp
    src/resource_loader.cpp
    src/runtime_context.cpp
    # 命令实现
    src/commands/start_command.cpp
    src/commands/stop_command.cpp
    src/commands/script_command.cpp
    src/commands/build_command.cpp
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
    wingman::proto
    wingman::transport
    wingman
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    clasp::clasp
)

if(TARGET wingman::lua)
    target_link_libraries(wingman-runtime PRIVATE wingman::lua)
    target_compile_definitions(wingman-runtime PRIVATE WINGMAN_HAS_LUA)
endif()

if(TARGET wingman::debug)
    target_link_libraries(wingman-runtime PRIVATE wingman::debug)
    target_compile_definitions(wingman-runtime PRIVATE WINGMAN_HAS_DEBUG)
endif()

# Python 引擎（可选）
if(WINGMAN_ENABLE_PYTHON)
    target_link_libraries(wingman-runtime PRIVATE wingman::python)
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
        target_sources(wingman-runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/wingman.rc")
    endif()
endif()

# ========== 子模块测试 ==========

option(WINGMAN_BUILD_CLIENT_TESTS "Build Client tests" OFF)
if(WINGMAN_BUILD_CLIENT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
