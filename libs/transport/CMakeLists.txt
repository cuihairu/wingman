cmake_minimum_required(VERSION 3.20)

# ========== wingman::transport ==========
# 分层传输架构

project(wingman_transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(asio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# 源文件
set(TRANSPORT_SOURCES
    src/session/session.cpp
    src/session/tcp_session.cpp
    src/channel/channel.cpp
    src/transport_server.cpp
    src/transport_client.cpp
    src/simple_protocol.cpp
    src/stream_channel.cpp
    src/stream_manager.cpp
)

# 创建库
add_library(wingman_transport STATIC
    ${TRANSPORT_SOURCES}
)

target_include_directories(wingman_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wingman_transport PUBLIC asio::asio)

target_link_libraries(wingman_transport PRIVATE spdlog::spdlog_header_only)

# Windows 特定
if(WIN32)
    target_link_libraries(wingman_transport PUBLIC
        ws2_32
    )
endif()

# 导出库
add_library(wingman::transport ALIAS wingman_transport)

# 编译选项
target_compile_features(wingman_transport PUBLIC cxx_std_23)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman_transport PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(wingman_transport PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ========== 测试 ==========
option(BUILD_TRANSPORT_TESTS "Build transport library tests" OFF)
if(BUILD_TRANSPORT_TESTS)
    add_subdirectory(tests)
endif()
