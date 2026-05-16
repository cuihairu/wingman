cmake_minimum_required(VERSION 3.20)

# ========== wingman::transport ==========
# 分层传输架构

project(wingman_transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 asio（可选）
find_package(asio CONFIG QUIET)

if(NOT asio_FOUND)
    # 使用 FetchContent 下载 asio（header-only）
    message(STATUS "asio not found via find_package, using FetchContent")
    include(FetchContent)

    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG        asio-1-28-2
    )

    # asio 是 header-only 库，不需要构建
    FetchContent_MakeAvailable(asio)
else()
    message(STATUS "asio found via CONFIG")
endif()

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

# 链接 asio（如果通过 CONFIG 找到）
if(TARGET asio::asio)
    target_link_libraries(wingman_transport PUBLIC asio::asio)
endif()

# Windows 特定
if(WIN32)
    target_link_libraries(wingman_transport PUBLIC
        ws2_32
    )
endif()

# 导出库
add_library(wingman::transport ALIAS wingman_transport)

# 编译选项
target_compile_features(wingman_transport PUBLIC cxx_std_20)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(wingman_transport PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(wingman_transport PRIVATE -Wall -Wextra -Wpedantic)
endif()

# FetchContent 路径修复
if(asio_SOURCE_DIR)
    target_include_directories(wingman_transport PRIVATE ${asio_SOURCE_DIR}/asio/include)
endif()

# ========== 测试 ==========
option(BUILD_TRANSPORT_TESTS "Build transport library tests" OFF)
if(BUILD_TRANSPORT_TESTS)
    add_subdirectory(tests)
endif()
