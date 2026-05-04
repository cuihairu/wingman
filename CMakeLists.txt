cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg integration
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Options
option(WINGMAN_BUILD_SERVER "Build network server" ON)
option(WINGMAN_BUILD_DEBUGGER "Build debugger" ON)
option(WINGMAN_BUILD_TESTS "Build tests" OFF)

# Find dependencies
find_package(lua CONFIG REQUIRED)
find_package(opencv4 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Source files
set(CORE_SOURCES
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
    src/human.cpp
    src/macro.cpp
    src/trigger.cpp
    src/utils.cpp
)

set(SERVER_SOURCES
    server/src/server.cpp
    server/src/client.cpp
    server/src/protocol.cpp
)

set(DEBUGGER_SOURCES
    debugger/src/debugger.cpp
    debugger/src/breakpoint.cpp
    debugger/src/protocol.cpp
)

# Core executable
add_executable(wingman
    src/main.cpp
    ${CORE_SOURCES}
)

target_link_libraries(wingman PRIVATE
    Lua::Lua
    opencv4::opencv_core
    opencv4::opencv_imgproc
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    asio::asio
    Threads::Threads
)

# Include directories
target_include_directories(wingman PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
)

# Server module (if enabled)
if(WINGMAN_BUILD_SERVER)
    add_library(wingman-server STATIC ${SERVER_SOURCES})
    target_link_libraries(wingman-server PUBLIC asio::asio)
    target_include_directories(wingman-server PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    )
    target_link_libraries(wingman PRIVATE wingman-server)
endif()

# Debugger module (if enabled)
if(WINGMAN_BUILD_DEBUGGER)
    add_library(wingman-debugger STATIC ${DEBUGGER_SOURCES})
    target_include_directories(wingman-debugger PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/debugger/include
    )
    target_link_libraries(wingman PRIVATE wingman-debugger)
endif()

# Installation
install(TARGETS wingman
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Copy scripts
install(DIRECTORY scripts/
    DESTINATION scripts
    FILES_MATCHING PATTERN "*.lua"
)

# Documentation
find_package(Doxxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_MAN NO)
    set(DOXYGEN_OUTPUT_DIRECTORY docs/api)
    doxygen_add_docs(docs_doxygen ALL)
endif()
