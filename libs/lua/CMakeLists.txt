cmake_minimum_required(VERSION 3.20)

# ========== wingman::lua ==========
# Lua 引擎 + sol2 绑定 + EmmyLua 调试器集成

# Lua 功能只在 Windows 上支持
if(NOT WIN32)
    message(STATUS "Lua bindings are Windows-only, skipping wingman_lua")
    return()
endif()

project(wingman_lua LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
# sol2 是 header-only 库，会自动链接 Lua
find_package(sol2 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

message(STATUS "Found sol2 for Lua bindings")

# ========== 源文件 ==========
# 注意: lua_bindings, lua_extensions, lua_http, debug_adapter, script_manager
# 需要额外的依赖，暂时禁用
set(LUA_SOURCES
    src/lua_engine.cpp
    # src/lua_bindings.cpp  # TODO: 需要额外的头文件
    # src/lua_extensions.cpp  # TODO: 需要额外的头文件
    # src/lua_http.cpp  # TODO: 依赖 crow.h
    # src/debug_adapter.cpp  # TODO: 依赖 emmy_adapter
    # src/script_manager.cpp  # TODO: 依赖 lua_bindings
)

# 如果源文件不存在，使用空实现
foreach(src ${LUA_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(WARNING "Source file ${src} not found, creating stub")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/${src}"
            "// Auto-generated stub for ${src}\n"
            "#include \"wingman/lua/lua_engine.hpp\"\n"
            "// TODO: Implement ${src}\n"
        )
    endif()
endforeach()

# ========== 创建库 ==========
add_library(wingman_lua STATIC ${LUA_SOURCES})

# 包含目录
target_include_directories(wingman_lua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# 链接库 - 使用 PUBLIC 确保依赖传递给使用 wingman_lua 的目标
# sol2 是 header-only，会自动处理 Lua 链接
target_link_libraries(wingman_lua PUBLIC
    sol2::sol2
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    wingman
)

# 导出库
add_library(wingman::lua ALIAS wingman_lua)

# EmmyLua Debugger 支持
option(WINGMAN_ENABLE_EMMY "Enable EmmyLua Debugger integration" ON)
if(WINGMAN_ENABLE_EMMY)
    target_compile_definitions(wingman_lua PUBLIC WINGMAN_ENABLE_EMMY)

    # EmmyLua 将在运行时从以下位置加载：
    # 1. <exe-dir>/emmy/emmy_core.dll
    # 2. WINGMAN_EMMY_PATH 环境变量
    message(STATUS "EmmyLua Debugger integration enabled")
endif()

# ========== 编译选项 ==========
target_compile_features(wingman_lua PUBLIC cxx_std_17)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman_lua PRIVATE /W4)
else()
    target_compile_options(wingman_lua PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ========== 测试 ==========
option(BUILD_LUA_TESTS "Build lua library tests" OFF)
if(BUILD_LUA_TESTS)
    add_subdirectory(tests)
endif()
