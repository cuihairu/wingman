cmake_minimum_required(VERSION 3.20)

project(WingmanMinimalTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

include_directories(../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Only tests that don't require external dependencies
add_executable(minimal_tests
    ../cpp/version_test.cpp
    ../cpp/json_test.cpp
    ../cpp/node_status_test.cpp
)

target_sources(minimal_tests PRIVATE
    ../../src/json.cpp
    ../../src/node_status.cpp
)

target_link_libraries(minimal_tests PRIVATE
    gtest
    gtest_main
)
