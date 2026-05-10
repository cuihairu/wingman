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

# vcpkg 的 Lua 包需要特殊处理
# 创建 imported target 用于 Lua 库
if(NOT TARGET lua_static)
    if(LUA_LIBRARIES)
        # 尝试将 LUA_LIBRARIES 作为路径
        if(EXISTS "${LUA_LIBRARIES}")
            add_library(lua_static STATIC IMPORTED)
            set_target_properties(lua_static PROPERTIES
                IMPORTED_LOCATION "${LUA_LIBRARIES}"
                INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
            )
            message(STATUS "Created lua_static target from LUA_LIBRARIES: ${LUA_LIBRARIES}")
        else()
            # LUA_LIBRARIES 可能是 target 名称或库列表，尝试查找实际的 .lib 文件
            # 在 vcpkg x64-windows-static 中，Lua 库通常位于 installed/x64-windows-static/lib
            find_file(LUA_LIB_FILE
                NAMES "lua55.lib" "lua54.lib" "lua5.5.lib" "lua5.4.lib" "lua.lib"
                PATHS "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows-static/lib"
                  "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
                  "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib"
                NO_DEFAULT_PATH
            )
            if(LUA_LIB_FILE)
                add_library(lua_static STATIC IMPORTED)
                set_target_properties(lua_static PROPERTIES
                    IMPORTED_LOCATION "${LUA_LIB_FILE}"
                    INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
                )
                message(STATUS "Created lua_static target from found library: ${LUA_LIB_FILE}")
            else()
                message(FATAL_ERROR "Could not find Lua library file. Searched for lua*.lib in vcpkg lib directories.")
            endif()
        endif()
    else()
        message(FATAL_ERROR "LUA_LIBRARIES is empty. Lua package not found.")
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
    ${LUA_INCLUDE_DIR}
)

# 链接库 - 使用 PUBLIC 确保依赖传递给使用 wingman_lua 的目标
target_link_libraries(wingman_lua PUBLIC
    lua_static
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    wingman
)

# 确保 Lua 头文件目录被正确传播
if(LUA_INCLUDE_DIR)
    target_include_directories(wingman_lua PUBLIC ${LUA_INCLUDE_DIR})
endif()

# 导出库
add_library(wingman::lua ALIAS wingman_lua)

# 创建 lua_static 的全局 ALIAS，以便其他目标可以访问
add_library(wingman::lua_static ALIAS lua_static)

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
