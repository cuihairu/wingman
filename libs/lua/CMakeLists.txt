cmake_minimum_required(VERSION 3.20)

# ========== wingman::lua ==========
# Lua 引擎 + 绑定 + EmmyLua 调试器集成

project(wingman_lua LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
# 注意：Lua 链接问题待修复，暂时跳过 Lua 相关代码
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 暂时禁用 Lua 库链接
# TODO: 修复 Lua 库在 vcpkg x64-windows-static 中的链接问题
option(WINGMAN_ENABLE_LUA "Enable Lua scripting support" OFF)

if(WINGMAN_ENABLE_LUA)
    # 查找 Lua 库
    find_package(unofficial-lua CONFIG QUIET)
    if(TARGET unofficial::lua::lua-cpp)
        set(LUA_TARGET unofficial::lua::lua-cpp)
        message(STATUS "Using Lua C++ library: unofficial::lua::lua-cpp")
    elseif(TARGET unofficial::lua::lua)
        set(LUA_TARGET unofficial::lua::lua)
        message(STATUS "Using Lua library: unofficial::lua::lua")
    else()
        message(WARNING "Lua library not found, Lua support will be disabled")
        set(WINGMAN_ENABLE_LUA OFF)
    endif()
endif()

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
target_link_libraries(wingman_lua PUBLIC
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    wingman
)

# 条件性链接 Lua 库
if(WINGMAN_ENABLE_LUA AND LUA_TARGET)
    target_link_libraries(wingman_lua PUBLIC ${LUA_TARGET})
    target_include_directories(wingman_lua PUBLIC ${LUA_INCLUDE_DIR})
    target_compile_definitions(wingman_lua PUBLIC WINGMAN_ENABLE_LUA)
endif()

# 导出库
add_library(wingman::lua ALIAS wingman_lua)

# 创建 Lua 库的全局 ALIAS（如果启用）
if(WINGMAN_ENABLE_LUA AND LUA_TARGET)
    add_library(wingman::lua_lib ALIAS ${LUA_TARGET})
endif()

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
