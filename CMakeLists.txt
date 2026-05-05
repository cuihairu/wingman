cmake_minimum_required(VERSION 3.20)
project(Wingman VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add UTF-8 compiler flag
add_compile_options(/utf-8)

# vcpkg integration
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Options
option(WINGMAN_BUILD_SERVER "Build network server" ON)
option(WINGMAN_BUILD_DEBUGGER "Build debugger" ON)
option(WINGMAN_BUILD_TESTS "Build tests" OFF)

# Find dependencies via vcpkg
find_package(Lua REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Testing framework
if(WINGMAN_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()

# Core source files
set(CORE_SOURCES
    src/screen.cpp
    src/input.cpp
    src/window.cpp
    src/process.cpp
    src/trigger.cpp
    src/recorder.cpp
    src/http.cpp
    src/json.cpp
    # src/kvstore.cpp  # TODO: Re-enable when sqlite3 is properly configured
    src/system.cpp
    src/performance.cpp
    # src/script_manager.cpp  # TODO: Re-enable after fixing Lua compatibility issues
    # src/security.cpp  # TODO: Re-enable after fixing WINTRUST issues
    src/game_profile.cpp
    src/storage.cpp
)

# Lua bindings
set(LUA_SOURCES
    bindings/lua_bindings.cpp
    bindings/lua_extensions.cpp
)

set(SERVER_SOURCES
    server/src/server.cpp
    server/src/client.cpp
    server/src/protocol.cpp
    server/src/orchestrator.cpp
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

# Add Lua bindings
foreach(src ${LUA_SOURCES})
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
target_link_libraries(wingman PRIVATE
    ${LUA_LIBRARIES}
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    asio::asio
    CURL::libcurl
    Threads::Threads
    gdiplus
    user32
    gdi32
)

# Server module (if enabled)
if(WINGMAN_BUILD_SERVER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/src/server.cpp")
        add_library(wingman-server STATIC ${SERVER_SOURCES} src/storage_remote.cpp)
        target_include_directories(wingman-server PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_link_libraries(wingman-server PUBLIC asio::asio nlohmann_json::nlohmann_json)
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

# Tests (if enabled)
if(WINGMAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
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
