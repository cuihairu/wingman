cmake_minimum_required(VERSION 3.20)

# ========== Wingman Client ==========
# 无 UI 的命令行客户端，使用 Drogon 提供 WebSocket API

project(wingman_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖库
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
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

# Drogon HTTP 框架
find_package(Drogon CONFIG REQUIRED)

# 修复 Jsoncpp_lib 不正确的 debug/release 库链接
# Drogon 的 FindJsoncpp.cmake 没有正确处理多配置生成
if(TARGET Jsoncpp_lib)
    # 重新定义 Jsoncpp_lib 以正确处理 debug/release
    get_target_property(_old_libs Jsoncpp_lib INTERFACE_LINK_LIBRARIES)
    if(NOT _old_libs MATCHES "\\$<\\$")
        # 查找 debug 和 release 版本的库
        find_library(JSONCPP_LIB_RELEASE NAMES jsoncpp PATHS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib" NO_DEFAULT_PATH)
        find_library(JSONCPP_LIB_DEBUG NAMES jsoncpp PATHS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/debug/lib" NO_DEFAULT_PATH)

        if(JSONCPP_LIB_RELEASE AND JSONCPP_LIB_DEBUG)
            # 使用生成器表达式：debug 配置用 debug 库，其他用 release 库
            set_target_properties(Jsoncpp_lib PROPERTIES
                INTERFACE_LINK_LIBRARIES "$<$<CONFIG:DEBUG,RELWITHDEBINFO>:${JSONCPP_LIB_DEBUG}>$<$<NOT:$<CONFIG:DEBUG,RELWITHDEBINFO>>:${JSONCPP_LIB_RELEASE}>"
            )
        endif()
    endif()
endif()

# Lua 库通过 wingman::lua 依赖自动链接

# Client 源文件
set(CLIENT_SOURCES
    src/main.cpp
    src/agent.cpp
    src/remote_client.cpp
    src/remote_server.cpp
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
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    clasp::clasp
    Drogon::Drogon
)

# Windows-only 库
if(WIN32 AND TARGET wingman::lua)
    target_link_libraries(wingman-runtime PRIVATE wingman::lua)
endif()
if(WIN32 AND TARGET wingman::debug)
    target_link_libraries(wingman-runtime PRIVATE wingman::debug)
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
