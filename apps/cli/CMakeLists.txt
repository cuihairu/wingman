cmake_minimum_required(VERSION 3.20)

# ========== Wingman CLI ==========
# 命令行工具

project(wingman_cli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CLI 源文件
add_executable(wingman-cli
    wingman-cli.cpp
)

# 链接库
target_link_libraries(wingman-cli PRIVATE
    spdlog::spdlog_header_only
)

# Windows 特定
if(WIN32)
    target_link_libraries(wingman-cli PRIVATE
        ws2_32
    )
endif()

# 复制到输出目录
set_target_properties(wingman-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
