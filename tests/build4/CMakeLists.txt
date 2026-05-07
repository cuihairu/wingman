cmake_minimum_required(VERSION 3.20)

project(WingmanMinimalTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN)

# Use local googletest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

# Include wingman headers and nlohmann
include_directories(../../include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create test executable
add_executable(minimal_tests
    ../cpp/version_test.cpp
    ../cpp/json_test.cpp
    ../cpp/kvstore_test.cpp
)

# Link implementations
target_sources(minimal_tests PRIVATE
    ../../src/json.cpp
    ../../src/kvstore.cpp
)

target_link_libraries(minimal_tests PRIVATE
    gtest
    gtest_main
)
