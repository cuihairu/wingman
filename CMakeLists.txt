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

# Find dependencies (Windows: vcpkg, Linux/macOS: system packages)
if(WIN32)
    find_package(lua CONFIG REQUIRED)
    find_package(opencv4 CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(asio CONFIG REQUIRED)
endif()

find_package(Threads REQUIRED)

# Core source files
set(CORE_SOURCES
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
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
)

# Add core sources if they exist
foreach(src ${CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        target_sources(wingman PRIVATE ${src})
    endif()
endforeach()

# Include directories
target_include_directories(wingman PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
)

# Windows specific libraries
if(WIN32)
    target_link_libraries(wingman PRIVATE
        Lua::Lua
        opencv4::opencv_core
        opencv4::opencv_imgproc
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        asio::asio
        Threads::Threads
        gdiplus
        user32
        gdi32
    )
else()
    target_link_libraries(wingman PRIVATE
        Threads::Threads
        X11
    )
endif()

# Server module (if enabled)
if(WINGMAN_BUILD_SERVER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/src/server.cpp")
        add_library(wingman-server STATIC ${SERVER_SOURCES})
        target_include_directories(wingman-server PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/server/include
        )
        if(WIN32)
            target_link_libraries(wingman-server PUBLIC asio::asio)
        endif()
        target_link_libraries(wingman PRIVATE wingman-server)
    endif()
endif()

# Debugger module (if enabled)
if(WINGMAN_BUILD_DEBUGGER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/debugger/src/debugger.cpp")
        add_library(wingman-debugger STATIC ${DEBUGGER_SOURCES})
        target_include_directories(wingman-debugger PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/debugger/include
        )
        target_link_libraries(wingman PRIVATE wingman-debugger)
    endif()
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
