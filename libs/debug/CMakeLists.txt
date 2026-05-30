cmake_minimum_required(VERSION 3.20)

# ========== wingman::debug ==========
# EmmyLua Debugger 适配层

project(wingman_debug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖
find_package(Lua REQUIRED)

# 调试器源文件
set(DEBUG_SOURCES
    src/emmy_adapter.cpp
)

# 创建库
add_library(wingman_debug STATIC ${DEBUG_SOURCES})

target_include_directories(wingman_debug PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LUA_INCLUDE_DIR}
)

target_link_libraries(wingman_debug PUBLIC
    ${LUA_LIBRARIES}
)

# EmmyLua Debugger 路径配置
# EmmyLua 将在运行时从以下位置加载：
# 1. 可执行文件目录/emmy/
# 2. WINGMAN_EMMY_PATH 环境变量
# 3. 编译时指定的路径

target_compile_definitions(wingman_debug PUBLIC
    WINGMAN_EMMY_VERSION="0.5.3"
)

# 导出库
add_library(wingman::debug ALIAS wingman_debug)

# ========== 复制 EmmyLua 文件到输出目录 ==========

# EmmyLua 文件应该在构建时复制到输出目录
# 用户需要手动下载 EmmyLua 或通过 FetchContent 获取

# ========== 测试 ==========
option(BUILD_DEBUG_TESTS "Build debug library tests" OFF)
if(BUILD_DEBUG_TESTS)
    add_subdirectory(tests)
endif()
