cmake_minimum_required(VERSION 3.20)

# ========== wingman::crash ==========
# Crashpad 客户端封装：零平台宏，路径推导交调用方（见 include/wingman/crash/crash.hpp）

project(wingman_crash LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(wingman_crash STATIC
    src/crash_client.cpp
)

target_include_directories(wingman_crash PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# crashpad_client 带 PUBLIC include（mini_chromium/crashpad 源根）与
# crashpad 侧链接，消费者（runtime）传递获得；WINGMAN_HAS_CRASHPAD 门
# 同样传递，未链接本库时调用点编译为空实现。
target_link_libraries(wingman_crash PUBLIC crashpad_client)
target_compile_definitions(wingman_crash PUBLIC WINGMAN_HAS_CRASHPAD)

add_library(wingman::crash ALIAS wingman_crash)

target_compile_features(wingman_crash PUBLIC cxx_std_23)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman_crash PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(wingman_crash PRIVATE -Wall -Wextra -Wpedantic)
endif()
