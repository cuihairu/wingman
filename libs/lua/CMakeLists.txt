cmake_minimum_required(VERSION 3.20)

# ========== wingman::lua ==========
# Lua 引擎 + 绑定 + EmmyLua 调试器集成

project(wingman_lua LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Lua REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ========== 源文件 ==========
set(LUA_SOURCES
    src/lua_engine.cpp
    src/lua_bindings.cpp
    src/lua_extensions.cpp
    src/lua_http.cpp
    src/debug_adapter.cpp
    src/script_manager.cpp
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
add_library(wingman::lua STATIC ${LUA_SOURCES})

# 包含目录
target_include_directories(wingman::lua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${LUA_INCLUDE_DIR}
)

# 链接库
target_link_libraries(wingman::lua PUBLIC
    ${LUA_LIBRARIES}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    wingman::core
)

# EmmyLua Debugger 支持
option(WINGMAN_ENABLE_EMMY "Enable EmmyLua Debugger integration" ON)
if(WINGMAN_ENABLE_EMMY)
    target_compile_definitions(wingman::lua PUBLIC WINGMAN_ENABLE_EMMY)

    # EmmyLua 将在运行时从以下位置加载：
    # 1. <exe-dir>/emmy/emmy_core.dll
    # 2. WINGMAN_EMMY_PATH 环境变量
    message(STATUS "EmmyLua Debugger integration enabled")
endif()

# 导出库
add_library(wingman::lua ALIAS wingman::lua)

# ========== 编译选项 ==========
target_compile_features(wingman::lua PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(wingman::lua PRIVATE /W4 /utf-8)
else()
    target_compile_options(wingman::lua PRIVATE -Wall -Wextra -Wpedantic)
endif()
