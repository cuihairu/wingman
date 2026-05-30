cmake_minimum_required(VERSION 3.20)

# Wingman 性能基准测试

project(wingman_benchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 基准测试可执行文件
add_executable(kvstore_bench
    kvstore_bench.cpp
)

target_include_directories(kvstore_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/wingman/include
)

target_link_libraries(kvstore_bench PRIVATE
    wingman
)

# Windows 特定
if(WIN32)
    target_link_libraries(kvstore_bench PRIVATE
        ws2_32
    )
endif()
