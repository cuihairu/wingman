cmake_minimum_required(VERSION 3.20)

# ========== wingman::debug ==========
# 调试器适配层
# - Lua: EmmyLua Debugger
# - Python: debugpy (planned)

project(wingman_debug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 调试器源文件
set(DEBUG_SOURCES)

# Lua 支持
if(WINGMAN_BUILD_LUA)
    find_package(unofficial-lua CONFIG REQUIRED)
    list(APPEND DEBUG_SOURCES src/emmy_adapter.cpp)
endif()

# Python 支持 (TODO: implement debugpy adapter)
if(WINGMAN_ENABLE_PYTHON)
    # find_package(pybind11 CONFIG REQUIRED)
    # find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    # list(APPEND DEBUG_SOURCES src/debugpy_adapter.cpp)
    message(STATUS "Python debugging support: planned (debugpy adapter not yet implemented)")
endif()

# 至少需要一种脚本引擎支持
if(NOT WINGMAN_BUILD_LUA AND NOT WINGMAN_ENABLE_PYTHON)
    message(FATAL_ERROR "wingman_debug requires at least one scripting engine (Lua or Python)")
endif()

# 创建库
add_library(wingman_debug STATIC ${DEBUG_SOURCES})

target_include_directories(wingman_debug PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 根据支持的脚本引擎添加编译定义
if(WINGMAN_BUILD_LUA)
    target_compile_definitions(wingman_debug PUBLIC WINGMAN_ENABLE_EMMY)
endif()

# 链接 Lua 库
if(WINGMAN_BUILD_LUA)
    target_link_libraries(wingman_debug PRIVATE unofficial::lua::lua)
endif()

# EmmyLua Debugger 路径配置
# EmmyLua 将在运行时从以下位置加载：
# 1. 可执行文件目录/emmy/
# 2. WINGMAN_EMMY_PATH 环境变量
# 3. 编译时指定的路径

if(WINGMAN_BUILD_LUA)
    target_compile_definitions(wingman_debug PUBLIC
        WINGMAN_EMMY_VERSION="0.5.3"
    )
endif()

target_compile_features(wingman_debug PUBLIC cxx_std_23)

# 导出库
add_library(wingman::debug ALIAS wingman_debug)

# ========== 测试 ==========
option(BUILD_DEBUG_TESTS "Build debug library tests" OFF)
if(BUILD_DEBUG_TESTS)
    add_subdirectory(tests)
endif()
